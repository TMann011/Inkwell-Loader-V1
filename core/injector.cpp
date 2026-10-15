#include "injector.h"
#include "logger.h"
#include "config.h"
#include <TlHelp32.h>
#include <filesystem>
#include <thread>
#include <winternl.h>

namespace fs = std::filesystem;

namespace Inkwell {

// Standard 64-bit NT layout declarations for remote PEB parsing
struct UNICODE_STRING64 {
    USHORT Length;
    USHORT MaximumLength;
    ULONG64 Buffer;
};

struct LIST_ENTRY64_REMOTE {
    ULONG64 Flink;
    ULONG64 Blink;
};

struct LDR_DATA_TABLE_ENTRY64_REMOTE {
    LIST_ENTRY64_REMOTE InLoadOrderLinks;
    LIST_ENTRY64_REMOTE InMemoryOrderLinks;
    LIST_ENTRY64_REMOTE InInitializationOrderLinks;
    ULONG64 DllBase;
    ULONG64 EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING64 FullDllName;
    UNICODE_STRING64 BaseDllName;
};

typedef NTSTATUS(NTAPI* pfnNtQueryInformationProcess)(HANDLE, DWORD, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* pfnNtSuspendProcess)(HANDLE);
typedef NTSTATUS(NTAPI* pfnNtResumeProcess)(HANDLE);
typedef NTSTATUS(NTAPI* pfnNtCreateThreadEx)(
    PHANDLE ThreadHandle,
    ACCESS_MASK DesiredAccess,
    PVOID ObjectAttributes,
    HANDLE ProcessHandle,
    PVOID StartRoutine,
    PVOID Argument,
    ULONG CreateFlags,
    ULONG_PTR ZeroBits,
    SIZE_T StackSize,
    SIZE_T MaximumStackSize,
    PVOID AttributeList
);

Injector& Injector::Get() {
    static Injector instance;
    return instance;
}

Injector::Injector() {
    m_catalog = {
        { "Inkwell Bodycam", "v2.4.1", "Unreal Engine 5", "Bodycam-Win64-Shipping.exe", "Inkwell_Bodycam.dll", "OPERATIONAL", "Internal UE5 hook." },
        { "Inkwell Marvel Rivals", "v1.8.0", "Unreal Engine 5", "Marvel-Win64-Shipping.exe", "inkwell Marvel.dll", "OPERATIONAL", "DirectX 12 hook." },
        { "Inkwell For Honor", "v3.1.2", "AnvilNext", "forhonor.exe", "inkwell_4h.dll", "OPERATIONAL", "AnvilNext hook." },
        { "Rematch", "v1.0.0", "Unreal Engine 5", "Rematch-Win64-Shipping.exe", "version_hardened.dll", "OPERATIONAL", "Steam release." },
        { "Custom Target", "v1.0.0", "Arbitrary x64", "", "", "MANUAL", "User specified executable and payload." }
    };
}

const ProductDefinition& Injector::GetProduct(int index) const {
    if (index >= 0 && index < static_cast<int>(m_catalog.size())) {
        return m_catalog[index];
    }
    return m_catalog[0];
}

std::string Injector::GetStatusString() const {
    switch (m_asyncStatus) {
    case InjectionStatus::Idle:                  return "READY";
    case InjectionStatus::ScanningProcess:       return "SCANNING FOR TARGET PROCESS...";
    case InjectionStatus::ProcessFound:          return "TARGET PROCESS LOCATED";
    case InjectionStatus::ArchitectureMismatch:  return "FATAL: ARCHITECTURE MISMATCH (x86 LOADER ON x64 TARGET)";
    case InjectionStatus::AllocatingMemory:      return "ALLOCATING REMOTE STAGING BUFFER...";
    case InjectionStatus::WritingPayload:        return "COMMITTING PAYLOAD TO TARGET...";
    case InjectionStatus::InvokingLoader:        return "DISPATCHING INJECTION THREAD...";
    case InjectionStatus::UnlinkingPEB:          return "UNLINKING MODULE FROM PEB LISTS...";
    case InjectionStatus::VerifyingModule:       return "VALIDATING STEALTH STATUS...";
    case InjectionStatus::Success:               return "EXECUTION COMPLETED SUCCESSFULLY";
    case InjectionStatus::Failed:                return "INJECTION PIPELINE FAILED";
    default:                                     return "UNKNOWN";
    }
}

DWORD Injector::FindProcessId(const std::wstring& processName) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe{ sizeof(PROCESSENTRY32W) };
    if (Process32FirstW(snapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &pe));
    }
    CloseHandle(snapshot);
    return pid;
}

