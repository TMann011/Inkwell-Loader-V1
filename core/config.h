#pragma once
#include <string>

namespace Inkwell {

struct ConfigData {
    // Auth settings
    std::string licenseKey = "INK-CORE-7749";
    bool rememberKey = true;

    // Operation settings
    int selectedProduct = 0;
    bool stealthPebUnlink = true;
    bool autoInjectWhenDetected = false;
    bool closeOnInject = false;
    bool terminateAntiCheatService = true;
    bool verifyModuleLoaded = true;

    // Custom target paths
    std::string customProcessName = "TargetGame.exe";
    std::string customDllPath = "payload.dll";
};

class ConfigManager {
public:
    static ConfigManager& Get();

    bool LoadFromFile(const std::string& filepath = "inkwell_config.ini");
    bool SaveToFile(const std::string& filepath = "inkwell_config.ini");

    ConfigData& Data() { return m_data; }
    const ConfigData& Data() const { return m_data; }

private:
    ConfigManager() = default;
    ConfigData m_data;
};

} // namespace Inkwell
