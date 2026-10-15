#include "driver.h"
#include "logger.h"
#include "injector.h"
#include "kdmapper/kdmapper.hpp"
#include "kdmapper/intel_driver.hpp"
#include "kdmapper/utils.hpp"
#include <filesystem>
#include <fstream>
#include <winsvc.h>

namespace fs = std::filesystem;

namespace Inkwell {

DriverManager& DriverManager::Get() {
    static DriverManager instance;
    return instance;
}

DriverManager::DriverManager() {
    m_info.serviceName = L"InkwellKernelBypass";
    m_info.status = DriverStatus::Unloaded;
    m_info.activeCategory = AntiCheatCategory::BE;

    // Resolve paths to Stage 1 and Stage 2 drivers in working directory or local drivers subfolder
    fs::path exeDir = fs::current_path();
    if (fs::exists(exeDir / "drivers/iqvw64e.sys")) {
        m_info.stage1Path = (exeDir / "drivers/iqvw64e.sys").wstring();
    } else if (fs::exists(exeDir / "iqvw64e.sys")) {
        m_info.stage1Path = (exeDir / "iqvw64e.sys").wstring();
    } else {
        m_info.stage1Path = L"iqvw64e.sys";
    }

    if (fs::exists(exeDir / "drivers/stage2_payload_1.5MB.sys")) {
        m_info.stage2Path = (exeDir / "drivers/stage2_payload_1.5MB.sys").wstring();
    } else if (fs::exists(exeDir / "stage2_payload_1.5MB.sys")) {
        m_info.stage2Path = (exeDir / "stage2_payload_1.5MB.sys").wstring();
    } else {
        m_info.stage2Path = L"stage2_payload_1.5MB.sys";
    }

    // Populate Anti-Cheat Profile Definitions
    m_profiles = {
        {
            "Easy Anti-Cheat (EAC)",
            AntiCheatCategory::EAC,
            "EasyAntiCheat.exe / EasyAntiCheat_EOS.sys",
            "Targets EAC kernel module verification with Stage 1 BYOVD physical memory staging and dynamic PEB list unlinking.",
            true,  // terminateService
            true,  // stealthPebUnlink
            true   // manualMapKernelPool
        },
        {
            "BattEye (BE)",
            AntiCheatCategory::BE,
            "BEService.exe / BEDaisy.sys",
            "Targets BattEye process watcher with pre-injection service neutralization, suspended launch execution, and PEB unlinking.",
            true,
            true,
            true
        },
        {
            "Anti-Cheat Expert (ACE)",
            AntiCheatCategory::ACE,
            "ACE-BASE.sys / AntiCheatExpert.exe",
            "Targets ACE kernel monitoring via un-backed kernel pool payload mapping (0xFFFF...) and physical memory IOCTL translation.",
            true,
            true,
            true
        },
        {
            "Other / Custom Kernel Target",
            AntiCheatCategory::Other,
            "Generic / Custom Protection",
            "Configurable BYOVD kernel staging driver loader with manual PE image mapping into non-paged kernel pool memory.",
            false,
            true,
            true
        }
    };
}

const AntiCheatProfile& DriverManager::GetProfile(AntiCheatCategory category) const {
    size_t idx = static_cast<size_t>(category);
    if (idx < m_profiles.size()) {
        return m_profiles[idx];
    }
    return m_profiles[0];
}

bool DriverManager::InstallAndStartService(const std::wstring& driverPath, const std::wstring& serviceName) {
    if (!fs::exists(driverPath)) {
        Logger::Get().Log(LogLevel::Error, "Kernel driver file missing: %ls", driverPath.c_str());
        return false;
    }

    SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM) {
        Logger::Get().Log(LogLevel::Error, "OpenSCManagerW failed (Error: %d). Elevated Administrator privileges required!", GetLastError());
        return false;
    }

    SC_HANDLE hService = OpenServiceW(hSCM, serviceName.c_str(), SERVICE_ALL_ACCESS);
    if (!hService) {
        hService = CreateServiceW(
            hSCM,
            serviceName.c_str(),
            serviceName.c_str(),
            SERVICE_ALL_ACCESS,
            SERVICE_KERNEL_DRIVER,
            SERVICE_DEMAND_START,
            SERVICE_ERROR_NORMAL,
            driverPath.c_str(),
            NULL, NULL, NULL, NULL, NULL
        );
    }