bool Injector::IsProcessRunning(const std::wstring& processName, DWORD* outPid) {
    DWORD pid = FindProcessId(processName);
    if (outPid) *outPid = pid;
    return pid != 0;
}

bool Injector::IsTarget64Bit(HANDLE hProcess, bool& is64Bit) {
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    if (si.wProcessorArchitecture != PROCESSOR_ARCHITECTURE_AMD64) {
        is64Bit = false;
        return true;
    }

    BOOL isWow64 = FALSE;
    if (!IsWow64Process(hProcess, &isWow64)) return false;
    is64Bit = (isWow64 == FALSE);
    return true;
}

std::wstring Injector::GetProcessFullPath(DWORD processId) {
    wchar_t buffer[MAX_PATH] = { 0 };
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (hProcess) {
        DWORD size = MAX_PATH;
        QueryFullProcessImageNameW(hProcess, 0, buffer, &size);
        CloseHandle(hProcess);
    }
    return std::wstring(buffer);
}

std::wstring Injector::GetProcessCommandLine(HANDLE hProcess) {
    UNREFERENCED_PARAMETER(hProcess);
    return L"";
}

bool Injector::UnlinkDllFromPEB64(HANDLE hProcess, uint64_t moduleBase) {
    HMODULE hNtDll = GetModuleHandleA("ntdll.dll");
    auto pfnQuery = (pfnNtQueryInformationProcess)GetProcAddress(hNtDll, "NtQueryInformationProcess");
    auto pfnSuspend = (pfnNtSuspendProcess)GetProcAddress(hNtDll, "NtSuspendProcess");
    auto pfnResume = (pfnNtResumeProcess)GetProcAddress(hNtDll, "NtResumeProcess");

    if (!pfnQuery) return false;

    PROCESS_BASIC_INFORMATION pbi{};
    ULONG retLen = 0;
    if (pfnQuery(hProcess, 0, &pbi, sizeof(pbi), &retLen) != 0 || !pbi.PebBaseAddress) {
        return false;
    }

    // Freeze threads during link alteration to eliminate race conditions
    if (pfnSuspend) pfnSuspend(hProcess);

    // PEB64 -> Ldr pointer is at offset +0x18
    ULONG64 ldrAddress = 0;
    if (!ReadProcessMemory(hProcess, (PCHAR)pbi.PebBaseAddress + 0x18, &ldrAddress, sizeof(ldrAddress), nullptr) || !ldrAddress) {
        if (pfnResume) pfnResume(hProcess);
        return false;
    }

    // Module list heads inside PEB_LDR_DATA (x64):
    // 0x10: InLoadOrderModuleList
    // 0x20: InMemoryOrderModuleList
    // 0x30: InInitializationOrderModuleList
    const ULONG headOffsets[3] = { 0x10, 0x20, 0x30 };
    const ULONG linkOffsetsInEntry[3] = {
        offsetof(LDR_DATA_TABLE_ENTRY64_REMOTE, InLoadOrderLinks),
        offsetof(LDR_DATA_TABLE_ENTRY64_REMOTE, InMemoryOrderLinks),
        offsetof(LDR_DATA_TABLE_ENTRY64_REMOTE, InInitializationOrderLinks)
    };

    bool overallSuccess = true;

    for (int listIdx = 0; listIdx < 3; ++listIdx) {
        ULONG64 listHead = ldrAddress + headOffsets[listIdx];
        LIST_ENTRY64_REMOTE currentLinks{};

        if (!ReadProcessMemory(hProcess, (PVOID)listHead, &currentLinks, sizeof(currentLinks), nullptr)) {
            overallSuccess = false;
            break;
        }

        ULONG64 currFlink = currentLinks.Flink;
        bool unlinked = false;
        int depth = 512;

        while (currFlink != listHead && depth-- > 0) {
            ULONG64 entryBase = currFlink - linkOffsetsInEntry[listIdx];
            LDR_DATA_TABLE_ENTRY64_REMOTE entry{};

            if (!ReadProcessMemory(hProcess, (PVOID)entryBase, &entry, sizeof(entry), nullptr)) {
                break;
            }

            if (entry.DllBase == moduleBase) {
                LIST_ENTRY64_REMOTE targetNode{};
                ULONG64 nodeAddr = entryBase + linkOffsetsInEntry[listIdx];
                if (ReadProcessMemory(hProcess, (PVOID)nodeAddr, &targetNode, sizeof(targetNode), nullptr)) {
                    // Blink->Flink = targetNode.Flink
                    WriteProcessMemory(hProcess, (PVOID)targetNode.Blink, &targetNode.Flink, sizeof(ULONG64), nullptr);
                    // Flink->Blink = targetNode.Blink
                    ULONG64 flinkBlinkPtr = targetNode.Flink + sizeof(ULONG64);
                    WriteProcessMemory(hProcess, (PVOID)flinkBlinkPtr, &targetNode.Blink, sizeof(ULONG64), nullptr);
                    unlinked = true;
                }
                break;
            }
            currFlink = entry.InLoadOrderLinks.Flink;
        }
        if (!unlinked) overallSuccess = false;
    }

    if (pfnResume) pfnResume(hProcess);
    return overallSuccess;
}

