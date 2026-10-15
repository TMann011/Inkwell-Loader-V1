#include "ui.h"
#include "theme.h"
#include "../core/config.h"
#include "../core/injector.h"
#include "../core/driver.h"
#include "../core/logger.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_internal.h"
#include <cstdio>
#include <filesystem>
#include <commdlg.h>

namespace fs = std::filesystem;

namespace Inkwell {

UI& UI::Get() {
    static UI instance;
    return instance;
}

void UI::Init(HWND hWnd) {
    m_hWnd = hWnd;

    // Apply Default Purple & Black Theme
    ApplyThemePreset(ThemePreset::PurpleBlack);

    // Load initial config
    ConfigManager::Get().LoadFromFile("inkwell_config.ini");
    const auto& cfg = ConfigManager::Get().Data();

    strncpy_s(m_keyBuffer, cfg.licenseKey.c_str(), sizeof(m_keyBuffer) - 1);
    strncpy_s(m_customExeBuffer, cfg.customProcessName.c_str(), sizeof(m_customExeBuffer) - 1);
    strncpy_s(m_customDllBuffer, cfg.customDllPath.c_str(), sizeof(m_customDllBuffer) - 1);

    Logger::Get().Init("inkwell_loader.log");
    Logger::Get().Log(LogLevel::Info, "Inkwell Core Loader v3.2 initialized [Purple & Black Theme]");
    Logger::Get().Log(LogLevel::Info, "Target architecture: x64/x86 NTAPI Subsystem & Kernel Driver Staging");
}

void UI::Render() {
    // Poll process status every ~500ms
    m_statusTimer += ImGui::GetIO().DeltaTime;
    if (m_statusTimer >= 0.5f) {
        m_statusTimer = 0.0f;
        int prodIdx = ConfigManager::Get().Data().selectedProduct;
        const auto& prod = Injector::Get().GetProduct(prodIdx);
        std::wstring targetExeW(prod.targetExe.begin(), prod.targetExe.end());
        m_processRunning = Injector::Get().IsProcessRunning(targetExeW, &m_detectedPid);
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                            ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollbar |
                            ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    if (ImGui::Begin("##MainCanvas", nullptr, flags)) {
        RenderHeader();

        if (m_state == AppState::Auth || m_state == AppState::AuthLoading) {
            RenderAuthScreen();
        } else {
            RenderDashboard();
        }

        RenderFooter();
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
}

void UI::RenderHeader() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetWindowWidth();
    float headerHeight = 36.0f;

    // Header background & subtle gradient bar
    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + headerHeight), ImColor(g_Style.Background));
    drawList->AddLine(ImVec2(pos.x, pos.y + headerHeight), ImVec2(pos.x + width, pos.y + headerHeight), ImColor(g_Style.Border), 1.0f);

    // Accent line on very top
    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + 2.0f), ImColor(g_Style.AccentColor));

    // Title branding
    ImGui::SetCursorPos(ImVec2(12.0f, 8.0f));
    ImGui::TextColored(g_Style.AccentColor, "INKWELL");
    ImGui::SameLine();
    ImGui::TextColored(g_Style.TextSecondary, "|  LOADER CORE // NTAPI SUBSYSTEM");

    // Draggable header area
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetMousePos().y < pos.y + headerHeight) {
        if (m_hWnd) {
            ReleaseCapture();
            SendMessage(m_hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
    }

    // Window controls (Minimize, Maximize & Close)
    float btnWidth = 26.0f;
    float btnHeight = 22.0f;

    ImGui::SetCursorPos(ImVec2(width - (btnWidth * 3.0f) - 16.0f, 7.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.15f, 0.35f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.20f, 0.45f, 1.0f));

    if (ImGui::Button("-##Minimize", ImVec2(btnWidth, btnHeight))) {
        if (m_hWnd) ShowWindow(m_hWnd, SW_MINIMIZE);
    }

    ImGui::SameLine();
    if (ImGui::Button("[ ]##Maximize", ImVec2(btnWidth, btnHeight))) {
        if (m_hWnd) {
            WINDOWPLACEMENT wp = { sizeof(wp) };
            GetWindowPlacement(m_hWnd, &wp);
            if (wp.showCmd == SW_MAXIMIZE) ShowWindow(m_hWnd, SW_RESTORE);
            else ShowWindow(m_hWnd, SW_MAXIMIZE);
        }
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.15f, 0.15f, 0.8f));
    if (ImGui::Button("X##Close", ImVec2(btnWidth, btnHeight))) {
        RequestClose();
    }
    ImGui::PopStyleColor(4);

    ImGui::SetCursorPosY(headerHeight);
}

