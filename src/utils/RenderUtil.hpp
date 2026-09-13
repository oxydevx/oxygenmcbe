#pragma once

#include <imgui.h>
#include <string>

namespace RenderUtil {

    inline ImU32 Col(unsigned argb) {
        unsigned a = (argb >> 24) & 0xFF;
        unsigned r = (argb >> 16) & 0xFF;
        unsigned g = (argb >> 8) & 0xFF;
        unsigned b = (argb >> 0) & 0xFF;
        return IM_COL32(r, g, b, a);
    }

    inline ImU32 Col(int r, int g, int b, int a = 255) {
        return IM_COL32(r, g, b, a);
    }

    inline ImU32 Col(ImVec4 c) {
        return ImGui::ColorConvertFloat4ToU32(c);
    }

    inline ImDrawList* BgDrawList() {
        return ImGui::GetBackgroundDrawList();
    }

    inline void Rect(float x, float y, float w, float h,
        ImU32 fillColor,
        float rounding = 0.0f) {
        BgDrawList()->AddRectFilled(
            { x, y }, { x + w, y + h },
            fillColor, rounding);
    }

    inline void RectRounded(float x, float y, float w, float h,
        ImU32 fillColor, float rounding, ImDrawFlags flags) {
        BgDrawList()->AddRectFilled(
            { x, y }, { x + w, y + h },
            fillColor, rounding, flags);
    }

    inline void RectOutline(float x, float y, float w, float h,
        ImU32 borderColor,
        float thickness = 1.0f,
        float rounding = 0.0f) {
        BgDrawList()->AddRect(
            { x, y }, { x + w, y + h },
            borderColor, rounding, 0, thickness);
    }

    inline void RectOutlineRounded(float x, float y, float w, float h,
        ImU32 borderColor, float rounding, ImDrawFlags flags, float thickness = 1.0f) {
        BgDrawList()->AddRect(
            { x, y }, { x + w, y + h },
            borderColor, rounding, flags, thickness);
    }

    inline void RectFilled(float x, float y, float w, float h,
        ImU32 fillColor,
        ImU32 borderColor,
        float rounding = 0.0f,
        float thickness = 1.0f) {
        Rect(x, y, w, h, fillColor, rounding);
        RectOutline(x, y, w, h, borderColor, thickness, rounding);
    }

    inline void Text(float x, float y,
        const std::string& str,
        ImU32 color) {
        BgDrawList()->AddText({ x, y }, color, str.c_str());
    }

    inline void TextCentered(float x, float y, float w, float h,
        const std::string& str,
        ImU32 color) {
        ImVec2 sz = ImGui::CalcTextSize(str.c_str());
        float tx = x + (w - sz.x) * 0.5f;
        float ty = y + (h - sz.y) * 0.5f;
        BgDrawList()->AddText({ tx, ty }, color, str.c_str());
    }

    inline void TextLeft(float x, float y, float w, float h,
        const std::string& str,
        ImU32 color,
        float paddingLeft = 6.0f) {
        ImVec2 sz = ImGui::CalcTextSize(str.c_str());
        float ty = y + (h - sz.y) * 0.5f;
        BgDrawList()->AddText({ x + paddingLeft, ty }, color, str.c_str());
    }

    inline void Line(float x1, float y1, float x2, float y2,
        ImU32 color, float thickness = 1.0f) {
        BgDrawList()->AddLine({ x1, y1 }, { x2, y2 }, color, thickness);
    }

    inline ImVec2 MousePos() {
        return ImGui::GetIO().MousePos;
    }

    inline bool IsHovered(float x, float y, float w, float h) {
        ImVec2 m = MousePos();
        return m.x >= x && m.x <= x + w &&
            m.y >= y && m.y <= y + h;
    }

    inline bool IsClicked(float x, float y, float w, float h) {
        return IsHovered(x, y, w, h) &&
            ImGui::GetIO().MouseClicked[0];
    }

    inline float ScreenWidth() { return ImGui::GetIO().DisplaySize.x; }
    inline float ScreenHeight() { return ImGui::GetIO().DisplaySize.y; }

    inline void DrawGlow(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float size = 15.0f, float intensity = 0.5f, float rounding = 0.0f) {
        int r = (color >> 0) & 0xFF;
        int g = (color >> 8) & 0xFF;
        int b = (color >> 16) & 0xFF;
        int a = (color >> 24) & 0xFF;

        for (int i = 1; i <= size; i++) {
            float alphaFactor = 1.0f - ((float)i / size);
            alphaFactor *= alphaFactor;
            int alpha = (int)((float)a * alphaFactor * intensity * 0.4f);

            if (alpha <= 0) continue;

            drawList->AddRect(
                { pMin.x - i, pMin.y - i },
                { pMax.x + i, pMax.y + i },
                IM_COL32(r, g, b, alpha),
                rounding + i, 0, 1.0f
            );
        }
    }

}