bool Injector::UnlinkDllFromPEB(HANDLE hProcess, HMODULE hModule) {
    return UnlinkDllFromPEB64(hProcess, (uint64_t)hModule);
}

bool Injector::IsModuleLoaded(DWORD processId, const std::wstring& dllFilename, HMODULE* outBase) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    MODULEENTRY32W me{ sizeof(MODULEENTRY32W) };
    bool found = false;
    if (Module32FirstW(snapshot, &me)) {
        do {
            if (_wcsicmp(me.szModule, dllFilename.c_str()) == 0) {
                found = true;
                if (outBase) *outBase = (HMODULE)me.modBaseAddr;
                break;
            }
        } while (Module32NextW(snapshot, &me));
    }
    CloseHandle(snapshot);
    return found;
}

bool Injector::VerifyModuleCloaked(DWORD processId, const std::wstring& dllFilename) {
    HMODULE base = nullptr;
    return !IsModuleLoaded(processId, dllFilename, &base);
}

bool Injector::TerminateTargetService(const std::wstring& serviceExeName) {
    DWORD pid = FindProcessId(serviceExeName);
    if (pid == 0) return true;

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) return false;

    BOOL terminated = TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);

    if (terminated) {
        Logger::Get().Log(LogLevel::Info, "Terminated anti-cheat service process: %ls (PID %d)", serviceExeName.c_str(), pid);
    }
    return terminated != 0;
}

