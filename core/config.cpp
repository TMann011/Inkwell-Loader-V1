#include "config.h"
#include <Windows.h>
#include <filesystem>

namespace Inkwell {

ConfigManager& ConfigManager::Get() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::LoadFromFile(const std::string& filepath) {
    std::string fullPath = (std::filesystem::path(filepath).is_relative()) ? 
        (std::filesystem::current_path() / filepath).string() : filepath;

    if (!std::filesystem::exists(fullPath)) {
        SaveToFile(fullPath);
        return false;
    }

    char buffer[512];

    GetPrivateProfileStringA("Auth", "license_key", "INK-CORE-7749", buffer, sizeof(buffer), fullPath.c_str());
    m_data.licenseKey = buffer;
    m_data.rememberKey = GetPrivateProfileIntA("Auth", "remember_key", 1, fullPath.c_str()) != 0;

    m_data.selectedProduct = GetPrivateProfileIntA("Settings", "selected_product", 0, fullPath.c_str());
    m_data.stealthPebUnlink = GetPrivateProfileIntA("Settings", "stealth_peb_unlink", 1, filepath.c_str()) != 0;
    m_data.autoInjectWhenDetected = GetPrivateProfileIntA("Settings", "auto_inject", 0, filepath.c_str()) != 0;
    m_data.closeOnInject = GetPrivateProfileIntA("Settings", "close_on_inject", 0, filepath.c_str()) != 0;
    m_data.terminateAntiCheatService = GetPrivateProfileIntA("Settings", "terminate_ac_service", 1, filepath.c_str()) != 0;
    m_data.verifyModuleLoaded = GetPrivateProfileIntA("Settings", "verify_module", 1, filepath.c_str()) != 0;

    GetPrivateProfileStringA("Custom", "process_name", "TargetGame.exe", buffer, sizeof(buffer), filepath.c_str());
    m_data.customProcessName = buffer;

    GetPrivateProfileStringA("Custom", "dll_path", "payload.dll", buffer, sizeof(buffer), filepath.c_str());
    m_data.customDllPath = buffer;

    return true;
}

bool ConfigManager::SaveToFile(const std::string& filepath) {
    std::string fullPath = (std::filesystem::path(filepath).is_relative()) ? 
        (std::filesystem::current_path() / filepath).string() : filepath;

    WritePrivateProfileStringA("Auth", "license_key", m_data.licenseKey.c_str(), fullPath.c_str());
    WritePrivateProfileStringA("Auth", "remember_key", m_data.rememberKey ? "1" : "0", fullPath.c_str());

    WritePrivateProfileStringA("Settings", "selected_product", std::to_string(m_data.selectedProduct).c_str(), fullPath.c_str());
    WritePrivateProfileStringA("Settings", "stealth_peb_unlink", m_data.stealthPebUnlink ? "1" : "0", fullPath.c_str());
    WritePrivateProfileStringA("Settings", "auto_inject", m_data.autoInjectWhenDetected ? "1" : "0", fullPath.c_str());
    WritePrivateProfileStringA("Settings", "close_on_inject", m_data.closeOnInject ? "1" : "0", fullPath.c_str());
    WritePrivateProfileStringA("Settings", "terminate_ac_service", m_data.terminateAntiCheatService ? "1" : "0", fullPath.c_str());
    WritePrivateProfileStringA("Settings", "verify_module", m_data.verifyModuleLoaded ? "1" : "0", fullPath.c_str());

    WritePrivateProfileStringA("Custom", "process_name", m_data.customProcessName.c_str(), fullPath.c_str());
    WritePrivateProfileStringA("Custom", "dll_path", m_data.customDllPath.c_str(), fullPath.c_str());

    return true;
}

} // namespace Inkwell
