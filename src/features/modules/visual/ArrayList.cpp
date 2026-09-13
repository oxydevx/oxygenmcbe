#include "ArrayList.hpp"
#include "../../GuiTheme.hpp"
#include "../../ModuleManager.hpp"
#include "../../utils/Resources.hpp"
#include "imgui.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>

ArrayList::ArrayList() : Feature("ArrayList", Category::Visual) {
    this->Description  = "Shows enabled modules in the top-right corner";
    this->Enabled      = false;
    this->CallAllTime  = true;  
    this->IsBackground = false;
    this->Keybind      = 0;
}

void ArrayList::OnRender() {
    auto& io = ImGui::GetIO();
    const auto& a = GuiTheme::GetAccent();
    const float gAlpha = GuiTheme::Alpha();

    const float margin    = 10.0f;
    const float fontScale = 1.6f;
    const float slideDist = 40.0f;
    const float padX      = 11.0f;
    const float padY      = 5.0f;
    const float round     = 10.0f;

    
    ImFont* gsFont = nullptr;
    for (size_t i = 0; i < Resources::FontNames.size(); i++) {
        if (Resources::FontNames[i] == "GoogleSans") {
            gsFont = Resources::Fonts[i];
            break;
        }
    }
    ImFont* font = gsFont ? gsFont : ImGui::GetFont();
    const float baseSize = ImGui::GetStyle().FontSizeBase;
    const float fontSize = baseSize * fontScale;
    const float lineH    = fontSize * 1.55f;

    
    std::vector<std::string> targets;
    for (auto& sp : ModuleManager::FeatureList) {
        Feature* f = sp.get();
        if (f->Enabled && !f->HideFromList)
            targets.push_back(f->Name);
    }
    
    if (!this->Enabled)
        targets.clear();

    std::stable_sort(targets.begin(), targets.end(),
        [](const std::string& x, const std::string& y) { return x.length() > y.length(); });

    std::unordered_map<std::string, int> targetIndex;
    for (size_t i = 0; i < targets.size(); i++)
        targetIndex[targets[i]] = (int)i;

    
    const float aLerp = std::min(1.0f, io.DeltaTime * 10.0f);
    const float yLerp = std::min(1.0f, io.DeltaTime * 16.0f);
    for (auto& kv : m_rowAnim) {
        auto it = targetIndex.find(kv.first);
        const bool vis = (it != targetIndex.end());
        const float tgtA = vis ? 1.0f : 0.0f;
        kv.second.alpha += (tgtA - kv.second.alpha) * aLerp;
        if (kv.second.alpha < 0.003f) kv.second.alpha = 0.0f;
        if (vis)
            kv.second.y += ((float)it->second * lineH - kv.second.y) * yLerp;
    }
    for (auto& n : targets)
        if (m_rowAnim.find(n) == m_rowAnim.end())
            m_rowAnim[n] = { 0.0f, (float)targetIndex[n] * lineH };

    for (auto it = m_rowAnim.begin(); it != m_rowAnim.end();) {
        if (it->second.alpha <= 0.0f && targetIndex.find(it->first) == targetIndex.end())
            it = m_rowAnim.erase(it);
        else
            ++it;
    }

    if (m_rowAnim.empty())
        return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    const ImVec4 bg      = ImGui::ColorConvertU32ToFloat4(a.SURFACE);
    const ImVec4 text    = ImGui::ColorConvertU32ToFloat4(a.ON_SURFACE);
    const ImVec4 outline = ImGui::ColorConvertU32ToFloat4(a.OUTLINE);

    
    auto ease = [](float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    };

    for (auto& kv : m_rowAnim) {
        const std::string& n = kv.first;
        ArrayListAnim& st = kv.second;
        if (st.alpha <= 0.001f) continue;

        const float e     = ease(st.alpha);                 
        const float textW = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, n.c_str()).x;
        const float fullW = textW + padX * 2.0f;
        const float h     = fontSize + padY * 2.0f;
        const float s     = 0.92f + 0.08f * e;              
        const float cx    = io.DisplaySize.x - margin - fullW * 0.5f + (1.0f - e) * slideDist;
        const float cy    = margin + st.y + h * 0.5f;

        const ImVec2 pmin(cx - (fullW * 0.5f) * s, cy - (h * 0.5f) * s);
        const ImVec2 pmax(cx + (fullW * 0.5f) * s, cy + (h * 0.5f) * s);

        
        for (int i = 3; i >= 1; --i) {
            float off = (float)i * 1.5f;
            unsigned char sa = (unsigned char)(0x12 * (i / 3.0f + 0.3f) * gAlpha * e);
            dl->AddRectFilled({ pmin.x, pmin.y + off }, { pmax.x, pmax.y + off },
                IM_COL32(0, 0, 0, sa), round, ImDrawFlags_RoundCornersAll);
        }

        
        dl->AddRectFilled(pmin, pmax,
            ImGui::ColorConvertFloat4ToU32(ImVec4(bg.x, bg.y, bg.z, bg.w * gAlpha * e)), round);
        dl->AddRect(pmin, pmax,
            ImGui::ColorConvertFloat4ToU32(ImVec4(outline.x, outline.y, outline.z, outline.w * gAlpha * e)),
            round, ImDrawFlags_RoundCornersAll, 1.0f);

        
        dl->AddText(font, fontSize, ImVec2(pmin.x + padX, cy - fontSize * 0.5f),
            ImGui::ColorConvertFloat4ToU32(ImVec4(text.x, text.y, text.z, text.w * gAlpha * e)), n.c_str());
    }
}