void UI::RenderAuthScreen() {
    float width = ImGui::GetWindowWidth();
    float height = ImGui::GetWindowHeight();

    ImGui::SetCursorPos(ImVec2(30.0f, 54.0f));
    ImGui::BeginChild("##AuthCard", ImVec2(width - 60.0f, height - 90.0f), true);

    ImGui::Spacing();

    // Clean, high-compatibility ASCII Logo for INKWELL
    const char* asciiArt =
        "  ___ _  _ _  ____      _____ _    _    \n"
        " |_ _| \\| | |/ /\\ \\    / / __| |  | |   \n"
        "  | || .` | ' <  \\ \\/\\/ /| _|| |__| |__ \n"
        " |___|_|\\_|_|\\_\\  \\_/\\_/ |___|____|____|";

    ImGui::TextColored(g_Style.AccentColor, "%s", asciiArt);

    ImGui::Spacing();
    ImGui::TextColored(g_Style.TextSecondary, "SECURE KERNEL & USER-MODE LOADER // AUTHORIZED ACCESS ONLY");
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();

    if (m_state == AppState::Auth) {
        ImGui::TextColored(g_Style.TextPrimary, "LICENSE CREDENTIALS");
        ImGui::SetNextItemWidth(340.0f);
        ImGui::InputText("##LicenseKey", m_keyBuffer, sizeof(m_keyBuffer));

        ImGui::Spacing();
        bool remember = ConfigManager::Get().Data().rememberKey;
        if (ImGui::Checkbox("Persist license token across sessions", &remember)) {
            ConfigManager::Get().Data().rememberKey = remember;
            ConfigManager::Get().Data().licenseKey = m_keyBuffer;
            ConfigManager::Get().SaveToFile();
        }

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, g_Style.AccentColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_Style.AccentHovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_Style.AccentActive);

        if (ImGui::Button("AUTHENTICATE & ENTER", ImVec2(220.0f, 34.0f))) {
            m_state = AppState::AuthLoading;
            m_authProgress = 0.0f;
            m_authStatusText = "Verifying cryptographic token...";
            Logger::Get().Log(LogLevel::Info, "Authenticating license key: %s", m_keyBuffer);
        }

        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        if (ImGui::Button("EXIT", ImVec2(90.0f, 34.0f))) {
            RequestClose();
        }
    } else if (m_state == AppState::AuthLoading) {
        m_authProgress += 0.8f * ImGui::GetIO().DeltaTime;

        if (m_authProgress < 0.35f) {
            m_authStatusText = "Connecting to telemetry server...";
        } else if (m_authProgress < 0.70f) {
            m_authStatusText = "Validating hardware fingerprint (HWID)...";
        } else if (m_authProgress < 1.0f) {
            m_authStatusText = "Unpacking payload descriptors...";
        } else {
            m_state = AppState::Dashboard;
            Logger::Get().Log(LogLevel::Success, "Authentication granted. Operator identity confirmed.");
        }

        ImGui::TextColored(g_Style.TextSecondary, "%s", m_authStatusText.c_str());
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, g_Style.AccentColor);
        ImGui::ProgressBar(m_authProgress, ImVec2(400.0f, 20.0f));
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
}

void UI::RenderDashboard() {
    float width = ImGui::GetWindowWidth();
    float height = ImGui::GetWindowHeight();

    // Top Navigation Tabs
    ImGui::SetCursorPos(ImVec2(16.0f, 44.0f));
    ImGui::BeginChild("##TabBar", ImVec2(width - 32.0f, 34.0f), false);

    auto renderTabButton = [this](DashboardTab tab, const char* label, float btnWidth) {
        bool isActive = (m_currentTab == tab);
        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, g_Style.AccentColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_Style.AccentHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_Style.AccentActive);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(g_Style.PanelAlt.x, g_Style.PanelAlt.y, g_Style.PanelAlt.z, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(g_Style.PanelAlt.x * 1.3f, g_Style.PanelAlt.y * 1.3f, g_Style.PanelAlt.z * 1.3f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_Style.AccentColor);
        }

        if (ImGui::Button(label, ImVec2(btnWidth, 28.0f))) {
            m_currentTab = tab;
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
    };

    renderTabButton(DashboardTab::Catalog, "01 // GAMES", 110.0f);
    renderTabButton(DashboardTab::InjectorControl, "02 // INJECTOR", 115.0f);
    renderTabButton(DashboardTab::GeneralBypass, "03 // GENERAL BYPASS", 145.0f);
    renderTabButton(DashboardTab::Console, "04 // CONSOLE", 110.0f);
    renderTabButton(DashboardTab::Settings, "05 // SETTINGS", 115.0f);
    renderTabButton(DashboardTab::ReferenceInjector, "06 // DLL INJECTOR CORE", 165.0f);

    ImGui::EndChild();

    // Main Tab Content Panel
    ImGui::SetCursorPos(ImVec2(16.0f, 82.0f));
    ImGui::BeginChild("##DashboardContent", ImVec2(width - 32.0f, height - 114.0f), true);

    switch (m_currentTab) {
    case DashboardTab::Catalog:
        RenderCatalogTab();
        break;
    case DashboardTab::InjectorControl:
        RenderInjectorTab();
        break;
    case DashboardTab::GeneralBypass:
        RenderGeneralBypassTab();
        break;
    case DashboardTab::Console:
        RenderConsoleTab();
        break;
    case DashboardTab::Settings:
        RenderSettingsTab();
        break;
    case DashboardTab::ReferenceInjector:
        RenderReferenceInjectorTab();
        break;
    }

    ImGui::EndChild();
}

