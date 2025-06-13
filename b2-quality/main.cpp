#include <Windows.h>
#include <Magnification.h>
#include <iostream>

#pragma comment(lib, "Magnification.lib")

static float zoomFactor = 1.0f; // file-local zoom factor

using MagSmoothFn = BOOL(WINAPI*)(BOOL);

BOOL SetZoom(float magnificationFactor)
{
    if (magnificationFactor < 1.0f)
        return FALSE;

    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    int xOff = (int)(w * (1.0f - 1.0f / magnificationFactor) / 2);
    int yOff = (int)(h * (1.0f - 1.0f / magnificationFactor) / 2);

    return MagSetFullscreenTransform(magnificationFactor, xOff, yOff);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_HOTKEY)
    {
        // Ctrl + / Ctrl -
        if (wParam == 1)
        {
            zoomFactor += 0.1f;
            SetZoom(zoomFactor);
        }
        else if (wParam == 2)
        {
            zoomFactor = max(1.0f, zoomFactor - 0.1f);
            SetZoom(zoomFactor);
        }
        return 0;
    }
    else if (msg == WM_DESTROY)
    {
        MagUninitialize();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int)
{
    if (!MagInitialize())
    {
        MessageBox(nullptr, L"Magnification API init failed", L"Error", MB_ICONERROR);
        return 1;
    }

    // Load smoothing functions dynamically
    HMODULE mag = LoadLibraryW(L"Magnification.dll");
    if (mag)
    {
        auto pFSmooth = (MagSmoothFn)GetProcAddress(mag, "MagSetFullscreenUseBitmapSmoothing");
        auto pLSmooth = (MagSmoothFn)GetProcAddress(mag, "MagSetLensUseBitmapSmoothing");
        if (pFSmooth) pFSmooth(TRUE);
        if (pLSmooth) pLSmooth(TRUE);
    }

    const wchar_t CLASS_NAME[] = L"FullscreenMagnifier";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    if (!RegisterClass(&wc))
    {
        MessageBox(nullptr, L"RegisterClass failed", L"Error", MB_ICONERROR);
        MagUninitialize();
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        CLASS_NAME, L"Magnifier", WS_POPUP,
        0, 0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, hInst, nullptr);

    if (!hwnd)
    {
        MessageBox(nullptr, L"CreateWindow failed", L"Error", MB_ICONERROR);
        MagUninitialize();
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    RegisterHotKey(hwnd, 1, MOD_CONTROL, VK_OEM_PLUS);
    RegisterHotKey(hwnd, 2, MOD_CONTROL, VK_OEM_MINUS);
    SetZoom(zoomFactor);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterHotKey(hwnd, 1);
    UnregisterHotKey(hwnd, 2);
    if (mag) FreeLibrary(mag);

    return 0;
}
