#pragma once
#include <string>
#include <vector>
#include <Windows.h>

namespace Inkwell {

struct ProductDefinition {
    std::string name;
    std::string version;
    std::string gameEngine;
    std::string targetExe;
    std::string defaultDllName;
    std::string statusText;
    std::string description;
};

enum class InjectionTechnique {
    StandardLoadLibrary = 0, // LoadLibraryW + 64-bit PEB Unlink
    NtCreateThreadEx = 1,    // Undocumented NTAPI Kernel Object Dispatch (ntdll!NtCreateThreadEx)
    QueueUserAPC = 2,        // Asynchronous Procedure Call Thread Hijacking (QueueUserAPC)
    ManualMap = 3,           // Reflective PE Manual Mapper (Zero PEB footprint)
    DllHijackProxy = 4,      // DLL Proxy / Hijack Generator (version.dll proxy)
    KernelDirectCopy = 5     // Kernel-assisted Direct Virtual Memory Copy
};

enum class InjectionStatus {
    Idle,
    ScanningProcess,
    ProcessFound,
    ArchitectureMismatch,
    AllocatingMemory,
    WritingPayload,
    InvokingLoader,
    UnlinkingPEB,
    VerifyingModule,
    Success,
    Failed
};

struct ProcessModuleInfo {
    std::wstring moduleName;
    std::wstring modulePath;
    uint64_t baseAddress;
    uint32_t moduleSize;
};

struct SystemProcessInfo {
    DWORD pid;
    std::wstring processName;
    std::wstring fullPath;
    bool is64Bit;
};

class Injector {
public:
    static Injector& Get();

    // Process discovery primitives
    DWORD FindProcessId(const std::wstring& processName);
    bool IsProcessRunning(const std::wstring& processName, DWORD* outPid = nullptr);
    bool IsTarget64Bit(HANDLE hProcess, bool& is64Bit);
    std::wstring GetProcessFullPath(DWORD processId);
    std::wstring GetProcessCommandLine(HANDLE hProcess);
    std::vector<SystemProcessInfo> EnumerateActiveProcesses();
    std::vector<ProcessModuleInfo> EnumerateProcessModules(DWORD processId);

    // Injection technique configuration
    void SetTechnique(InjectionTechnique tech) { m_selectedTechnique = tech; }
    InjectionTechnique GetTechnique() const { return m_selectedTechnique; }

    // Core injection primitives across modes
    bool InjectDll(DWORD processId, const std::wstring& dllPath, bool unlinkFromPeb = true);
    bool InjectNtCreateThreadEx(DWORD processId, const std::wstring& dllPath);
    bool InjectQueueUserAPC(DWORD processId, const std::wstring& dllPath);
    bool ManualMapDll(DWORD processId, const std::wstring& dllPath);
    bool DeployDllHijack(DWORD processId, const std::wstring& dllPath);
    bool KernelDirectCopyDll(DWORD processId, const std::wstring& dllPath);
    bool LaunchAndInject(const std::wstring& exePath, const std::wstring& args, const std::wstring& dllPath, bool unlinkFromPeb = true);

    // 64-bit Remote PEB Unlinking primitive
    bool UnlinkDllFromPEB64(HANDLE hProcess, uint64_t moduleBase);
    bool UnlinkDllFromPEB(HANDLE hProcess, HMODULE hModule);

    // Module introspection and verification
    bool IsModuleLoaded(DWORD processId, const std::wstring& dllFilename, HMODULE* outBase = nullptr);
    bool VerifyModuleCloaked(DWORD processId, const std::wstring& dllFilename);

    // Anti-cheat / service suppression
    bool TerminateTargetService(const std::wstring& serviceExeName);

    // Catalog access
    const std::vector<ProductDefinition>& GetCatalog() const { return m_catalog; }
    const ProductDefinition& GetProduct(int index) const;

    // Asynchronous injection dispatch
    void StartAsyncInjection(int productIndex, const std::string& customExe = "", const std::string& customDll = "");
    InjectionStatus GetAsyncStatus() const { return m_asyncStatus; }
    std::string GetStatusString() const;
    float GetProgress() const { return m_progress; }

private:
    Injector();
    ~Injector() = default;

    std::vector<ProductDefinition> m_catalog;
    InjectionTechnique m_selectedTechnique = InjectionTechnique::StandardLoadLibrary;
    InjectionStatus m_asyncStatus = InjectionStatus::Idle;
    float m_progress = 0.0f;
    std::wstring m_lastErrorMsg;
};

} // namespace Inkwell