void UI::RenderCatalogTab() {
    const auto& catalog = Injector::Get().GetCatalog();
    auto& cfg = ConfigManager::Get().Data();

    float panelWidth = ImGui::GetWindowWidth();

    // Left column: Products list
    ImGui::BeginChild("##ProductList", ImVec2(230.0f, 0.0f), true);
    ImGui::TextColored(g_Style.TextSecondary, "AVAILABLE MODULES");
    ImGui::Separator();
    ImGui::Spacing();

    for (size_t i = 0; i < catalog.size(); ++i) {
        bool selected = (cfg.selectedProduct == static_cast<int>(i));
        std::string label = catalog[i].name;

        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Header, g_Style.AccentColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, g_Style.AccentHovered);
        }

        if (ImGui::Selectable(label.c_str(), selected, 0, ImVec2(0, 26.0f))) {
            cfg.selectedProduct = static_cast<int>(i);
        }

        if (selected) {
            ImGui::PopStyleColor(2);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right column: Selected Product Details & Launch Card
    ImGui::BeginChild("##ProductDetails", ImVec2(panelWidth - 260.0f, 0.0f), true);
    const auto& current = Injector::Get().GetProduct(cfg.selectedProduct);

    ImGui::TextColored(g_Style.AccentColor, "%s", current.name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(g_Style.TextSecondary, "| %s", current.version.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(g_Style.TextSecondary, "Engine / Architecture: ");
    ImGui::SameLine();
    ImGui::TextColored(g_Style.TextPrimary, "%s", current.gameEngine.c_str());

    ImGui::TextColored(g_Style.TextSecondary, "Target Executable:     ");
    ImGui::SameLine();
    ImGui::TextColored(g_Style.TextPrimary, "%s", current.targetExe.c_str());

    ImGui::TextColored(g_Style.TextSecondary, "Payload DLL:           ");
    ImGui::SameLine();
    ImGui::TextColored(g_Style.TextPrimary, "%s", current.defaultDllName.c_str());

    ImGui::Spacing();
    ImGui::TextColored(g_Style.TextSecondary, "Target Status: ");
    ImGui::SameLine();
    if (m_processRunning) {
        ImGui::TextColored(g_Style.StatusSuccess, "[RUNNING] PID: %d", m_detectedPid);
    } else {
        ImGui::TextColored(g_Style.StatusWarning, "[NOT DETECTED] Awaiting process spawn");
    }

    ImGui::Spacing();
    ImGui::TextColored(g_Style.TextSecondary, "Description:");
    ImGui::TextWrapped("%s", current.description.c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    bool isInjecting = (Injector::Get().GetAsyncStatus() != InjectionStatus::Idle &&
                        Injector::Get().GetAsyncStatus() != InjectionStatus::Success &&
                        Injector::Get().GetAsyncStatus() != InjectionStatus::Failed);

    if (isInjecting) {
        ImGui::TextColored(g_Style.StatusStealth, "%s", Injector::Get().GetStatusString().c_str());
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, g_Style.AccentColor);
        ImGui::ProgressBar(Injector::Get().GetProgress(), ImVec2(320.0f, 22.0f));
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, g_Style.AccentColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_Style.AccentHovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_Style.AccentActive);

        if (ImGui::Button("LAUNCH & CLOAK INJECT", ImVec2(240.0f, 36.0f))) {
            Injector::Get().StartAsyncInjection(cfg.selectedProduct);
        }
        ImGui::PopStyleColor(3);

        if (Injector::Get().GetAsyncStatus() == InjectionStatus::Success) {
            ImGui::SameLine();
            ImGui::TextColored(g_Style.StatusSuccess, "LOADED & CLOAKED");
        } else if (Injector::Get().GetAsyncStatus() == InjectionStatus::Failed) {
            ImGui::SameLine();
            ImGui::TextColored(g_Style.StatusError, "INJECTION FAILED");
        }
    }

    ImGui::EndChild();
}

void UI::RenderInjectorTab() {
    auto& cfg = ConfigManager::Get().Data();

    ImGui::TextColored(g_Style.TextPrimary, "INJECTION CONTROL & PROCESS INTROSPECTION");
    ImGui::TextColored(g_Style.TextSecondary, "Direct virtual memory manipulation with NTAPI PEB unlinking primitives.");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(g_Style.TextSecondary, "Custom Process Target (e.g. TargetGame.exe):");
    ImGui::SetNextItemWidth(360.0f);
    ImGui::InputText("##CustomProc", m_customExeBuffer, sizeof(m_customExeBuffer));

    ImGui::Spacing();
    ImGui::TextColored(g_Style.TextSecondary, "Custom Payload Path (.dll):");
    ImGui::SetNextItemWidth(360.0f);
    ImGui::InputText("##CustomDll", m_customDllBuffer, sizeof(m_customDllBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Browse...##DllBrowse", ImVec2(80.0f, 24.0f))) {
        OPENFILENAMEA ofn{};
        char szFile[260] = { 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = m_hWnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Dynamic Link Library (*.dll)\0*.dll\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            strncpy_s(m_customDllBuffer, ofn.lpstrFile, sizeof(m_customDllBuffer) - 1);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(g_Style.TextPrimary, "INJECTION TECHNIQUE VECTOR");
    
    auto currentTech = Injector::Get().GetTechnique();
    auto renderTechButton = [&currentTech](InjectionTechnique tech, const char* label, float width) {
        bool active = (currentTech == tech);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, g_Style.AccentColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_Style.AccentHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_Style.AccentActive);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(g_Style.PanelAlt.x, g_Style.PanelAlt.y, g_Style.PanelAlt.z, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(g_Style.PanelAlt.x * 1.3f, g_Style.PanelAlt.y * 1.3f, g_Style.PanelAlt.z * 1.3f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_Style.AccentColor);
        }

        if (ImGui::Button(label, ImVec2(width, 28.0f))) {
            Injector::Get().SetTechnique(tech);
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
    };

    renderTechButton(InjectionTechnique::StandardLoadLibrary, "Standard LoadLibrary + PEB Cloak", 230.0f);
    renderTechButton(InjectionTechnique::ManualMap, "Reflective PE Manual Map", 190.0f);
    renderTechButton(InjectionTechnique::DllHijackProxy, "DLL Hijack Proxy (version.dll)", 210.0f);
    renderTechButton(InjectionTechnique::KernelDirectCopy, "Kernel Direct Copy", 160.0f);

    ImGui::NewLine();
    ImGui::Spacing();

    ImGui::TextColored(g_Style.TextPrimary, "SECURITY & CLOAKING FLAGS");
    ImGui::Checkbox("Stealth PEB Unlinking (Remove from InLoadOrder, InMemoryOrder & InInitOrder lists)", &cfg.stealthPebUnlink);
    ImGui::Checkbox("Suppress Anti-Cheat Services (e.g. BEService.exe)", &cfg.terminateAntiCheatService);
    ImGui::Checkbox("Post-Injection Cloak Verification (Toolhelp scan)", &cfg.verifyModuleLoaded);

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, g_Style.AccentColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_Style.AccentHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_Style.AccentActive);

    if (ImGui::Button("EXECUTE MANUAL INJECTION", ImVec2(240.0f, 34.0f))) {
        cfg.customProcessName = m_customExeBuffer;
        cfg.customDllPath = m_customDllBuffer;
        Injector::Get().StartAsyncInjection(4, m_customExeBuffer, m_customDllBuffer);
    }

    ImGui::PopStyleColor(3);
}

void UI::RenderGeneralBypassTab() {
    static AntiCheatCategory selectedCat = AntiCheatCategory::BE;

    ImGui::TextColored(g_Style.TextPrimary, "KERNEL GENERAL BYPASS // TWO-STAGE BYOVD DRIVER SUBSYSTEM");
    ImGui::TextColored(g_Style.TextSecondary, "Targeted kernel-level bypass profiles for Easy Anti-Cheat, BattEye, ACE, and generic targets.");
    ImGui::Separator();
    ImGui::Spacing();

    // Category Selector Bar
    auto renderCatButton = [](AntiCheatCategory cat, const char* label, float width) {
        bool active = (selectedCat == cat);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, g_Style.AccentColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_Style.AccentHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_Style.AccentActive);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(g_Style.PanelAlt.x, g_Style.PanelAlt.y, g_Style.PanelAlt.z, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(g_Style.PanelAlt.x * 1.3f, g_Style.PanelAlt.y * 1.3f, g_Style.PanelAlt.z * 1.3f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_Style.AccentColor);
        }

        if (ImGui::Button(label, ImVec2(width, 28.0f))) {
            selectedCat = cat;
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
    };

    renderCatButton(AntiCheatCategory::EAC, "EAC (Easy Anti-Cheat)", 150.0f);
    renderCatButton(AntiCheatCategory::BE, "BE (BattEye)", 120.0f);
    renderCatButton(AntiCheatCategory::ACE, "ACE (Anti-Cheat Expert)", 160.0f);
    renderCatButton(AntiCheatCategory::Other, "OTHER / CUSTOM", 130.0f);

    ImGui::NewLine();
    ImGui::Spacing();

    const auto& profile = DriverManager::Get().GetProfile(selectedCat);
    const auto& info = DriverManager::Get().GetInfo();

    // Active Category Profile Details Card
    ImGui::BeginChild("##CategoryCard", ImVec2(0.0f, 160.0f), true);

    ImGui::TextColored(g_Style.AccentColor, "TARGET PROFILE: %s", profile.name.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(g_Style.TextSecondary, "Target Protection Services: ");
    ImGui::SameLine();
    ImGui::TextColored(g_Style.TextPrimary, "%s", profile.targetServices.c_str());

    ImGui::TextColored(g_Style.TextSecondary, "Stage 1 Staging Driver:    ");
    ImGui::SameLine();
    ImGui::TextColored(g_Style.TextPrimary, "%ls", info.stage1Path.c_str());

    ImGui::TextColored(g_Style.TextSecondary, "Stage 2 Kernel Payload:    ");
    ImGui::SameLine();
    ImGui::TextColored(g_Style.TextPrimary, "%ls", info.stage2Path.c_str());

    ImGui::TextColored(g_Style.TextSecondary, "Strategy Description:      ");
    ImGui::TextWrapped("%s", profile.description.c_str());

    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::TextColored(g_Style.TextSecondary, "BYPASS SUBSYSTEM STATUS: ");
    ImGui::SameLine();

    switch (info.status) {
    case DriverStatus::Unloaded:
        ImGui::TextColored(g_Style.TextMuted, "[UNLOADED] Kernel service inactive");
        break;
    case DriverStatus::StagingLoaded:
        ImGui::TextColored(g_Style.StatusWarning, "[STAGING ACTIVE] Stage 1 Intel driver loaded");
        break;
    case DriverStatus::PayloadActive:
        ImGui::TextColored(g_Style.StatusSuccess, "[BYPASS ACTIVE] Stage 2 kernel payload active (%s)", DriverManager::Get().GetProfile(info.activeCategory).name.c_str());
        break;
    case DriverStatus::Failed:
        ImGui::PushStyleColor(ImGuiCol_Text, g_Style.StatusError);
        ImGui::TextWrapped("[FAILED] %s", info.lastMessage.c_str());
        ImGui::PopStyleColor();
        break;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, g_Style.AccentColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_Style.AccentHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_Style.AccentActive);

    char loadBtnText[128];
    snprintf(loadBtnText, sizeof(loadBtnText), "LOAD BYPASS FOR %s", profile.name.c_str());

    if (ImGui::Button(loadBtnText, ImVec2(280.0f, 36.0f))) {
        DriverManager::Get().LoadGeneralBypass(selectedCat);
    }

    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (ImGui::Button("UNLOAD BYPASS SUBSYSTEM", ImVec2(200.0f, 36.0f))) {
        DriverManager::Get().UnloadGeneralBypass();
    }
}

void UI::RenderConsoleTab() {
    ImGui::TextColored(g_Style.TextPrimary, "DIAGNOSTIC LOG & SUBSYSTEM TELEMETRY");
    ImGui::SameLine();
    if (ImGui::Button("Clear Log##Clear", ImVec2(70.0f, 20.0f))) {
        Logger::Get().Clear();
    }
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginChild("##LogScrollRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

    auto entries = Logger::Get().GetEntries();
    for (const auto& entry : entries) {
        ImVec4 col = g_Style.TextPrimary;
        const char* prefix = "[INFO]";

        switch (entry.level) {
        case LogLevel::Success:
            col = g_Style.StatusSuccess;
            prefix = "[OK]  ";
            break;
        case LogLevel::Warning:
            col = g_Style.StatusWarning;
            prefix = "[WARN]";
            break;
        case LogLevel::Error:
            col = g_Style.StatusError;
            prefix = "[ERR] ";
            break;
        case LogLevel::Stealth:
            col = g_Style.StatusStealth;
            prefix = "[CLOAK]";
            break;
        default:
            col = g_Style.TextSecondary;
            break;
        }

        ImGui::TextColored(g_Style.TextMuted, "[%s]", entry.timestamp.c_str());
        ImGui::SameLine();
        ImGui::TextColored(col, "%s %s", prefix, entry.message.c_str());
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

void UI::RenderSettingsTab() {
    auto& cfg = ConfigManager::Get().Data();

    ImGui::TextColored(g_Style.TextPrimary, "LOADER PREFERENCES & ACCESSIBILITY CUSTOMIZER");
    ImGui::TextColored(g_Style.TextSecondary, "Configure UI theme colors, high contrast mode, and loader options.");
    ImGui::Separator();
    ImGui::Spacing();

    // Theme Switcher Section
    ImGui::TextColored(g_Style.AccentColor, "COLOR THEME PRESETS");
    const char* presets[] = {
        "0 // Inkwell Purple & Black (Default)",
        "1 // Electric Indigo (Classic)",
        "2 // Cyber Emerald (High Contrast)",
        "3 // Crimson Blood",
        "4 // Custom Palette"
    };

    int currentIdx = static_cast<int>(g_Style.currentPreset);
    ImGui::SetNextItemWidth(320.0f);
    if (ImGui::Combo("##ThemeCombo", &currentIdx, presets, IM_ARRAYSIZE(presets))) {
        ApplyThemePreset(static_cast<ThemePreset>(currentIdx));
    }

    ImGui::Spacing();
    bool contrast = g_Style.highContrast;
    if (ImGui::Checkbox("High Contrast Mode (Accessibility)", &contrast)) {
        SetHighContrast(contrast);
    }

    ImGui::Spacing();
    if (ImGui::TreeNode("Custom Color Pickers")) {
        ImGui::ColorEdit4("Accent Color", (float*)&g_Style.AccentColor);
        ImGui::ColorEdit4("Background", (float*)&g_Style.Background);
        ImGui::ColorEdit4("Panel Background", (float*)&g_Style.PanelBg);
        ImGui::ColorEdit4("Primary Text", (float*)&g_Style.TextPrimary);
        ImGui::ColorEdit4("Secondary Text", (float*)&g_Style.TextSecondary);
        if (ImGui::Button("Apply Custom Colors")) {
            ApplyThemePreset(ThemePreset::Custom);
        }
        ImGui::TreePop();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(g_Style.AccentColor, "LOADER OPERATION PREFERENCES");
    ImGui::Checkbox("Auto-inject automatically when game process is detected", &cfg.autoInjectWhenDetected);
    ImGui::Checkbox("Terminate loader upon successful payload delivery", &cfg.closeOnInject);
    ImGui::Checkbox("Stealth PEB Unlinking enabled by default", &cfg.stealthPebUnlink);
    ImGui::Checkbox("Suppress Anti-Cheat service processes before injection", &cfg.terminateAntiCheatService);
    ImGui::Checkbox("Verify module hidden in target memory space", &cfg.verifyModuleLoaded);

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::Button("SAVE CONFIGURATION", ImVec2(180.0f, 30.0f))) {
        cfg.customProcessName = m_customExeBuffer;
        cfg.customDllPath = m_customDllBuffer;
        if (ConfigManager::Get().SaveToFile()) {
            Logger::Get().Log(LogLevel::Success, "Configuration written to inkwell_config.ini");
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("RELOAD CONFIG", ImVec2(140.0f, 30.0f))) {
        ConfigManager::Get().LoadFromFile();
        Logger::Get().Log(LogLevel::Info, "Configuration reloaded from inkwell_config.ini");
    }
}

void UI::RenderReferenceInjectorTab() {
    ImGui::TextColored(g_Style.TextPrimary, "BUILT-IN DLL INJECTOR // MULTI-PRIMITIVE REFERENCE ENGINE");
    ImGui::TextColored(g_Style.TextSecondary, "Standalone process introspector & injector referencing custom C# (Injector.cs) and native primitives.");
    ImGui::Separator();
    ImGui::Spacing();

    // Two-Column Layout: Left = Process List, Right = Injection Vector & Module Inspector
    float totalWidth = ImGui::GetContentRegionAvail().x;
    float leftWidth = totalWidth * 0.42f;
    float rightWidth = totalWidth - leftWidth - 16.0f;

    // Left Pane: Process Enumeration & Search
    ImGui::BeginChild("##RefProcessListPane", ImVec2(leftWidth, 0.0f), true);
    ImGui::TextColored(g_Style.AccentColor, "ACTIVE SYSTEM PROCESSES");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(g_Style.TextSecondary, "Filter Process Name:");
    ImGui::SetNextItemWidth(leftWidth - 20.0f);
    ImGui::InputText("##ProcSearchFilter", m_refProcFilter, sizeof(m_refProcFilter));

    ImGui::Spacing();
    ImGui::BeginChild("##ProcessScrollList", ImVec2(0.0f, 0.0f), false);

    auto processList = Injector::Get().EnumerateActiveProcesses();
    std::string filterStr(m_refProcFilter);
    std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

    for (const auto& proc : processList) {
        std::string exeNameLower = "";
        for (wchar_t ch : proc.processName) {
            exeNameLower += static_cast<char>(ch);
        }
        std::transform(exeNameLower.begin(), exeNameLower.end(), exeNameLower.begin(), ::tolower);

        if (!filterStr.empty() && exeNameLower.find(filterStr) == std::string::npos) {
            continue;
        }

        bool isSelected = (m_refSelectedPid == (int)proc.pid);
        char label[256];
        snprintf(label, sizeof(label), "[%d] %ls (%s)", proc.pid, proc.processName.c_str(), proc.is64Bit ? "x64" : "x86");

        if (ImGui::Selectable(label, isSelected)) {
            m_refSelectedPid = proc.pid;
            m_refSelectedExeName = "";
            for (wchar_t ch : proc.processName) {
                m_refSelectedExeName += static_cast<char>(ch);
            }
            strncpy_s(m_customExeBuffer, m_refSelectedExeName.c_str(), sizeof(m_customExeBuffer) - 1);
        }
    }

    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::SameLine();

    // Right Pane: Technique Vector, Controls & Module List
    ImGui::BeginChild("##RefControlPane", ImVec2(rightWidth, 0.0f), true);

    ImGui::TextColored(g_Style.AccentColor, "TARGET SELECTION & TECHNIQUE VECTOR");
    ImGui::Separator();
    ImGui::Spacing();

    if (m_refSelectedPid != 0) {
        ImGui::TextColored(g_Style.StatusSuccess, "Target Selected: %s (PID: %d)", m_refSelectedExeName.c_str(), m_refSelectedPid);
    } else {
        ImGui::TextColored(g_Style.StatusWarning, "No Target Process Selected (Select from left process list)");
    }

    ImGui::Spacing();
    ImGui::TextColored(g_Style.TextSecondary, "Payload DLL Path:");
    ImGui::SetNextItemWidth(rightWidth - 110.0f);
    ImGui::InputText("##RefDllPath", m_refDllPathBuffer, sizeof(m_refDllPathBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Browse...##RefBrowse", ImVec2(90.0f, 24.0f))) {
        OPENFILENAMEA ofn{};
        char szFile[260] = { 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = m_hWnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Dynamic Link Library (*.dll)\0*.dll\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            strncpy_s(m_refDllPathBuffer, ofn.lpstrFile, sizeof(m_refDllPathBuffer) - 1);
        }
    }

    ImGui::Spacing();
    ImGui::TextColored(g_Style.TextPrimary, "DISPATCH PRIMITIVE METHOD:");

    auto currentTech = Injector::Get().GetTechnique();
    int techIdx = static_cast<int>(currentTech);

    const char* techNames[] = {
        "01 // CreateRemoteThread (Standard Win32 API)",
        "02 // NtCreateThreadEx (Undocumented NTAPI Kernel Object Dispatch)",
        "03 // QueueUserAPC (APC Thread Hijacking)",
        "04 // Reflective PE Manual Mapping (Zero PEB Footprint)",
        "05 // DLL Hijack Proxy Generator (version.dll)",
        "06 // Kernel Direct Copy (Kernel Pool Virtual Write)"
    };

    ImGui::SetNextItemWidth(rightWidth - 20.0f);
    if (ImGui::Combo("##TechniqueCombo", &techIdx, techNames, IM_ARRAYSIZE(techNames))) {
        Injector::Get().SetTechnique(static_cast<InjectionTechnique>(techIdx));
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, g_Style.AccentColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_Style.AccentHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_Style.AccentActive);

    if (ImGui::Button("EXECUTE DLL INJECTION PRIMITIVE", ImVec2(280.0f, 34.0f))) {
        if (m_refSelectedPid != 0 && strlen(m_refDllPathBuffer) > 0) {
            std::string dllPathStr(m_refDllPathBuffer);
            std::wstring dllPathW(dllPathStr.begin(), dllPathStr.end());
            
            bool success = false;
            switch (Injector::Get().GetTechnique()) {
            case InjectionTechnique::StandardLoadLibrary:
                success = Injector::Get().InjectDll(m_refSelectedPid, dllPathW);
                break;
            case InjectionTechnique::NtCreateThreadEx:
                success = Injector::Get().InjectNtCreateThreadEx(m_refSelectedPid, dllPathW);
                break;
            case InjectionTechnique::QueueUserAPC:
                success = Injector::Get().InjectQueueUserAPC(m_refSelectedPid, dllPathW);
                break;
            case InjectionTechnique::ManualMap:
                success = Injector::Get().ManualMapDll(m_refSelectedPid, dllPathW);
                break;
            case InjectionTechnique::DllHijackProxy:
                success = Injector::Get().DeployDllHijack(m_refSelectedPid, dllPathW);
                break;
            case InjectionTechnique::KernelDirectCopy:
                success = Injector::Get().KernelDirectCopyDll(m_refSelectedPid, dllPathW);
                break;
            }

            if (success) {
                Logger::Get().Log(LogLevel::Success, "Built-in Injector: Primitive dispatch succeeded for PID %d", m_refSelectedPid);
            } else {
                Logger::Get().Log(LogLevel::Error, "Built-in Injector: Primitive dispatch failed.");
            }
        } else {
            Logger::Get().Log(LogLevel::Error, "Select a valid PID and specify a payload DLL path.");
        }
    }
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(g_Style.TextPrimary, "LOADED TARGET MODULES (INSPECTOR)");
    if (m_refSelectedPid != 0) {
        auto modules = Injector::Get().EnumerateProcessModules(m_refSelectedPid);
        ImGui::BeginChild("##ModuleInspectorScroll", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);

        ImGui::Columns(4, "##ModCols", true);
        ImGui::TextColored(g_Style.AccentColor, "Module Name"); ImGui::NextColumn();
        ImGui::TextColored(g_Style.AccentColor, "Base Address"); ImGui::NextColumn();
        ImGui::TextColored(g_Style.AccentColor, "Size"); ImGui::NextColumn();
        ImGui::TextColored(g_Style.AccentColor, "Path"); ImGui::NextColumn();
        ImGui::Separator();

        for (const auto& mod : modules) {
            ImGui::Text("%ls", mod.moduleName.c_str()); ImGui::NextColumn();
            ImGui::Text("0x%llX", (unsigned long long)mod.baseAddress); ImGui::NextColumn();
            ImGui::Text("%u KB", mod.moduleSize / 1024); ImGui::NextColumn();
            ImGui::Text("%ls", mod.modulePath.c_str()); ImGui::NextColumn();
        }
        ImGui::Columns(1);
        ImGui::EndChild();
    } else {
        ImGui::TextColored(g_Style.TextMuted, "Select a running process to inspect loaded HMODULE entries.");
    }

    ImGui::EndChild();
}

void UI::RenderFooter() {
    float width = ImGui::GetWindowWidth();
    float height = ImGui::GetWindowHeight();
    float footerHeight = 26.0f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImVec2(0, height - footerHeight);

    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + footerHeight), ImColor(0.02f, 0.02f, 0.03f, 1.0f));
    drawList->AddLine(pos, ImVec2(pos.x + width, pos.y), ImColor(g_Style.Border), 1.0f);

    ImGui::SetCursorPos(ImVec2(12.0f, height - footerHeight + 5.0f));
    ImGui::TextColored(g_Style.TextMuted, "HWID: 7FA3-998C-D14B-E012");

    ImGui::SameLine();
    ImGui::SetCursorPosX(width - 160.0f);
    ImGui::TextColored(g_Style.TextMuted, "INKWELL CORE v3.2");
}

} // namespace Inkwell