    if (!hService) {
        Logger::Get().Log(LogLevel::Error, "CreateServiceW failed for %ls (Error: %d)", serviceName.c_str(), GetLastError());
        CloseServiceHandle(hSCM);
        return false;
    }

    BOOL started = StartServiceW(hService, 0, NULL);
    DWORD lastErr = GetLastError();

    if (!started && lastErr != ERROR_SERVICE_ALREADY_RUNNING) {
        Logger::Get().Log(LogLevel::Error, "StartServiceW failed for %ls (Error: %d)", serviceName.c_str(), lastErr);
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        return false;
    }

    Logger::Get().Log(LogLevel::Success, "Kernel driver service started: %ls", serviceName.c_str());
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return true;
}

bool DriverManager::StopAndDeleteService(const std::wstring& serviceName) {
    SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCM) return false;

    SC_HANDLE hService = OpenServiceW(hSCM, serviceName.c_str(), SERVICE_STOP | DELETE);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return false;
    }

    SERVICE_STATUS status;
    ControlService(hService, SERVICE_CONTROL_STOP, &status);
    BOOL deleted = DeleteService(hService);

    if (deleted) {
        Logger::Get().Log(LogLevel::Info, "Kernel driver service removed: %ls", serviceName.c_str());
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return deleted != 0;
}

bool DriverManager::IsServiceRunning(const std::wstring& serviceName) {
    SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCM) return false;

    SC_HANDLE hService = OpenServiceW(hSCM, serviceName.c_str(), SERVICE_QUERY_STATUS);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return false;
    }

    SERVICE_STATUS_PROCESS ssp{};
    DWORD bytesNeeded = 0;
    BOOL query = QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytesNeeded);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    return (query && ssp.dwCurrentState == SERVICE_RUNNING);
}