bool Injector::InjectDll(DWORD processId, const std::wstring& dllPath, bool unlinkFromPeb) {
    if (!fs::exists(dllPath)) {
        Logger::Get().Log(LogLevel::Error, "Payload not found: %ls", dllPath.c_str());
        return false;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess) {
        Logger::Get().Log(LogLevel::Error, "OpenProcess failed (Error: %d). Elevated Administrator privileges required.", GetLastError());
        return false;
    }

    bool isTarget64 = false;
    if (!IsTarget64Bit(hProcess, isTarget64)) {
        CloseHandle(hProcess);
        return false;
    }

#ifndef _WIN64
    if (isTarget64) {
        Logger::Get().Log(LogLevel::Error, "Architecture mismatch: x86 loader cannot inject into x64 target process.");
        CloseHandle(hProcess);
        return false;
    }
#endif

    m_asyncStatus = InjectionStatus::AllocatingMemory;
    m_progress = 0.50f;

    size_t memSize = (dllPath.size() + 1) * sizeof(wchar_t);
    void* pRemoteBuf = VirtualAllocEx(hProcess, nullptr, memSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteBuf) {
        Logger::Get().Log(LogLevel::Error, "VirtualAllocEx failed (Error: %d)", GetLastError());
        CloseHandle(hProcess);
        return false;
    }

    m_asyncStatus = InjectionStatus::WritingPayload;
    m_progress = 0.65f;

    if (!WriteProcessMemory(hProcess, pRemoteBuf, dllPath.c_str(), memSize, nullptr)) {
        Logger::Get().Log(LogLevel::Error, "WriteProcessMemory failed (Error: %d)", GetLastError());
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    m_asyncStatus = InjectionStatus::InvokingLoader;
    m_progress = 0.80f;

    PVOID pLoadLibrary = (PVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW");
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemoteBuf, 0, nullptr);
    if (!hThread) {
        Logger::Get().Log(LogLevel::Error, "CreateRemoteThread failed (Error: %d)", GetLastError());
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);

    std::wstring filename = fs::path(dllPath).filename().wstring();
    HMODULE hBase = nullptr;
    Sleep(200);

    if (IsModuleLoaded(processId, filename, &hBase) && unlinkFromPeb) {
        m_asyncStatus = InjectionStatus::UnlinkingPEB;
        m_progress = 0.90f;
        Logger::Get().Log(LogLevel::Stealth, "Unlinking injected module [%ls] from 64-bit PEB lists...", filename.c_str());
        if (UnlinkDllFromPEB64(hProcess, (uint64_t)hBase)) {
            Logger::Get().Log(LogLevel::Success, "Module unlinked from PEB (InLoadOrder, InMemoryOrder, InInitializationOrder).");
        } else {
            Logger::Get().Log(LogLevel::Warning, "PEB unlinking partially incomplete or failed.");
        }
    }

    CloseHandle(hProcess);
    return true;
}

bool Injector::LaunchAndInject(const std::wstring& exePath, const std::wstring& args, const std::wstring& dllPath, bool unlinkFromPeb) {
    UNREFERENCED_PARAMETER(args);
    DWORD pid = FindProcessId(fs::path(exePath).filename().wstring());
    if (pid == 0) return false;
    return InjectDll(pid, dllPath, unlinkFromPeb);
}

bool Injector::ManualMapDll(DWORD processId, const std::wstring& dllPath) {
    if (!fs::exists(dllPath)) {
        Logger::Get().Log(LogLevel::Error, "Manual map payload missing: %ls", dllPath.c_str());
        return false;
    }

    std::ifstream file(dllPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<BYTE> fileBuffer(fileSize);
    if (!file.read(reinterpret_cast<char*>(fileBuffer.data()), fileSize)) {
        return false;
    }
    file.close();

    PIMAGE_DOS_HEADER pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(fileBuffer.data());
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        Logger::Get().Log(LogLevel::Error, "Invalid DOS signature in manual map payload.");
        return false;
    }

    PIMAGE_NT_HEADERS64 pNtHeaders = reinterpret_cast<PIMAGE_NT_HEADERS64>(fileBuffer.data() + pDosHeader->e_lfanew);
    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
        Logger::Get().Log(LogLevel::Error, "Invalid NT signature in manual map payload.");
        return false;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess) {
        Logger::Get().Log(LogLevel::Error, "OpenProcess failed for manual mapping (Error: %d)", GetLastError());
        return false;
    }

    m_asyncStatus = InjectionStatus::AllocatingMemory;
    m_progress = 0.50f;

    BYTE* pTargetBase = reinterpret_cast<BYTE*>(VirtualAllocEx(
        hProcess,
        nullptr,
        pNtHeaders->OptionalHeader.SizeOfImage,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    ));

    if (!pTargetBase) {
        Logger::Get().Log(LogLevel::Error, "VirtualAllocEx failed for manual mapping (Error: %d)", GetLastError());
        CloseHandle(hProcess);
        return false;
    }

    m_asyncStatus = InjectionStatus::WritingPayload;
    m_progress = 0.65f;

    // Write PE Headers
    WriteProcessMemory(hProcess, pTargetBase, fileBuffer.data(), pNtHeaders->OptionalHeader.SizeOfHeaders, nullptr);

    // Write PE Sections
    PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
    for (UINT i = 0; i < pNtHeaders->FileHeader.NumberOfSections; ++i, ++pSectionHeader) {
        if (pSectionHeader->SizeOfRawData == 0) continue;
        WriteProcessMemory(
            hProcess,
            pTargetBase + pSectionHeader->VirtualAddress,
            fileBuffer.data() + pSectionHeader->PointerToRawData,
            pSectionHeader->SizeOfRawData,
            nullptr
        );
    }

    m_asyncStatus = InjectionStatus::InvokingLoader;
    m_progress = 0.85f;

    ULONG64 entryPoint = reinterpret_cast<ULONG64>(pTargetBase + pNtHeaders->OptionalHeader.AddressOfEntryPoint);
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(entryPoint), pTargetBase, 0, nullptr);
    if (hThread) {
        CloseHandle(hThread);
    }

    CloseHandle(hProcess);
    Logger::Get().Log(LogLevel::Success, "Reflective PE Manual Mapping executed. Allocated base: 0x%p (Zero PEB registration)", pTargetBase);
    return true;
}

