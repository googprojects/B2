#include <Windows.h>
#include <dwmapi.h>
#include <Magnification.h>
#include <iostream>
#include <vector>
#include <string>

// Custom min and max functions to avoid std::
template <typename T>
T my_min(T a, T b) {
    return (a < b) ? a : b;
}

template <typename T>
T my_max(T a, T b) {
    return (a > b) ? a : b;
}

#pragma comment(lib, "Magnification.lib")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "User32.lib")

static float zoomFactor = 1.0f;
HWND targetHwnd = nullptr;
HWND magWindow = nullptr;

// Make the process DPI aware
void SetDPIAwareness() {
    if (HMODULE user32 = LoadLibraryW(L"user32.dll")) {
        auto pSetProcessDPIAware = (decltype(&SetProcessDPIAware))GetProcAddress(user32, "SetProcessDPIAware");
        if (pSetProcessDPIAware) pSetProcessDPIAware();
    }
}

// Get client rect in screen coordinates
RECT GetClientRectScreen(HWND hwnd) {
    RECT client;
    GetClientRect(hwnd, &client);
    
    POINT pt = { client.left, client.top };
    ClientToScreen(hwnd, &pt);
    client.left = pt.x;
    client.top = pt.y;
    
    pt = { client.right, client.bottom };
    ClientToScreen(hwnd, &pt);
    client.right = pt.x;
    client.bottom = pt.y;
    
    return client;
}

BOOL UpdateMagnifier() {
    if (!targetHwnd || !IsWindow(targetHwnd)) return FALSE;

    RECT clientRect = GetClientRectScreen(targetHwnd); // Get client rect in screen coordinates

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    int clientW = clientRect.right - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;

    // Calculate the maximum zoom that fits the client area on screen
    float maxZoomX = (float)screenW / clientW;
    float maxZoomY = (float)screenH / clientH;
    float maxZoom = my_min(maxZoomX, maxZoomY); // Use custom my_min
    
    // Apply the current zoom factor (clamped to max zoom)
    float zoom = my_min(zoomFactor, maxZoom); // Use custom my_min

    // Calculate the source rectangle for magnification
    // This will be the client area scaled down by the zoom factor
    float srcWidth = (float)clientW / zoom;
    float srcHeight = (float)clientH / zoom;

    // The MagSetFullscreenTransform function expects the top-left corner
    // of the source rectangle in screen coordinates.
    // So, we just use the top-left of our clientRect.
    int srcX = clientRect.left;
    int srcY = clientRect.top;

    // Set the magnification transform
    if (!MagSetFullscreenTransform(zoom, srcX, srcY)) {
        return FALSE;
    }

    // Force redraw
    InvalidateRect(magWindow, NULL, TRUE);
    UpdateWindow(magWindow);

    return TRUE;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindowTextLengthW(hwnd) == 0) return TRUE;

    DWORD styles = GetWindowLong(hwnd, GWL_STYLE);
    if (!(styles & WS_VISIBLE)) return TRUE;

    // Exclude the console window itself (this application)
    // and the magnifier window we create.
    if (hwnd == GetConsoleWindow() || hwnd == magWindow) return TRUE;

    wchar_t title[256];
    GetWindowTextW(hwnd, title, 256);

    auto windows = reinterpret_cast<std::vector<std::pair<HWND, std::wstring>>*>(lParam);
    windows->emplace_back(hwnd, title);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_HOTKEY:
        if (wParam == 1) { // Zoom in
            zoomFactor += 0.1f;
            UpdateMagnifier();
        }
        else if (wParam == 2) { // Zoom out
            zoomFactor = my_max(1.0f, zoomFactor - 0.1f); // Use custom my_max
            UpdateMagnifier();
        }
        return 0;

    case WM_TIMER:
        UpdateMagnifier();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int main() {
    SetDPIAwareness();

    if (!MagInitialize()) {
        std::cerr << "Magnification API initialization failed.\n";
        return 1;
    }

    // Create our transparent window (before enumerating to exclude it)
    const wchar_t CLASS_NAME[] = L"MagWinClass";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    magWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        CLASS_NAME, 
        L"Magnifier", 
        WS_POPUP,
        0, 0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        nullptr, 
        nullptr,
        GetModuleHandle(nullptr), 
        nullptr
    );

    // Set layered window attributes for transparency
    SetLayeredWindowAttributes(magWindow, 0, 255, LWA_ALPHA);
    ShowWindow(magWindow, SW_SHOW);

    // Get all visible windows
    std::vector<std::pair<HWND, std::wstring>> windows;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&windows));

    // Display window list
    std::wcout << L"Select a window to magnify:\n";
    for (size_t i = 0; i < windows.size(); ++i) {
        std::wcout << i << L": " << windows[i].second << L"\n";
    }

    // Get user selection
    int choice;
    std::wcout << L"\nEnter window number: ";
    std::wcin >> choice;

    if (std::wcin.fail() || choice < 0 || choice >= windows.size()) {
        std::wcout << L"Invalid selection. Exiting.\n";
        DestroyWindow(magWindow); // Clean up magnifier window
        MagUninitialize();
        return 1;
    }

    targetHwnd = windows[choice].first;

    // Register hotkeys
    RegisterHotKey(magWindow, 1, MOD_CONTROL, VK_OEM_PLUS);  // Ctrl++
    RegisterHotKey(magWindow, 2, MOD_CONTROL, VK_OEM_MINUS); // Ctrl+-
    RegisterHotKey(magWindow, 2, MOD_CONTROL, VK_SUBTRACT);  // Ctrl+- (numpad)

    // Initial update and timer for periodic refreshes
    UpdateMagnifier();
    SetTimer(magWindow, 1, 100, nullptr); // Refresh every 100ms

    std::cout << "Magnifier running. Use Ctrl + / Ctrl - to zoom. Press ESC to exit.\n";

    // Main message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
            break;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    KillTimer(magWindow, 1);
    UnregisterHotKey(magWindow, 1);
    UnregisterHotKey(magWindow, 2);
    DestroyWindow(magWindow);
    MagUninitialize();
    return 0;
}
