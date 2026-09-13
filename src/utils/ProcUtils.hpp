#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace ProcUtils {

int GetModuleCount();

HWND GetMinecraftWindow();

std::vector<std::wstring> GetModulePaths();

std::string GetVersion();

}