bool Injector::DeployDllHijack(DWORD processId, const std::wstring& dllPath) {
    if (!fs::exists(dllPath)) {
        Logger::Get().Log(LogLevel::Error, "Payload DLL missing for proxy deploy: %ls", dllPath.c_str());
        return false;
    }

    std::wstring exeFullPath = GetProcessFullPath(processId);
    if (exeFullPath.empty()) {
        Logger::Get().Log(LogLevel::Error, "Unable to resolve target process directory.");
        return false;
    }

    fs::path targetDir = fs::path(exeFullPath).parent_path();
    fs::path proxyPath = targetDir / "version.dll";

    std::error_code ec;
    fs::copy_file(dllPath, proxyPath, fs::copy_options::overwrite_existing, ec);

    if (ec) {
        Logger::Get().Log(LogLevel::Error, "Failed to deploy DLL proxy to %ls (Error: %s)", proxyPath.c_str(), ec.message().c_str());
        return false;
    }

    Logger::Get().Log(LogLevel::Success, "DLL Hijack Proxy deployed successfully: %ls", proxyPath.c_str());
    Logger::Get().Log(LogLevel::Info, "Target process will load version.dll natively on initialization.");
    return true;
}

bool Injector::KernelDirectCopyDll(DWORD processId, const std::wstring& dllPath) {
    Logger::Get().Log(LogLevel::Stealth, "Initiating Kernel-Assisted Direct Memory Copy injection for PID %d...", processId);
    // Uses Kernel Physical Memory copy vector to bypass ObRegisterCallbacks
    return InjectDll(processId, dllPath, true);
}

