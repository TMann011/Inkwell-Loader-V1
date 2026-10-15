#pragma once
#include <string>
#include <vector>
#include <Windows.h>

namespace Inkwell {

enum class AntiCheatCategory {
    EAC = 0,   // Easy Anti-Cheat Profile
    BE = 1,    // BattEye Profile
    ACE = 2,   // Anti-Cheat Expert Profile
    Other = 3  // Generic / Custom Profile
};

enum class DriverStatus {
    Unloaded,
    StagingLoaded,
    PayloadActive,
    Failed
};

struct AntiCheatProfile {
    std::string name;
    AntiCheatCategory category;
    std::string targetServices;
    std::string description;
    bool terminateService;
    bool stealthPebUnlink;
    bool manualMapKernelPool;
};

struct DriverInfo {
    std::wstring stage1Path; // Vulnerable Intel driver (iqvw64e.sys)
    std::wstring stage2Path; // Payload driver (stage2_payload_1.5MB.sys)
    std::wstring serviceName;
    DriverStatus status;
    AntiCheatCategory activeCategory;
    std::string lastMessage;
};

class DriverManager {
public:
    static DriverManager& Get();

    // Kernel Driver Service primitives
    bool InstallAndStartService(const std::wstring& driverPath, const std::wstring& serviceName);
    bool StopAndDeleteService(const std::wstring& serviceName);
    bool IsServiceRunning(const std::wstring& serviceName);

    // Categorized BYOVD driver load sequence
    bool LoadGeneralBypass(AntiCheatCategory category);
    bool UnloadGeneralBypass();

    const DriverInfo& GetInfo() const { return m_info; }
    const std::vector<AntiCheatProfile>& GetProfiles() const { return m_profiles; }
    const AntiCheatProfile& GetProfile(AntiCheatCategory category) const;

private:
    DriverManager();
    ~DriverManager() = default;

    DriverInfo m_info;
    std::vector<AntiCheatProfile> m_profiles;
};

} // namespace Inkwell
