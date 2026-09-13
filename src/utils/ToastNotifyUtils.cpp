#include "ToastNotifyUtils.hpp"
#include <string>
#include <shellapi.h>
#include <commctrl.h>

namespace ToastNotifyUtils {

static const UINT WM_TRAY_CALLBACK = WM_APP + 1;

static HWND FindMinecraftWindow() {
    HWND result = nullptr;
    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        char title[256];
        if (GetWindowTextA(hwnd, title, sizeof(title)) && strstr(title, "Minecraft")) {
            *reinterpret_cast<HWND*>(lp) = hwnd;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&result));
    return result;
}

static LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAY_CALLBACK) {
        switch (lParam) {
        case NIN_BALLOONUSERCLICK: {
            HWND mcWnd = FindMinecraftWindow();
            if (mcWnd) {
                ShowWindow(mcWnd, SW_RESTORE);
                SetForegroundWindow(mcWnd);
            }
            PostQuitMessage(0);
            return 0;
        }
        case NIN_BALLOONTIMEOUT:
            PostQuitMessage(0);
            return 0;
        }
    }
    else if (msg == WM_DESTROY) {
        PostQuitMessage(0);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void ShowNotification(const std::string& title, const std::string& message) {
    const wchar_t CLASS_NAME[] = L"OxygenToastWindow";

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSW wc = {};
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    HWND hWnd = CreateWindowExW(0, CLASS_NAME, L"", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, hInst, nullptr);
    if (!hWnd) return;

    std::wstring wTitle(title.begin(), title.end());
    std::wstring wMessage(message.begin(), message.end());

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_INFO | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAY_CALLBACK;
    nid.uTimeout = 3000;
    nid.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    wcscpy_s(nid.szInfoTitle, wTitle.c_str());
    wcscpy_s(nid.szInfo, wMessage.c_str());

    Shell_NotifyIconW(NIM_ADD, &nid);

    MSG msg;
    DWORD start = GetTickCount();
    while (GetTickCount() - start < 5000) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            Sleep(10);
        }
    }

    Shell_NotifyIconW(NIM_DELETE, &nid);
    DestroyWindow(hWnd);
    UnregisterClassW(CLASS_NAME, hInst);
}

}