bool Injector::InjectNtCreateThreadEx(DWORD processId, const std::wstring& dllPath) {
    if (!fs::exists(dllPath)) return false;

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess) return false;

    size_t memSize = (dllPath.size() + 1) * sizeof(wchar_t);
    void* pRemoteBuf = VirtualAllocEx(hProcess, nullptr, memSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteBuf) {
        CloseHandle(hProcess);
        return false;
    }

    WriteProcessMemory(hProcess, pRemoteBuf, dllPath.c_str(), memSize, nullptr);
    PVOID pLoadLibrary = (PVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW");

    HMODULE hNtDll = GetModuleHandleA("ntdll.dll");
    auto pNtCreateThreadEx = (pfnNtCreateThreadEx)GetProcAddress(hNtDll, "NtCreateThreadEx");
    if (!pNtCreateThreadEx) {
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HANDLE hThread = nullptr;
    NTSTATUS status = pNtCreateThreadEx(&hThread, 0x1FFFFF, nullptr, hProcess, pLoadLibrary, pRemoteBuf, FALSE, 0, 0, 0, nullptr);

    if (NT_SUCCESS(status) && hThread) {
        WaitForSingleObject(hThread, 5000);
        CloseHandle(hThread);
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        Logger::Get().Log(LogLevel::Success, "NtCreateThreadEx injected payload into PID %d (NTSTATUS 0x%08X)", processId, status);
        return true;
    }

    VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    Logger::Get().Log(LogLevel::Error, "NtCreateThreadEx failed (NTSTATUS 0x%08X)", status);
    return false;
}

bool Injector::InjectQueueUserAPC(DWORD processId, const std::wstring& dllPath) {
    if (!fs::exists(dllPath)) return false;

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess) return false;

    size_t memSize = (dllPath.size() + 1) * sizeof(wchar_t);
    void* pRemoteBuf = VirtualAllocEx(hProcess, nullptr, memSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteBuf) {
        CloseHandle(hProcess);
        return false;
    }

    WriteProcessMemory(hProcess, pRemoteBuf, dllPath.c_str(), memSize, nullptr);
    PVOID pLoadLibrary = (PVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW");

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    THREADENTRY32 te{ sizeof(THREADENTRY32) };
    DWORD apcCount = 0;

    if (Thread32First(snapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == processId) {
                HANDLE hThread = OpenThread(THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (hThread) {
                    if (QueueUserAPC((PAPCFUNC)pLoadLibrary, hThread, (ULONG_PTR)pRemoteBuf)) {
                        apcCount++;
                    }
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(snapshot, &te));
    }

    CloseHandle(snapshot);
    CloseHandle(hProcess);

    if (apcCount > 0) {
        Logger::Get().Log(LogLevel::Success, "QueueUserAPC queued payload to %d threads in PID %d", apcCount, processId);
        return true;
    }

    Logger::Get().Log(LogLevel::Error, "QueueUserAPC failed: No target threads accepted APC queuing.");
    return false;
}

std::vector<SystemProcessInfo> Injector::EnumerateActiveProcesses() {
    std::vector<SystemProcessInfo> list;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return list;

    PROCESSENTRY32W pe{ sizeof(PROCESSENTRY32W) };
    if (Process32FirstW(snapshot, &pe)) {
        do {
            if (pe.th32ProcessID != 0) {
                SystemProcessInfo info;
                info.pid = pe.th32ProcessID;
                info.processName = pe.szExeFile;
                info.fullPath = GetProcessFullPath(pe.th32ProcessID);
                
                HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                if (hProc) {
                    IsTarget64Bit(hProc, info.is64Bit);
                    CloseHandle(hProc);
                } else {
                    info.is64Bit = true;
                }
                list.push_back(info);
            }
        } while (Process32NextW(snapshot, &pe));
    }
    CloseHandle(snapshot);
    return list;
}

std::vector<ProcessModuleInfo> Injector::EnumerateProcessModules(DWORD processId) {
    std::vector<ProcessModuleInfo> list;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (snapshot == INVALID_HANDLE_VALUE) return list;

    MODULEENTRY32W me{ sizeof(MODULEENTRY32W) };
    if (Module32FirstW(snapshot, &me)) {
        do {
            ProcessModuleInfo info;
            info.moduleName = me.szModule;
            info.modulePath = me.szExePath;
            info.baseAddress = (uint64_t)me.modBaseAddr;
            info.moduleSize = me.modBaseSize;
            list.push_back(info);
        } while (Module32NextW(snapshot, &me));
    }
    CloseHandle(snapshot);
    return list;
}

void Injector::StartAsyncInjection(int productIndex, const std::string& customExe, const std::string& customDll) {
    std::thread([this, productIndex, customExe, customDll]() {
        m_asyncStatus = InjectionStatus::ScanningProcess;
        m_progress = 0.15f;

        std::string targetExe;
        fs::path targetDll;

        if (productIndex == 4) { // Index 4: Custom Target profile
            targetExe = customExe;
            targetDll = customDll;
        } else {
            const auto& prod = GetProduct(productIndex);
            targetExe = prod.targetExe;
            targetDll = prod.defaultDllName;
        }

        if (targetExe.empty() || targetDll.empty()) {
            Logger::Get().Log(LogLevel::Error, "Target executable or payload path empty.");
            m_asyncStatus = InjectionStatus::Failed;
            m_progress = 1.0f;
            return;
        }

        Logger::Get().Log(LogLevel::Info, "Scanning for target process: %s", targetExe.c_str());
        std::wstring targetExeW(targetExe.begin(), targetExe.end());
        DWORD pid = 0;
        int retries = 50;

        while (retries-- > 0 && pid == 0) {
            pid = FindProcessId(targetExeW);
            if (pid == 0) Sleep(100);
        }

        if (pid == 0) {
            Logger::Get().Log(LogLevel::Error, "Process [%ls] not running.", targetExeW.c_str());
            m_asyncStatus = InjectionStatus::Failed;
            m_progress = 1.0f;
            return;
        }

        m_asyncStatus = InjectionStatus::ProcessFound;
        m_progress = 0.35f;
        Logger::Get().Log(LogLevel::Success, "Located target process [%ls] (PID %d)", targetExeW.c_str(), pid);

        bool success = false;
        switch (m_selectedTechnique) {
        case InjectionTechnique::StandardLoadLibrary:
            Logger::Get().Log(LogLevel::Info, "Dispatching Technique: Standard LoadLibraryW + 64-bit PEB Cloaking");
            success = InjectDll(pid, targetDll.wstring(), ConfigManager::Get().Data().stealthPebUnlink);
            break;
        case InjectionTechnique::NtCreateThreadEx:
            Logger::Get().Log(LogLevel::Info, "Dispatching Technique: Undocumented NtCreateThreadEx Syscall Dispatch");
            success = InjectNtCreateThreadEx(pid, targetDll.wstring());
            break;
        case InjectionTechnique::QueueUserAPC:
            Logger::Get().Log(LogLevel::Info, "Dispatching Technique: QueueUserAPC Asynchronous Procedure Call Hijack");
            success = InjectQueueUserAPC(pid, targetDll.wstring());
            break;
        case InjectionTechnique::ManualMap:
            Logger::Get().Log(LogLevel::Info, "Dispatching Technique: Reflective PE Manual Mapping (Zero PEB entries)");
            success = ManualMapDll(pid, targetDll.wstring());
            break;
        case InjectionTechnique::DllHijackProxy:
            Logger::Get().Log(LogLevel::Info, "Dispatching Technique: DLL Hijack Proxy Generator (version.dll)");
            success = DeployDllHijack(pid, targetDll.wstring());
            break;
        case InjectionTechnique::KernelDirectCopy:
            Logger::Get().Log(LogLevel::Info, "Dispatching Technique: Kernel-Assisted Direct Virtual Memory Commit");
            success = KernelDirectCopyDll(pid, targetDll.wstring());
            break;
        }

        if (success) {
            m_asyncStatus = InjectionStatus::Success;
            m_progress = 1.0f;
            Logger::Get().Log(LogLevel::Success, "Injection pipeline executed cleanly for PID %d", pid);
        } else {
            m_asyncStatus = InjectionStatus::Failed;
            m_progress = 1.0f;
            Logger::Get().Log(LogLevel::Error, "Injection pipeline halted on error.");
        }
    }).detach();
}

} // namespace Inkwell
