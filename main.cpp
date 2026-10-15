#include "main.h"
#include "gui/ui.h"
#include "gui/theme.h"
#include "core/config.h"
#include "core/logger.h"
#include <shellapi.h>
#include <shlobj.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Inkwell {

HWND g_hMainWnd = nullptr;
LPDIRECT3D9 g_pD3D = nullptr;
LPDIRECT3DDEVICE9 g_pd3dDevice = nullptr;
D3DPRESENT_PARAMETERS g_d3dpp = {};

bool EnsureElevatedPrivileges() {
    if (IsUserAnAdmin()) {
        return true;
    }

    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, MAX_PATH)) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"runas";
        sei.lpFile = szPath;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;
        if (ShellExecuteExW(&sei)) {
            return false;
        }
    }
    return false;
}

bool CreateDeviceD3D(HWND hWnd) {
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
        return false;

    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE; // Vsync enabled for smooth rendering

    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0) {
        return false;
    }

    return true;
}

void CleanupDeviceD3D() {
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
    if (g_pD3D) {
        g_pD3D->Release();
        g_pD3D = nullptr;
    }
}

void ResetDevice() {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (FAILED(hr)) {
        return;
    }
    ImGui_ImplDX9_CreateDeviceObjects();
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_NCHITTEST: {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        ScreenToClient(hWnd, &pt);
        RECT rc;
        GetClientRect(hWnd, &rc);
        int border = 8;

        bool top = pt.y < border;
        bool bottom = pt.y >= rc.bottom - border;
        bool left = pt.x < border;
        bool right = pt.x >= rc.right - border;

        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        break;
    }
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
            g_d3dpp.BackBufferWidth = LOWORD(lParam);
            g_d3dpp.BackBufferHeight = HIWORD(lParam);
            ResetDevice();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

} // namespace Inkwell

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    if (!Inkwell::EnsureElevatedPrivileges()) {
        return 0;
    }

    // Register custom window class
    const wchar_t* kClassName = L"InkwellLoaderClass";
    const wchar_t* kWindowTitle = L"Inkwell Loader Core";

    WNDCLASSEXW wc = {
        sizeof(WNDCLASSEXW),
        CS_CLASSDC,
        Inkwell::WndProc,
        0L,
        0L,
        hInstance,
        NULL,
        LoadCursor(NULL, IDC_ARROW),
        NULL,
        NULL,
        kClassName,
        NULL
    };
    RegisterClassExW(&wc);

    // Calculate centered desktop coordinates
    RECT screenRect{};
    GetWindowRect(GetDesktopWindow(), &screenRect);
    int posX = (screenRect.right - Inkwell::WINDOW_WIDTH) / 2;
    int posY = (screenRect.bottom - Inkwell::WINDOW_HEIGHT) / 2;

    // Create resizable frameless window
    Inkwell::g_hMainWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        kWindowTitle,
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
        posX,
        posY,
        Inkwell::WINDOW_WIDTH,
        Inkwell::WINDOW_HEIGHT,
        NULL,
        NULL,
        wc.hInstance,
        NULL
    );

    if (!Inkwell::CreateDeviceD3D(Inkwell::g_hMainWnd)) {
        Inkwell::CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(Inkwell::g_hMainWnd, SW_SHOWDEFAULT);
    UpdateWindow(Inkwell::g_hMainWnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // Fileless ImGui state

    ImGui_ImplWin32_Init(Inkwell::g_hMainWnd);
    ImGui_ImplDX9_Init(Inkwell::g_pd3dDevice);

    // Initialize Inkwell UI manager
    Inkwell::UI::Get().Init(Inkwell::g_hMainWnd);

    // Message loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        if (!Inkwell::UI::Get().IsRunning()) {
            PostQuitMessage(0);
            continue;
        }

        // ImGui frame execution
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        Inkwell::UI::Get().Render();

        ImGui::EndFrame();

        Inkwell::g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        Inkwell::g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        Inkwell::g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

        // Dark clear canvas matching Inkwell background
        D3DCOLOR clearColor = D3DCOLOR_RGBA(15, 15, 20, 255);
        Inkwell::g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clearColor, 1.0f, 0);

        if (Inkwell::g_pd3dDevice->BeginScene() >= 0) {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            Inkwell::g_pd3dDevice->EndScene();
        }

        HRESULT result = Inkwell::g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
        if (result == D3DERR_DEVICELOST && Inkwell::g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
            Inkwell::ResetDevice();
        }
    }

    // Cleanup resources
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    Inkwell::CleanupDeviceD3D();
    DestroyWindow(Inkwell::g_hMainWnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}