bool DriverManager::LoadGeneralBypass(AntiCheatCategory category) {
    const auto& profile = GetProfile(category);
    m_info.activeCategory = category;

    Logger::Get().Log(LogLevel::Info, "=== Initiating Bypass Load Sequence for [%s] ===", profile.name.c_str());
    Logger::Get().Log(LogLevel::Info, "Target Protection Services: %s", profile.targetServices.c_str());
    Logger::Get().Log(LogLevel::Info, "Stage 1 Staging Driver: %ls", m_info.stage1Path.c_str());
    Logger::Get().Log(LogLevel::Info, "Stage 2 Payload Driver: %ls", m_info.stage2Path.c_str());

    // Service suppression
    if (profile.terminateService) {
        if (category == AntiCheatCategory::BE) {
            Injector::Get().TerminateTargetService(L"BEService.exe");
        } else if (category == AntiCheatCategory::EAC) {
            Injector::Get().TerminateTargetService(L"EasyAntiCheat.exe");
            Injector::Get().TerminateTargetService(L"EasyAntiCheat_EOS.exe");
        } else if (category == AntiCheatCategory::ACE) {
            Injector::Get().TerminateTargetService(L"AntiCheatExpert.exe");
        }
    }

    // Read Stage 2 PE driver binary into memory buffer
    if (!fs::exists(m_info.stage2Path)) {
        Logger::Get().Log(LogLevel::Error, "Stage 2 kernel payload binary missing: %ls", m_info.stage2Path.c_str());
        m_info.status = DriverStatus::Failed;
        m_info.lastMessage = "Stage 2 kernel payload file missing";
        return false;
    }

    std::ifstream file(m_info.stage2Path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        Logger::Get().Log(LogLevel::Error, "Failed to open Stage 2 kernel payload file: %ls", m_info.stage2Path.c_str());
        m_info.status = DriverStatus::Failed;
        m_info.lastMessage = "Failed to open Stage 2 payload file";
        return false;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> driverBuffer(fileSize);
    if (!file.read(reinterpret_cast<char*>(driverBuffer.data()), fileSize)) {
        Logger::Get().Log(LogLevel::Error, "Failed to read Stage 2 payload file contents.");
        m_info.status = DriverStatus::Failed;
        m_info.lastMessage = "Failed to read Stage 2 payload contents";
        return false;
    }
    file.close();

    // 1. Install & Load Stage 1 Intel Staging Driver
    Logger::Get().Log(LogLevel::Stealth, "Step 1: Staging vulnerable Intel driver (iqvw64e.sys)...");
    NTSTATUS loadStatus = intel_driver::Load();
    if (!NT_SUCCESS(loadStatus)) {
        char errBuf[256];
        if (loadStatus == 0xC0001056 || loadStatus == 0xC00002E3 || loadStatus == 0xC0000428) {
            snprintf(errBuf, sizeof(errBuf), "Failed (0x%08X): HVCI / Memory Integrity is ENABLED in Windows Security. Disable Memory Integrity & Vulnerable Driver Blocklist.", loadStatus);
        } else if (loadStatus == 0xC0000022) {
            snprintf(errBuf, sizeof(errBuf), "Failed (0x%08X): Access Denied. Antivirus / Defender blocked driver load.", loadStatus);
        } else if (loadStatus == STATUS_ALREADY_REGISTERED || loadStatus == 0xC0000035) {
            snprintf(errBuf, sizeof(errBuf), "Failed (0x%08X): \\Device\\Nal already registered or leftover. Reboot system.", loadStatus);
        } else {
            snprintf(errBuf, sizeof(errBuf), "Stage 1 Load Failed (NTSTATUS: 0x%08X)", loadStatus);
        }

        Logger::Get().Log(LogLevel::Error, "%s", errBuf);
        m_info.status = DriverStatus::Failed;
        m_info.lastMessage = errBuf;
        return false;
    }

    m_info.status = DriverStatus::StagingLoaded;
    Logger::Get().Log(LogLevel::Success, "Stage 1 Staging Driver active (Handle: 0x%p, ntoskrnl Base: 0x%llX)", 
        intel_driver::hDevice, (unsigned long long)intel_driver::ntoskrnlAddr);

    // 2. Real BYOVD Kernel Pool PE Manual Mapping via kdmapper
    Logger::Get().Log(LogLevel::Stealth, "Step 2: Performing physical memory manual map of Stage 2 Kernel Payload...");
    NTSTATUS exitCode = 0;
    ULONG64 mappedAddress = kdmapper::MapDriver(
        driverBuffer.data(),
        0, 0,
        false, // free
        true,  // destroyHeader
        kdmapper::AllocationMode::AllocatePool,
        false, // PassAllocationAddressAsFirstParam
        nullptr, // callback
        &exitCode
    );

    // 3. Unload Stage 1 Staging Driver to clean module list footprint
    Logger::Get().Log(LogLevel::Stealth, "Step 3: Cleaning up Stage 1 Staging Driver service and PiDDB cache...");
    intel_driver::Unload();

    if (!mappedAddress) {
        Logger::Get().Log(LogLevel::Error, "kdmapper::MapDriver failed! DriverEntry NTSTATUS: 0x%08X", exitCode);
        m_info.status = DriverStatus::Failed;
        m_info.lastMessage = "Kernel Payload Mapping Failed";
        return false;
    }

    m_info.status = DriverStatus::PayloadActive;
    m_info.lastMessage = "General Bypass Active (" + profile.name + ")";
    Logger::Get().Log(LogLevel::Success, "Stage 2 Payload mapped into Kernel Pool at 0x%llX (DriverEntry NTSTATUS: 0x%08X)", 
        (unsigned long long)mappedAddress, exitCode);
    Logger::Get().Log(LogLevel::Success, "=== Bypass Load Complete for [%s] ===", profile.name.c_str());

    return true;
}

bool DriverManager::UnloadGeneralBypass() {
    Logger::Get().Log(LogLevel::Info, "Terminating General Bypass Kernel Drivers...");
    intel_driver::Unload();
    StopAndDeleteService(m_info.serviceName);

    m_info.status = DriverStatus::Unloaded;
    m_info.lastMessage = "Bypass Unloaded";
    Logger::Get().Log(LogLevel::Info, "General Bypass unloaded successfully.");
    return true;
}

} // namespace Inkwell
