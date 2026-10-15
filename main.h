#pragma once
#include <Windows.h>
#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"

namespace Inkwell {

constexpr int WINDOW_WIDTH = 960;
constexpr int WINDOW_HEIGHT = 580;

extern HWND g_hMainWnd;
extern LPDIRECT3D9 g_pD3D;
extern LPDIRECT3DDEVICE9 g_pd3dDevice;
extern D3DPRESENT_PARAMETERS g_d3dpp;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

} // namespace Inkwell
