#include "D2DHook.hpp"
#include <imgui.h>

namespace Hooks::D2D {
    void DrawMotionBlur(float intensity) {

        auto drawList = ImGui::GetBackgroundDrawList();
        ImVec2 screenSize = ImGui::GetIO().DisplaySize;

        float alpha = intensity * 0.1f;

        drawList->AddRectFilled(ImVec2(0, 0), screenSize, IM_COL32(0, 0, 0, (int)(alpha * 255)));
    }
}