#include "Notifications.hpp"
#include "../../GuiTheme.hpp"
#include "../../../utils/Resources.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>

Notifications* g_notifications = nullptr;


static float EaseOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    float s = 1.0f - t;
    return 1.0f - s * s * s;
}

Notifications::Notifications() : Feature("Notifications", Category::Visual) {
    this->Description  = "In-game toasts: module toggles, cheat events and more";
    this->Enabled      = true;   
    this->CallAllTime  = true;   
    this->HideFromList = true;   
    this->Keybind      = 0;
    this->AddSlider("Notify life", &m_dur, 1.5f, 10.0f, "%.1f s");
    g_notifications = this;
}

void Notifications::Push(const std::string& title, const std::string& message, Type type) {
    if (!g_notifications) return;
    if (!g_notifications->Enabled) return; 
    std::lock_guard<std::mutex> lk(g_notifications->m_mutex);
    const float dur = std::clamp(g_notifications->m_dur, 1.5f, 10.0f);
    g_notifications->m_items.push_back({ title, message, type, dur, dur, 0.0f, 0.0f, 0.0f, 0.0f, true });
    if (g_notifications->m_items.size() > MAX_ITEMS)
        g_notifications->m_items.pop_front();
}

void Notifications::OnRender() {
    auto& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) return;

    const float dt = std::min(io.DeltaTime, 0.1f);
    const auto& a = GuiTheme::GetAccent();
    const float gAlpha = GuiTheme::Alpha();

    const float margin    = 14.0f;
    const float gap       = 8.0f;
    const float width     = 272.0f;
    const float rowH      = 64.0f;   
    const float rowStep   = rowH + gap;
    const float pad       = 12.0f;
    const float radius    = 10.0f;
    const float slideDist = 46.0f;   
    const float appearDur = 0.35f;   
    const float exitDur   = 0.35f;   
    const float aLerp     = std::min(1.0f, dt * 7.0f);
    const float yLerp     = std::min(1.0f, dt * 15.0f);
    const float xLerp     = std::min(1.0f, dt * 9.0f);

    ImFont* gsFont = nullptr;
    for (size_t i = 0; i < Resources::FontNames.size(); i++) {
        if (Resources::FontNames[i] == "GoogleSans") {
            gsFont = Resources::Fonts[i];
            break;
        }
    }
    ImFont* font = gsFont ? gsFont : ImGui::GetFont();
    const float baseSize = ImGui::GetStyle().FontSizeBase;
    const float titleFont = baseSize * 1.35f;
    const float msgFont   = baseSize * 1.05f;

    
    const float bottomAnchor = io.DisplaySize.y - margin;
    std::deque<Item> live;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        const size_t n = m_items.size();
        for (size_t i = 0; i < n; i++) {
            Item& it = m_items[i];
            it.age += dt;
            it.life -= dt;
            const float k = (float)(n - 1 - i);          
            const float slotTop = bottomAnchor - (k + 1.0f) * rowStep + gap;
            if (it.snapY) { it.y = slotTop; it.snapY = false; }
            else          { it.y += (slotTop - it.y) * yLerp; }
            it.x += (0.0f - it.x) * xLerp;
            const float tgt = (it.life > 0.0f) ? 1.0f : 0.0f;
            it.alpha += (tgt - it.alpha) * aLerp;
        }
        for (auto it = m_items.begin(); it != m_items.end();) {
            if (it->life <= 0.0f && it->alpha <= 0.01f)
                it = m_items.erase(it);
            else
                ++it;
        }
        live = m_items;
    }
    if (live.empty()) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    const ImVec4 cSurface = ImGui::ColorConvertU32ToFloat4(a.SURFACE);
    const ImVec4 cOutline = ImGui::ColorConvertU32ToFloat4(a.OUTLINE);

    for (const Item& it : live) {
        const float app     = EaseOut(it.age / appearDur);
        const float exit    = std::clamp(it.life / exitDur, 0.0f, 1.0f);
        const float vis     = app * exit * it.alpha * gAlpha;
        if (vis <= 0.002f) continue;

        
        const float xOff = (1.0f - app) * slideDist + (1.0f - exit) * slideDist * 0.5f + it.x;

        const float cx = io.DisplaySize.x - margin - width * 0.5f + xOff;
        const float cy = it.y + rowH * 0.5f;
        const float s  = 0.94f + 0.06f * app;   
        const float hw = width * 0.5f * s;
        const float hh = rowH * 0.5f * s;
        const ImVec2 pmin(cx - hw, cy - hh);
        const ImVec2 pmax(cx + hw, cy + hh);

        
        for (int i = 2; i >= 1; --i) {
            const float off = (float)i * 1.8f;
            const unsigned char sa = (unsigned char)(0x16 * (i / 2.0f + 0.4f) * vis);
            dl->AddRectFilled({ pmin.x, pmin.y + off }, { pmax.x, pmax.y + off },
                IM_COL32(0, 0, 0, sa), radius, ImDrawFlags_RoundCornersAll);
        }

        
        dl->AddRectFilled(pmin, pmax,
            ImGui::ColorConvertFloat4ToU32(ImVec4(cSurface.x, cSurface.y, cSurface.z, cSurface.w * vis)), radius);
        dl->AddRect(pmin, pmax,
            ImGui::ColorConvertFloat4ToU32(ImVec4(cOutline.x, cOutline.y, cOutline.z, cOutline.w * vis)),
            radius, ImDrawFlags_RoundCornersAll, 1.0f);

        
        const ImVec4 cAccent = ImGui::ColorConvertU32ToFloat4(a.PRIMARY);
        const float tx = pmin.x + pad;
        const float ty = pmin.y + 9.0f;
        dl->AddText(font, titleFont, ImVec2(tx, ty),
            ImGui::ColorConvertFloat4ToU32(ImVec4(cAccent.x, cAccent.y, cAccent.z, cAccent.w * vis)), it.title.c_str());
        if (!it.message.empty()) {
            const ImVec4 cDim = ImGui::ColorConvertU32ToFloat4(0xFFD6D0CC);
            dl->AddText(font, msgFont, ImVec2(tx, ty + titleFont + 2.0f),
                ImGui::ColorConvertFloat4ToU32(ImVec4(cDim.x, cDim.y, cDim.z, cDim.w * (vis * 0.92f))),
                it.message.c_str());
        }

        
        
        const float frac = std::clamp(it.life / it.maxLife, 0.0f, 1.0f);
        const float bx = pmin.x + pad;
        const float by = pmax.y - 8.0f;
        const float bw = (width - pad * 2.0f) * s;
        const float bh = 4.0f;
        const float barRound = bh * 0.5f;
        dl->AddRectFilled({ bx, by }, { bx + bw, by + bh },
            ImGui::ColorConvertFloat4ToU32(ImVec4(cOutline.x, cOutline.y, cOutline.z, cOutline.w * vis * 0.55f)),
            barRound, ImDrawFlags_RoundCornersAll);
        if (frac > 0.001f) {
            const float fw = bw * frac;
            const ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(cAccent.x, cAccent.y, cAccent.z, cAccent.w * (vis * 0.95f)));
            dl->AddRectFilled({ bx, by }, { bx + fw, by + bh }, fill, barRound, ImDrawFlags_RoundCornersAll);
            
            dl->AddCircleFilled(ImVec2(bx + fw, by + barRound), barRound, fill);
        }
    }
}