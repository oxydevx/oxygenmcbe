#include "ProcUtils.hpp"
#include "logger.hpp"
#include <tlhelp32.h>
#include <map>
#include <vector>

namespace ProcUtils {

int GetModuleCount() {
    HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (hModuleSnap == INVALID_HANDLE_VALUE)
        return 0;

    MODULEENTRY32 me32;
    me32.dwSize = sizeof(MODULEENTRY32);

    if (!Module32First(hModuleSnap, &me32)) {
        CloseHandle(hModuleSnap);
        return 0;
    }

    int count = 1;
    while (Module32Next(hModuleSnap, &me32))
        count++;

    CloseHandle(hModuleSnap);
    return count;
}

HWND GetMinecraftWindow() {
    static HWND window = nullptr;
    if (!window) {
        std::map<HWND, std::string> titles;
        auto callback = [](HWND hwnd, LPARAM lParam) -> BOOL {
            char title[256];
            GetWindowTextA(hwnd, title, sizeof(title));
            reinterpret_cast<std::map<HWND, std::string>*>(lParam)->insert({ hwnd, std::string(title) });
            return TRUE;
        };

        EnumWindows(callback, reinterpret_cast<LPARAM>(&titles));

        for (auto& [hwnd, title] : titles) {
            if (title.find("Minecraft") != std::string::npos) {
                window = hwnd;
                break;
            }
        }

        Logger::InfoTag("GetMinecraftWindow", "Minecraft window handle: 0x%llX", reinterpret_cast<unsigned long long>(window));
    }

    return window;
}

std::vector<std::wstring> GetModulePaths() {
    HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (hModuleSnap == INVALID_HANDLE_VALUE)
        return {};

    MODULEENTRY32 me32;
    me32.dwSize = sizeof(MODULEENTRY32);

    if (!Module32First(hModuleSnap, &me32)) {
        CloseHandle(hModuleSnap);
        return {};
    }

    std::vector<std::wstring> modulePaths;

    do {
        int len = MultiByteToWideChar(CP_ACP, 0, me32.szExePath, -1, nullptr, 0);
        std::wstring wpath(len - 1, L'\0');
        MultiByteToWideChar(CP_ACP, 0, me32.szExePath, -1, &wpath[0], len);
        modulePaths.push_back(std::move(wpath));
    } while (Module32Next(hModuleSnap, &me32));

    CloseHandle(hModuleSnap);
    return modulePaths;
}

std::string GetVersion() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);

    DWORD dwDummy;
    DWORD dwFVISize = GetFileVersionInfoSizeA(path, &dwDummy);
    if (dwFVISize == 0)
        return "Unknown";

    std::vector<char> versionInfo(dwFVISize);
    if (!GetFileVersionInfoA(path, 0, dwFVISize, versionInfo.data()))
        return "Unknown";

    VS_FIXEDFILEINFO* pFileInfo;
    UINT uLen;
    if (!VerQueryValueA(versionInfo.data(), "\\", reinterpret_cast<void**>(&pFileInfo), &uLen))
        return "Unknown";

    char buf[32];
    snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
        HIWORD(pFileInfo->dwFileVersionMS),
        LOWORD(pFileInfo->dwFileVersionMS),
        HIWORD(pFileInfo->dwFileVersionLS),
        LOWORD(pFileInfo->dwFileVersionLS));
    return buf;
}

}
