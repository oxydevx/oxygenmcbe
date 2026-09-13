#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <imgui.h>

struct ImFont;

namespace Resources {
    std::string GetDllDirectory();
    std::string GetFontsDirectory();
    bool LoadFonts();
    bool LoadPngTexture(const std::string& relativeFileName, ImTextureData& outTexture);

    extern std::vector<ImFont*> Fonts;
    extern std::vector<std::string> FontNames;
}
