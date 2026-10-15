#pragma once
#include <Windows.h>
#include <string>
#include "theme.h"

namespace Inkwell {

enum class AppState {
    Auth,
    AuthLoading,
    Dashboard
};

enum class DashboardTab {
    Catalog = 0,
    InjectorControl = 1,
    GeneralBypass = 2,
    Console = 3,
    Settings = 4,
    ReferenceInjector = 5
};

class UI {
public:
    static UI& Get();

    void Init(HWND hWnd);
    void Render();

    bool IsRunning() const { return m_running; }
    void RequestClose() { m_running = false; }

    AppState GetState() const { return m_state; }

private:
    UI() = default;

    void RenderHeader();
    void RenderAuthScreen();
    void RenderDashboard();

    void RenderCatalogTab();
    void RenderInjectorTab();
    void RenderGeneralBypassTab();
    void RenderConsoleTab();
    void RenderSettingsTab();
    void RenderReferenceInjectorTab();
    void RenderFooter();

    HWND m_hWnd = nullptr;
    bool m_running = true;
    AppState m_state = AppState::Auth;
    DashboardTab m_currentTab = DashboardTab::Catalog;

    // Auth screen fields
    char m_keyBuffer[128] = "INK-CORE-7749";
    float m_authProgress = 0.0f;
    std::string m_authStatusText = "STANDBY";

    // Injector state cache
    char m_customExeBuffer[128] = "TargetGame.exe";
    char m_customDllBuffer[260] = "payload.dll";
    DWORD m_detectedPid = 0;
    bool m_processRunning = false;
    float m_statusTimer = 0.0f;

    // Reference Injector State
    char m_refProcFilter[64] = "";
    int m_refSelectedPid = 0;
    std::string m_refSelectedExeName = "";
    char m_refDllPathBuffer[260] = "";
};

} // namespace Inkwell
