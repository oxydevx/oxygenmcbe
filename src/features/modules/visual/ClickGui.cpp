#include "ClickGui.hpp"
#include "../../ModuleManager.hpp"
#include "../../../utils/logger.hpp"
#include "../../../utils/Resources.hpp"
#include "../../../hooks/Keyboard.hpp"
#include "../misc/Test.hpp"
#include "Notifications.hpp"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

bool ClickGui::s_isListening = false;
bool ClickGui::s_textEditing = false;
bool ClickGui::s_guiOpen = false;
float ClickGui::s_animSpeed = 1.0f;
const char* ClickGui::s_dbgSection = "init";
char ClickGui::s_dbgBuf[160] = {};

static float EaseOut(float t) {
    float s = 1.0f - t;
    return 1.0f - s * s * s;
}



static float EaseInOutCubic(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t < 0.5f
        ? 4.0f * t * t * t
        : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
}

std::string ClickGui::CategoryAssetName(Category cat) {
    switch (cat) {
    case Category::Combat:   return "combat.png";
    case Category::Visual:   return "visual.png";
    case Category::Movement: return "movement.png";
    case Category::Player:   return "player.png";
    case Category::Misc:     return "misc.png";
    default:                 return "misc.png";
    }
}

void ClickGui::LoadCategoryIcons() {
    
    if (ImGui::GetCurrentContext() != nullptr) {
        for (auto& [cat, tex] : m_categoryIcons) {
            if (tex) {
                ImGui::UnregisterUserTexture(tex.get());
            }
        }
        if (m_arrowTex) {
            ImGui::UnregisterUserTexture(m_arrowTex.get());
            m_arrowTex = nullptr;
        }
    }
    m_categoryIcons.clear();

    for (const auto& panel : m_panels) {
        auto tex = std::make_unique<ImTextureData>();
        if (Resources::LoadPngTexture(CategoryAssetName(panel.cat), *tex)) {
            tex->RefCount = 1;
            if (ImGui::GetCurrentContext() != nullptr) {
                ImGui::RegisterUserTexture(tex.get());
                Logger::InfoTag("LoadCategoryIcons", "registered texture for %s (Status=%d, TexID=%p)", 
                    CategoryAssetName(panel.cat).c_str(), (int)tex->Status, (void*)tex->TexID);
            }
            m_categoryIcons[panel.cat] = std::move(tex);
        } else {
            Logger::WarnTag("LoadCategoryIcons", "failed to load PNG for %s", CategoryAssetName(panel.cat).c_str());
        }
    }

    
    m_arrowTex = std::make_unique<ImTextureData>();
    if (Resources::LoadPngTexture("arrowdown.png", *m_arrowTex)) {
        m_arrowTex->RefCount = 1;
        if (ImGui::GetCurrentContext() != nullptr)
            ImGui::RegisterUserTexture(m_arrowTex.get());
    }
    else {
        Logger::WarnTag("LoadCategoryIcons", "failed to load PNG arrowdown.png");
        m_arrowTex = nullptr;
    }

    
    m_framesSinceIconLoad = 0.0f;
}

     static unsigned BlendCol(unsigned a, unsigned b, float t);

     void ClickGui::SyncTheme() {
         const GuiTheme::AccentColors& a = GuiTheme::GetAccent();
         C_PRIMARY             = a.PRIMARY;
         C_PRIMARY_CONTAINER   = a.PRIMARY_CONTAINER;
         C_ON_PRIMARY          = a.ON_PRIMARY;
         C_ON_PRIMARY_CONTAINER = a.ON_PRIMARY_CONTAINER;
         C_KEYBIND_ACTIVE      = a.KEYBIND_ACTIVE;
         C_MOD_FLASH_ON        = a.MOD_FLASH_ON;

         C_SURFACE          = a.SURFACE;
         C_SURFACE_2        = BlendCol(a.SURFACE, a.ON_SURFACE, 0.06f);
         C_SURFACE_3        = BlendCol(a.SURFACE, a.ON_SURFACE, 0.12f);
         C_ON_SURFACE       = a.ON_SURFACE;
         C_ON_SURFACE_DIM   = BlendCol(a.ON_SURFACE, a.SURFACE, 0.25f);
         C_OUTLINE          = a.OUTLINE;
         C_TRACK            = BlendCol(a.OUTLINE, a.SURFACE, 0.20f);
         C_SETTINGS         = BlendCol(a.SURFACE, 0xFF000000, 0.25f);
     }

     void ClickGui::DrawShadow(ImDrawList* dl, ImVec2 p, float w, float h, float rad) const {
        
        for (int i = 4; i >= 1; --i) {
            float off = (float)i * 1.6f;
            unsigned a = (unsigned)(0x0A * (i / 4.0f + 0.4f));
            dl->AddRectFilled(
                { p.x, p.y + off }, { p.x + w, p.y + h + off },
                IM_COL32(0, 0, 0, (unsigned char)(a * m_guiAlpha)),
                rad, ImDrawFlags_RoundCornersAll);
        }
    }

ClickGui::ClickGui() : Feature("ClickGui", Category::Visual) {
    this->Description = "Custom module GUI";
    this->Enabled = false;
    this->CallAllTime = true;
    this->Keybind = VK_TAB;
    this->Visibility = true;
    this->IsBackground = false;

    SyncTheme();

    const float startX = 60.0f;
    const float startY = 80.0f;
    const float stepX = COL_W + 5.0f;

    m_panels = {
        { Category::Combat,   "Combat",   startX + stepX * 0, startY },
        { Category::Visual,   "Visual",   startX + stepX * 1, startY },
        { Category::Movement, "Movement", startX + stepX * 2, startY },
        { Category::Player,   "Player",   startX + stepX * 3, startY },
        { Category::Misc,     "Misc",     startX + stepX * 4, startY },
    };

    
    

    
    AddSlider("Anim Speed", &m_animSpeed, 0.0f, 3.0f, "%.2f");
}

void ClickGui::OnEnabled() {
    s_guiOpen = true;
    ImGui::GetIO().MouseDrawCursor = true;
    if (!m_iconsLoaded) {
        LoadCategoryIcons();
        m_iconsLoaded = true;
    }
    
    
}

void ClickGui::OnDisabled() {
    s_guiOpen = false;
    ImGui::GetIO().MouseDrawCursor = false;
    for (auto& p : m_panels) p.drag = false;
    m_sliderDrag.active = false;
    m_keybindListen.active = false;
    m_keybindListen.feature = nullptr;
    s_isListening = false;
    
}

std::string ClickGui::KeyName(int vk) {
    if (vk <= 0) return "None";
    switch (vk) {
    case VK_LBUTTON:  return "LMB";
    case VK_RBUTTON:  return "RMB";
    case VK_MBUTTON:  return "MMB";
    case VK_BACK:     return "Backspace";
    case VK_TAB:      return "Tab";
    case VK_RETURN:   return "Enter";
    case VK_SHIFT:    return "Shift";
    case VK_CONTROL:  return "Ctrl";
    case VK_MENU:     return "Alt";
    case VK_PAUSE:    return "Pause";
    case VK_CAPITAL:  return "CapsLk";
    case VK_ESCAPE:   return "Esc";
    case VK_SPACE:    return "Space";
    case VK_PRIOR:    return "PgUp";
    case VK_NEXT:     return "PgDn";
    case VK_END:      return "End";
    case VK_HOME:     return "Home";
    case VK_LEFT:     return "Left";
    case VK_UP:       return "Up";
    case VK_RIGHT:    return "Right";
    case VK_DOWN:     return "Down";
    case VK_INSERT:   return "Ins";
    case VK_DELETE:   return "Del";
    case VK_LWIN:     return "LWin";
    case VK_RWIN:     return "RWin";
    case VK_NUMPAD0:  return "Num0";
    case VK_NUMPAD1:  return "Num1";
    case VK_NUMPAD2:  return "Num2";
    case VK_NUMPAD3:  return "Num3";
    case VK_NUMPAD4:  return "Num4";
    case VK_NUMPAD5:  return "Num5";
    case VK_NUMPAD6:  return "Num6";
    case VK_NUMPAD7:  return "Num7";
    case VK_NUMPAD8:  return "Num8";
    case VK_NUMPAD9:  return "Num9";
    case VK_MULTIPLY: return "Num*";
    case VK_ADD:      return "Num+";
    case VK_SUBTRACT: return "Num-";
    case VK_DECIMAL:  return "Num.";
    case VK_DIVIDE:   return "Num/";
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        return "F" + std::to_string(vk - VK_F1 + 1);
    }
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
        char buf[2] = { static_cast<char>(vk), '\0' };
        return buf;
    }
    char buf[8];
    std::snprintf(buf, sizeof(buf), "0x%02X", vk);
    return buf;
}

int ClickGui::SettingRows(const Feature* f) const {
    int count = 0;
    for (auto& s : f->Settings) {
        if (s->visibility()) count++;
    }
    return count + 1;
}

float ClickGui::ExpandedHeight(const Feature* f, float anim) const {
    int dataRows = 0;
    for (auto& s : f->Settings) {
        if (s->visibility()) dataRows++;
    }
    float full = dataRows * SETTING_H + SETTINGS_PAD * 2.0f + KEYBIND_GAP + KEYBIND_H;
    return full * std::clamp(anim, 0.0f, 1.0f);
}

float ClickGui::PanelContentHeight(const Panel& p) const {
    float h = PANEL_PAD_TOP;
    auto feats = GetFeatures(p.cat);
    for (size_t i = 0; i < feats.size(); ++i) {
        h += MODULE_H;
        float anim = 0.0f;
        auto it = m_expandAnim.find(feats[i]->Name);
        if (it != m_expandAnim.end()) anim = it->second;
        h += ExpandedHeight(feats[i], anim);
        if (i != feats.size() - 1) h += MODULE_PAD;
    }
    h += PANEL_PAD_BOTTOM;
    return h;
}

float ClickGui::PanelHeight(const Panel& p) const {
    float contentH = PanelContentHeight(p);
    float eased = EaseOut(std::clamp(p.collapseAnim, 0.0f, 1.0f));
    return HEADER_H + contentH * eased;
}

void ClickGui::UpdateAnimations(float dt) {
    m_framesSinceIconLoad += dt;  

    
    
    const float spd = m_animSpeed;
    if (spd <= 0.0f) {
        
        m_animT    = Enabled ? 1.0f : 0.0f;
        m_guiAlpha = m_animT;
        m_guiScale = 0.85f + 0.15f * m_animT;
        for (auto& p : m_panels) p.collapseAnim = p.collapsed ? 0.0f : 1.0f;
        for (auto& [name, anim] : m_expandAnim) {
            bool exp = m_expanded.count(name) && m_expanded.at(name);
            anim = exp ? 1.0f : 0.0f;
        }
        for (auto& f : ModuleManager::FeatureList) {
            m_toggleAnim[f->Name] = f->Enabled ? 1.0f : 0.0f;
            for (auto& s : f->Settings) {
                if (auto cb = std::dynamic_pointer_cast<CheckboxSetting>(s))
                    m_checkAnim[f->Name + "::" + cb->label] = *cb->value ? 1.0f : 0.0f;
            }
        }
        for (auto& [name, fs] : m_flash) fs.timer = 0.0f;
        m_tooltipHover = (m_tooltipFeature != nullptr) ? TOOLTIP_HOLD : 0.0f;
        m_tooltipAnim  = (m_tooltipHover >= TOOLTIP_HOLD) ? 1.0f : 0.0f;
        return;
    }

    const float openDur   = OPEN_DURATION   / spd;
    const float expandDur = EXPAND_DURATION / spd;
    const float animSpeed = ANIM_SPEED      * spd;

    
    
    
    const float dir = Enabled ? 1.0f : -1.0f;
    m_animT += dir * dt / openDur;
    m_animT = std::clamp(m_animT, 0.0f, 1.0f);
    float e = EaseInOutCubic(m_animT);
    m_guiAlpha = e;
    m_guiScale = 0.85f + 0.15f * e;

    for (auto& p : m_panels) {
        float target = p.collapsed ? 0.0f : 1.0f;
        p.collapseAnim += (target - p.collapseAnim) * std::min(animSpeed * dt, 1.0f);
        if (std::fabs(p.collapseAnim - target) < 0.001f) p.collapseAnim = target;
    }

    for (auto& [name, anim] : m_expandAnim) {
        bool exp = m_expanded.count(name) && m_expanded.at(name);
        float target = exp ? 1.0f : 0.0f;
        
        
        const float step = dt / expandDur;
        if (anim < target)      anim = std::min(target, anim + step);
        else if (anim > target) anim = std::max(target, anim - step);
        if (std::fabs(anim - target) < 0.004f) anim = target;
    }

    
    for (auto& f : ModuleManager::FeatureList) {
        float& ta = m_toggleAnim[f->Name];
        float target = f->Enabled ? 1.0f : 0.0f;
        ta += (target - ta) * std::min(animSpeed * dt, 1.0f);
        if (std::fabs(ta - target) < 0.001f) ta = target;

        
        for (auto& s : f->Settings) {
            if (auto cb = std::dynamic_pointer_cast<CheckboxSetting>(s)) {
                float& ca = m_checkAnim[f->Name + "::" + cb->label];
                float ct = *cb->value ? 1.0f : 0.0f;
                ca += (ct - ca) * std::min(animSpeed * dt, 1.0f);
                if (std::fabs(ca - ct) < 0.01f) ca = ct;
            }
        }
    }

    for (auto& [name, fs] : m_flash) {
        if (fs.timer > 0.0f)
            fs.timer -= dt;
    }

    
    
    if (m_tooltipFeature != nullptr)
        m_tooltipHover += dt;
    else
        m_tooltipHover = 0.0f;
    float ttTarget = (m_tooltipHover >= TOOLTIP_HOLD) ? 1.0f : 0.0f;
    m_tooltipAnim += (ttTarget - m_tooltipAnim) * std::min(animSpeed * dt, 1.0f);
    if (std::fabs(m_tooltipAnim - ttTarget) < 0.01f) m_tooltipAnim = ttTarget;
}

void ClickGui::DrawSettings(ImDrawList* dl, Feature* f,
    float x, float baseY,
    bool isMouseDown, bool mouseClicked,
    ImVec2 mouse, bool handleInput, float contentFade)
{
    
    m_contentFade = contentFade; 
    ImGui::PushFont(ImGui::GetFont(), 13.0f);

    const float pad = SETTINGS_PAD;
    const float blockW = COL_W - 2.0f * MODULE_PAD;
    const float x2 = x + blockW - SETTINGS_PAD;
    const float innerX = x + pad;
    const float innerW = x2 - innerX;

    float rowY = baseY + SETTINGS_PAD;

    for (auto& s : f->Settings) {
        if (!s->visibility()) continue;

        const float rowY2 = rowY + SETTING_H;
        bool inRow = (mouse.x >= x && mouse.x < x + COL_W && mouse.y >= rowY && mouse.y < rowY2);

        if (auto sl = std::dynamic_pointer_cast<SliderSetting>(s)) {
            const float halfH = SETTING_H * 0.5f;

            ImVec2 labelSz = ImGui::CalcTextSize(sl->label.c_str());
            dl->AddText({ innerX, rowY + (halfH - labelSz.y) * 0.5f },
                Col(C_ON_SURFACE_DIM), sl->label.c_str());

            char valBuf[32];
            std::snprintf(valBuf, sizeof(valBuf), sl->format.c_str(), *sl->value);
            ImVec2 valSz = ImGui::CalcTextSize(valBuf);
            dl->AddText({ x2 - valSz.x, rowY + (halfH - valSz.y) * 0.5f },
                Col(C_ON_SURFACE), valBuf);

            const float barH = 6.0f;
            const float barW = innerW;
            const float barY = rowY + SETTING_H - barH - 10.0f;

            const float t = std::clamp((*sl->value - sl->minVal) / (sl->maxVal - sl->minVal), 0.0f, 1.0f);
            const float fillW = barW * t;

            dl->AddRectFilled({ innerX, barY }, { innerX + barW, barY + barH },
                Col(C_TRACK), barH * 0.5f);
            if (fillW > 0.0f)
                dl->AddRectFilled({ innerX, barY }, { innerX + fillW, barY + barH },
                    Col(C_PRIMARY), barH * 0.5f);

            const float knobR = barH * 0.95f;
            dl->AddCircleFilled({ innerX + fillW, barY + barH * 0.5f }, knobR, Col(C_ON_PRIMARY));

            bool inBar = (mouse.x >= innerX && mouse.x <= innerX + barW &&
                mouse.y >= rowY && mouse.y < rowY2);
            if (handleInput && inBar && mouseClicked) {
                m_sliderDrag.active = true;
                m_sliderDrag.feature = f;
                m_sliderDrag.label = sl->label;
            }
            if (m_sliderDrag.active &&
                m_sliderDrag.feature == f &&
                m_sliderDrag.label == sl->label)
            {
                if (handleInput && isMouseDown) {
                    float newT = (mouse.x - innerX) / barW;
                    *sl->value = sl->minVal + std::clamp(newT, 0.0f, 1.0f) * (sl->maxVal - sl->minVal);
                }
                else if (!handleInput || !isMouseDown) {
                    m_sliderDrag.active = false;
                }
            }
        }
        else if (auto cb = std::dynamic_pointer_cast<CheckboxSetting>(s)) {
            const float box = 18.0f;
            const float boxX = innerX;
            const float boxY = rowY + (SETTING_H - box) * 0.5f;

            
            
            float ca = m_checkAnim[f->Name + "::" + cb->label];
            dl->AddRectFilled({ boxX, boxY }, { boxX + box, boxY + box }, Col(C_PRIMARY_CONTAINER, ca), 4.0f);
            dl->AddRect({ boxX, boxY }, { boxX + box, boxY + box }, Col(C_OUTLINE, 1.0f - ca), 4.0f, 0, 1.5f);
            if (ca > 0.001f) {
                ImU32 ck = Col(C_ON_PRIMARY_CONTAINER, ca);
                const float cx0 = boxX + 4.0f,  cy0 = boxY + box * 0.52f;
                const float cx1 = boxX + 7.5f,  cy1 = boxY + box * 0.74f;
                const float cx2 = boxX + 14.0f, cy2 = boxY + box * 0.28f;
                float t1 = std::min(1.0f, ca * 2.0f);                 
                dl->AddLine({ cx0, cy0 }, { cx0 + (cx1 - cx0) * t1, cy0 + (cy1 - cy0) * t1 }, ck, 2.0f);
                float t2 = std::max(0.0f, (ca - 0.5f) * 2.0f);        
                dl->AddLine({ cx1, cy1 }, { cx1 + (cx2 - cx1) * t2, cy1 + (cy2 - cy1) * t2 }, ck, 2.0f);
            }

            float labelX = boxX + box + 10.0f;
            ImVec2 lblSz = ImGui::CalcTextSize(cb->label.c_str());
            dl->AddText({ labelX, rowY + (SETTING_H - lblSz.y) * 0.5f },
                Col(C_ON_SURFACE), cb->label.c_str());

            if (handleInput && inRow && mouseClicked)
                *cb->value = !(*cb->value);
        }
        else if (auto cm = std::dynamic_pointer_cast<ComboSetting>(s)) {
            if (cm->options.empty()) continue;

            ImVec2 labelSz = ImGui::CalcTextSize(cm->label.c_str());
            dl->AddText({ innerX, rowY + (SETTING_H - labelSz.y) * 0.5f },
                Col(C_ON_SURFACE_DIM), cm->label.c_str());

            const std::string& valStr = cm->options[std::clamp(*cm->value, 0, (int)cm->options.size() - 1)];
            ImVec2 valSz = ImGui::CalcTextSize(valStr.c_str());

            const float pillPadX = 10.0f, pillPadY = 5.0f;
            const float pillW = valSz.x + pillPadX * 2.0f;
            const float pillH = valSz.y + pillPadY * 2.0f;
            const float pillX = x2 - pillW;
            const float pillY = rowY + (SETTING_H - pillH) * 0.5f;

            dl->AddRectFilled({ pillX, pillY }, { pillX + pillW, pillY + pillH },
                Col(C_SURFACE_3), PILL_RADIUS);
            dl->AddRect({ pillX, pillY }, { pillX + pillW, pillY + pillH },
                Col(C_OUTLINE), PILL_RADIUS, 0, 1.0f);
            dl->AddText({ pillX + pillPadX, pillY + pillPadY },
                Col(C_ON_SURFACE), valStr.c_str());

            if (handleInput && inRow && mouseClicked) {
                if (*cm->value < 0 || static_cast<size_t>(*cm->value) >= cm->options.size())
                    *cm->value = 0;
                *cm->value = (*cm->value + 1) % static_cast<int>(cm->options.size());
                if (cm->label == "Mode") {
                    f->SetMode(cm->options[*cm->value]);
                }
            }
        }
        else if (auto bt = std::dynamic_pointer_cast<ButtonSetting>(s)) {
            
            const std::string valStr = bt->label;
            ImVec2 valSz = ImGui::CalcTextSize(valStr.c_str());
            const float pillPadX = 14.0f, pillPadY = 6.0f;
            const float pillW = valSz.x + pillPadX * 2.0f;
            const float pillH = valSz.y + pillPadY * 2.0f;
            const float pillX = x2 - pillW;
            const float pillY = rowY + (SETTING_H - pillH) * 0.5f;

            bool hover = (mouse.x >= pillX && mouse.x <= pillX + pillW &&
                          mouse.y >= pillY && mouse.y <= pillY + pillH);
            dl->AddRectFilled({ pillX, pillY }, { pillX + pillW, pillY + pillH },
                Col(hover ? C_SURFACE_2 : C_SURFACE_3), PILL_RADIUS);
            dl->AddRect({ pillX, pillY }, { pillX + pillW, pillY + pillH },
                Col(C_OUTLINE), PILL_RADIUS, 0, 1.0f);
            dl->AddText({ pillX + pillPadX, pillY + pillPadY }, Col(C_PRIMARY), valStr.c_str());

            if (handleInput && hover && mouseClicked)
                bt->onActivate();
        }
        else if (auto ts = std::dynamic_pointer_cast<TextSetting>(s)) {
            ImVec2 lblSz = ImGui::CalcTextSize(ts->label.c_str());
            float labelY = rowY + 2.0f;
            float boxY = labelY + lblSz.y + 3.0f;          
            float boxH = (rowY + SETTING_H) - boxY - 2.0f; 
            float boxX = innerX;
            float boxW = innerW;
            bool focused = (m_textEdit.active && m_textEdit.feature == f && m_textEdit.label == ts->label);
            bool inBox = (mouse.x >= boxX && mouse.x <= boxX + boxW &&
                          mouse.y >= boxY && mouse.y <= boxY + boxH);

            dl->AddRectFilled({ boxX, boxY }, { boxX + boxW, boxY + boxH },
                Col(focused ? C_SURFACE_2 : C_SURFACE_3), 6.0f);
            dl->AddRect({ boxX, boxY }, { boxX + boxW, boxY + boxH },
                Col(focused ? C_PRIMARY : C_OUTLINE), 6.0f, 0, focused ? 1.5f : 1.0f);

            dl->AddText({ boxX, labelY }, Col(C_ON_SURFACE_DIM), ts->label.c_str());

            std::string text = ts->buf;
            if (focused && ((int)(ImGui::GetTime() * 2.0f) & 1)) text += "|";

            
            const float txtW = boxW - 12.0f;
            ImVec2 txtSz = ImGui::CalcTextSize(text.c_str());
            if (txtSz.x > txtW) {
                while (text.size() > 1 && ImGui::CalcTextSize(text.c_str()).x > txtW) text.pop_back();
                text += "...";
            }
            txtSz = ImGui::CalcTextSize(text.c_str());
            dl->AddText({ boxX + 6.0f, boxY + (boxH - txtSz.y) * 0.5f },
                Col(C_ON_SURFACE), text.c_str());

            if (handleInput && inBox && mouseClicked) {
                if (focused) { m_textEdit.active = false; m_textEdit.feature = nullptr; }
                else { m_textEdit.feature = f; m_textEdit.label = ts->label; m_textEdit.active = true; }
            }

            if (focused) {
                if (Hooks::Keyboard::WasKeyPressed(VK_BACK)) {
                    size_t l = strlen(ts->buf);
                    if (l > 0) ts->buf[l - 1] = '\0';
                }
                if (Hooks::Keyboard::WasKeyPressed(VK_RETURN) || Hooks::Keyboard::WasKeyPressed(VK_ESCAPE)) {
                    m_textEdit.active = false; m_textEdit.feature = nullptr;
                }
                bool shift = Hooks::Keyboard::IsKeyDown(VK_SHIFT) ||
                             Hooks::Keyboard::IsKeyDown(VK_LSHIFT) ||
                             Hooks::Keyboard::IsKeyDown(VK_RSHIFT);
                
                bool caps  = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
                bool upper = shift ^ caps;
                auto append = [&](char ch) {
                    size_t l = strlen(ts->buf);
                    if (l < (size_t)ts->maxLen) { ts->buf[l] = ch; ts->buf[l + 1] = '\0'; }
                };
                for (int i = 0; i < 26; ++i)
                    if (Hooks::Keyboard::WasKeyPressed('A' + i))
                        append(upper ? ('A' + i) : ('a' + i));
                for (int i = 0; i < 10; ++i)
                    if (Hooks::Keyboard::WasKeyPressed('0' + i))
                        append('0' + i);
                if (Hooks::Keyboard::WasKeyPressed(VK_SPACE)) append(' ');
            }
        }

        rowY = rowY2;
    }

    rowY += KEYBIND_GAP;

    {
        const float kY2 = rowY + KEYBIND_H;
        const bool  isListening = (m_keybindListen.active && m_keybindListen.feature == f);
        bool inRow = (mouse.x >= x && mouse.x < x + COL_W && mouse.y >= rowY && mouse.y < kY2);

        const char* bindLabel = "Keybind";
        ImVec2 bindLabelSz = ImGui::CalcTextSize(bindLabel);
        dl->AddText({ innerX, rowY + (KEYBIND_H - bindLabelSz.y) * 0.5f },
            Col(C_ON_SURFACE_DIM), bindLabel);

        std::string keyStr = isListening ? "..." : KeyName(f->Keybind);

        ImVec2 keySz = ImGui::CalcTextSize(keyStr.c_str());
        const float pillPadX = 10.0f, pillPadY = 4.0f;
        const float pillW = keySz.x + pillPadX * 2.0f;
        const float pillH = keySz.y + pillPadY * 2.0f;
        const float pillX = x2 - pillW;
        const float pillY = rowY + (KEYBIND_H - pillH) * 0.5f;

        if (isListening) {
            dl->AddRectFilled({ pillX, pillY }, { pillX + pillW, pillY + pillH },
                Col(C_KEYBIND_ACTIVE), PILL_RADIUS);
            dl->AddText({ pillX + pillPadX, pillY + pillPadY },
                Col(C_ON_PRIMARY_CONTAINER), keyStr.c_str());
        }
        else {
            dl->AddRectFilled({ pillX, pillY }, { pillX + pillW, pillY + pillH },
                Col(C_KEYBIND), PILL_RADIUS);
            dl->AddText({ pillX + pillPadX, pillY + pillPadY },
                Col(C_PRIMARY), keyStr.c_str());
        }

        if (handleInput && inRow && mouseClicked && !isListening) {
            m_keybindListen.feature = f;
            m_keybindListen.active = true;
            s_isListening = true;
        }
    }

    ImGui::PopFont();
    m_contentFade = 1.0f; 
}

void ClickGui::PollKeybindListen(ClickGui::KeybindListen& listen) {
    if (!listen.active || !listen.feature) return;

    for (int vk = 0; vk <= 0xFE; ++vk) {
        if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON ||
            vk == VK_XBUTTON1 || vk == VK_XBUTTON2) continue;
        if (vk == 0xF2 || vk == 0xF3 || vk == 0xF4 || vk == 0xF5 || vk == 0xF0) continue;
        if (vk == 'Ins' || vk == 'Del') continue;

        if (!Hooks::Keyboard::WasKeyPressed(vk)) continue;

        listen.feature->Keybind = (vk == VK_ESCAPE) ? 0 : vk;
        listen.active = false;
        listen.feature = nullptr;
        s_isListening = false;
        ModuleManager::s_handledKeys = true;
        return;
    }
}

static unsigned BlendCol(unsigned a, unsigned b, float t) {
    auto ch = [](unsigned c, unsigned d, float tt, int sh) -> unsigned {
        int vc = (int)((c >> sh) & 0xFF);
        int vd = (int)((d >> sh) & 0xFF);
        return (unsigned)(vc + (int)(vd - vc) * tt) & 0xFF;
    };
    return (ch(a, b, t, 24) << 24) | (ch(a, b, t, 16) << 16) | (ch(a, b, t, 8) << 8) | ch(a, b, t, 0);
}

void ClickGui::OnEvent() {
    if (!Enabled && m_guiAlpha <= 0.0f) return;

    SyncTheme();

    
    s_animSpeed = m_animSpeed;

    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
        return;
    }

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) {
        Logger::ErrorTag("ClickGui::OnEvent", "ImDrawList is null!");
        return;
    }

    s_textEditing = m_textEdit.active;

    ImVec2      mouse = io.MousePos;

    float dt = io.DeltaTime;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;

    PollKeybindListen(m_keybindListen);

    static bool wasMouseDown = false;
    static bool wasRightMouseDown = false;

    bool isMouseDownNow = Hooks::Keyboard::IsKeyDown(VK_LBUTTON);
    bool isRightMouseDownNow = Hooks::Keyboard::IsKeyDown(VK_RBUTTON);

    bool mouseClicked = isMouseDownNow && !wasMouseDown;
    bool rightMouseClicked = isRightMouseDownNow && !wasRightMouseDown;

    wasMouseDown = isMouseDownNow;
    wasRightMouseDown = isRightMouseDownNow;

    ClickGui::s_dbgSection = "UpdateAnimations";
    UpdateAnimations(dt);

    
    m_tooltipFeature = nullptr;

    
    bool shouldHandleInput = Enabled;

    
    
    bool uiBlocking = m_showHudEditor || m_showConfigs;

    
    int panelCount = (int)m_panels.size();
    const float stepX = COL_W + 5.0f;
    float rowW = (panelCount > 0) ? (stepX * (panelCount - 1) + COL_W) : 0.0f;
    float rowStartX = (io.DisplaySize.x - rowW) * 0.5f;
    float maxPanelH = 0.0f;
    for (auto& pp : m_panels) maxPanelH = std::max(maxPanelH, PanelHeight(pp));
    float rowStartY = io.DisplaySize.y * 0.10f;
    if (rowStartY + maxPanelH > io.DisplaySize.y - 24.0f)
        rowStartY = std::max(24.0f, io.DisplaySize.y - maxPanelH - 24.0f);
    else
        rowStartY = std::max(24.0f, rowStartY);

    
    
    
    
    
    float openT = m_guiAlpha;
    ImVec2 screenCenter = { io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f };
    const float stagger = 0.035f;                                          
    const float span    = 1.0f + stagger * (float)(panelCount - 1);        

    for (int pi = 0; pi < panelCount; ++pi) {
        auto& p = m_panels[pi];
        ImVec2 target = { rowStartX + stepX * pi, rowStartY };
        float t = std::clamp(openT * span - stagger * (float)pi, 0.0f, 1.0f);
        p.x = screenCenter.x + (target.x - screenCenter.x) * t;
        p.y = screenCenter.y + (target.y - screenCenter.y) * t;

        const float x2 = p.x + COL_W;
        float       totalHeight = PanelHeight(p);

        bool inHeader = (mouse.x >= p.x && mouse.x < x2 &&
            mouse.y >= p.y && mouse.y < p.y + HEADER_H);

        if (shouldHandleInput && inHeader && rightMouseClicked)
            p.collapsed = !p.collapsed;

        
        ClickGui::s_dbgSection = "panel-surface";
        DrawShadow(dl, { p.x, p.y }, COL_W, totalHeight, ROUNDING);

        dl->PushClipRect({ p.x, p.y }, { x2, p.y + totalHeight }, true);

        dl->AddRectFilled({ p.x, p.y }, { x2, p.y + totalHeight },
            Col(C_SURFACE), ROUNDING, ImDrawFlags_RoundCornersAll);

        
        ClickGui::s_dbgSection = "panel-icon";
        auto iconIt = m_categoryIcons.find(p.cat);
        if (iconIt != m_categoryIcons.end() && iconIt->second) {
            ImTextureData* tex = iconIt->second.get();
            if (tex && tex->Status == ImTextureStatus_OK && tex->TexID != ImTextureID_Invalid) {
                const float iconSize = 20.0f;
                const float iconX = p.x + 16.0f;
                const float iconY = p.y + (HEADER_H - iconSize) * 0.5f;
                ImTextureRef texRef = tex->GetTexRef();
                dl->AddImage(texRef, { iconX, iconY }, { iconX + iconSize, iconY + iconSize },
                    { 0.0f, 0.0f }, { 1.0f, 1.0f }, Col(C_PRIMARY));
            }
        }

        ClickGui::s_dbgSection = "panel-title";
        ImVec2 titleSz = ImGui::CalcTextSize(p.name.c_str());
        dl->AddText({ p.x + 44.0f, p.y + (HEADER_H - titleSz.y) * 0.5f },
            Col(C_ON_SURFACE), p.name.c_str());

        
        dl->AddLine({ p.x + 16.0f, p.y + HEADER_H - 1.0f },
            { x2 - 16.0f, p.y + HEADER_H - 1.0f }, Col(C_OUTLINE, 0.6f));

        float curY = p.y + HEADER_H + PANEL_PAD_TOP;

        
        
        Feature* lastFeat = nullptr;
        for (auto& sp : ModuleManager::FeatureList)
            if (sp->ModuleCategory == p.cat && sp->Visibility) lastFeat = sp.get();

        ClickGui::s_dbgSection = "panel-features";
        for (auto& sp : ModuleManager::FeatureList) {
            if (sp->ModuleCategory != p.cat) continue;
            if (!sp->Visibility) continue;   
            Feature* f = sp.get();
            bool isLastInPanel = (f == lastFeat);
            snprintf(ClickGui::s_dbgBuf, sizeof(ClickGui::s_dbgBuf), "block:%s", f->Name.c_str());
            ClickGui::s_dbgSection = ClickGui::s_dbgBuf;

            float& expAnim = m_expandAnim[f->Name];
            bool  open = (m_expanded.count(f->Name) && m_expanded.at(f->Name)) || expAnim > 0.5f;

            const float mY1 = curY;
            const float mY2 = curY + MODULE_H;

            const float chevX = (x2 - MODULE_PAD) - 16.0f;
            const float chevY = mY1 + MODULE_H * 0.5f;
            bool inChevron = (mouse.x >= chevX - 14.0f && mouse.x <= chevX + 14.0f &&
                              mouse.y >= chevY - 14.0f && mouse.y <= chevY + 14.0f);

            bool inModule = (mouse.x >= p.x + MODULE_PAD && mouse.x < x2 - MODULE_PAD &&
                mouse.y >= mY1 && mouse.y < mY2);

            
            if (shouldHandleInput && !uiBlocking && mouseClicked) {
                if (inChevron) {
                    bool& exp = m_expanded[f->Name];
                    exp = !exp;
                    if (m_expandAnim.find(f->Name) == m_expandAnim.end())
                        m_expandAnim[f->Name] = exp ? 0.0f : 1.0f;
                }
                else if (inModule) {
                    bool enabling = !f->Enabled;
                    f->Enabled = enabling;
                    m_flash[f->Name] = { FLASH_DURATION, enabling };
                }
            }
            if (shouldHandleInput && !uiBlocking && inModule && rightMouseClicked) {
                bool& exp = m_expanded[f->Name];
                exp = !exp;
                if (m_expandAnim.find(f->Name) == m_expandAnim.end())
                    m_expandAnim[f->Name] = exp ? 0.0f : 1.0f;
            }

            
            if (inModule)
                m_tooltipFeature = f;

            
            snprintf(ClickGui::s_dbgBuf, sizeof(ClickGui::s_dbgBuf), "draw:%s", f->Name.c_str());
            ClickGui::s_dbgSection = ClickGui::s_dbgBuf;
            float e = EaseInOutCubic(expAnim);
            float setH = (e > 0.001f) ? ExpandedHeight(f, e) : 0.0f;
            float blockH = MODULE_H + setH;

            unsigned rowBase = C_SURFACE_2;
            ImU32    nameCol = C_ON_SURFACE;
            
            float ta = m_toggleAnim[f->Name];
            rowBase = BlendCol(rowBase, C_PRIMARY_CONTAINER, ta);
            nameCol = BlendCol(nameCol, C_ON_PRIMARY_CONTAINER, ta);
            if (inModule) {
                float& ha = m_hoverAnim[f->Name];
                ha += ((1.0f - ha) * 0.22f);
                if (ha > 0.999f) ha = 1.0f;
                rowBase = BlendCol(rowBase, C_SURFACE_3, 0.18f * ha);
            }
            else {
                float& ha = m_hoverAnim[f->Name];
                ha += ((0.0f - ha) * 0.22f);
                if (ha < 0.001f) ha = 0.0f;
                rowBase = BlendCol(rowBase, C_SURFACE_3, 0.18f * ha);
            }

            auto flashIt = m_flash.find(f->Name);
            bool hasFlash = (flashIt != m_flash.end() && flashIt->second.timer > 0.0f);
            if (hasFlash) {
                float ft = flashIt->second.timer / FLASH_DURATION;
                unsigned flash = flashIt->second.enabling ? C_MOD_FLASH_ON : C_MOD_FLASH_OFF;
                rowBase = BlendCol(rowBase, flash, ft * 0.5f);
            }

            
            
            
            
            
            ImDrawFlags hdrCorners = open ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersAll;
            dl->AddRectFilled({ p.x + MODULE_PAD, mY1 }, { x2 - MODULE_PAD, mY2 },
                Col(rowBase), ROW_RADIUS, hdrCorners);

            
            
            if (setH > 0.5f) {
                dl->AddRectFilled({ p.x + MODULE_PAD, mY2 }, { x2 - MODULE_PAD, mY1 + blockH },
                    Col(C_SETTINGS, e), ROW_RADIUS, ImDrawFlags_RoundCornersBottom);
            }

            
            dl->AddRect({ p.x + MODULE_PAD, mY1 },
                { x2 - MODULE_PAD, mY1 + blockH }, Col(C_OUTLINE), ROW_RADIUS, ImDrawFlags_RoundCornersAll, 1.0f);

            ImVec2 fSz = ImGui::CalcTextSize(f->Name.c_str());
            float nameX = p.x + MODULE_PAD + 8.0f;
            dl->PushClipRect({ nameX - 2.0f, mY1 }, { chevX - 18.0f, mY1 + MODULE_H }, true);
            dl->AddText({ nameX, mY1 + (MODULE_H - fSz.y) * 0.5f },
                Col(nameCol), f->Name.c_str());
            dl->PopClipRect();

            
            if (m_arrowTex && m_arrowTex->Status == ImTextureStatus_OK && m_arrowTex->TexID != ImTextureID_Invalid) {
                const float aS = 18.0f;
                const float aX = chevX - aS * 0.5f;
                const float aY = chevY - aS * 0.5f;
                ImTextureRef texRef = m_arrowTex->GetTexRef();
                ImU32 tint = (f->Enabled) ? Col(C_ON_PRIMARY_CONTAINER) : Col(C_ON_SURFACE_DIM);
                ImVec2 uv0 = open ? ImVec2(0.0f, 1.0f) : ImVec2(0.0f, 0.0f);
                ImVec2 uv1 = open ? ImVec2(1.0f, 0.0f) : ImVec2(1.0f, 1.0f);
                dl->AddImage(texRef, { aX, aY }, { aX + aS, aY + aS }, uv0, uv1, tint);
            }

            
            if (setH > 0.5f) {
                snprintf(ClickGui::s_dbgBuf, sizeof(ClickGui::s_dbgBuf), "settings:%s", f->Name.c_str());
                ClickGui::s_dbgSection = ClickGui::s_dbgBuf;
                dl->PushClipRect({ p.x, mY2 }, { x2, mY1 + blockH }, true);
                DrawSettings(dl, f, p.x + MODULE_PAD, mY2, isMouseDownNow, mouseClicked, mouse,
                    shouldHandleInput && !uiBlocking, e);
                dl->PopClipRect();
            }

            curY = mY1 + blockH;
            if (!isLastInPanel)
                curY += MODULE_PAD;
        }

        dl->PopClipRect();

        
        dl->AddRect({ p.x, p.y }, { x2, p.y + totalHeight },
            Col(C_OUTLINE), ROUNDING, ImDrawFlags_RoundCornersAll, 1.0f);
    }

    
    
    
    
    if (m_tooltipAnim > 0.01f && m_tooltipFeature != nullptr) {
        const Feature* tt = m_tooltipFeature;
        ImGui::PushFont(ImGui::GetFont(), 13.0f);

        ImVec2 dSz = ImGui::CalcTextSize(tt->Description.c_str());

        const float padX = 11.0f, padY = 6.0f;
        const float ttW = dSz.x + padX * 2.0f;
        const float ttH = dSz.y + padY * 2.0f;

        float tx = mouse.x + 16.0f;
        float ty = mouse.y + 14.0f;
        if (tx + ttW > io.DisplaySize.x - 4.0f) tx = mouse.x - ttW - 12.0f;
        if (ty + ttH > io.DisplaySize.y - 4.0f) ty = mouse.y - ttH - 12.0f;
        tx = std::max(4.0f, tx);
        ty = std::max(4.0f, ty);
        ty += (1.0f - m_tooltipAnim) * 5.0f;   

        float fade = m_tooltipAnim;
        for (int i = 3; i >= 1; --i) {          
            float off = (float)i * 1.5f;
            unsigned char sa = (unsigned char)(0x12 * (i / 3.0f + 0.3f) * m_guiAlpha * fade);
            dl->AddRectFilled({ tx, ty + off }, { tx + ttW, ty + ttH + off },
                IM_COL32(0, 0, 0, sa), 10.0f, ImDrawFlags_RoundCornersAll);
        }
        dl->AddRectFilled({ tx, ty }, { tx + ttW, ty + ttH },
            Col(C_SURFACE, fade), 10.0f);
        dl->AddRect({ tx, ty }, { tx + ttW, ty + ttH },
            Col(C_OUTLINE, fade), 10.0f, ImDrawFlags_RoundCornersAll, 1.0f);

        ImVec2 txtSz = ImGui::CalcTextSize(tt->Description.c_str());
        dl->AddText({ tx + padX, ty + (ttH - txtSz.y) * 0.5f },
            Col(C_ON_SURFACE, fade), tt->Description.c_str());

        ImGui::PopFont();
    }

}

void ClickGui::DrawOverlayUI() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) return;
    if (!IsGUIActive()) return;   

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    SyncTheme();

    ImVec2 mouse = io.MousePos;
    bool isMouseDownNow = io.MouseDown[0];
    bool mouseClicked = io.MouseClicked[0];

    const auto inRect = [](const ImVec2& m, float x, float y, float w, float h) {
        return m.x >= x && m.x <= x + w && m.y >= y && m.y <= y + h;
    };

    
    auto pillRect = [&](const char* t, float cx, float cy) {
        ImVec2 s = ImGui::CalcTextSize(t);
        return ImVec4(cx - (s.x + 28.0f) * 0.5f, cy - (s.y + 12.0f) * 0.5f,
                      cx + (s.x + 28.0f) * 0.5f, cy + (s.y + 12.0f) * 0.5f);
    };
    auto drawPill = [&](const ImVec4& r, const char* t, bool hover, bool filled) {
        const unsigned bg = filled
            ? (hover ? BlendCol(C_PRIMARY, C_PRIMARY_CONTAINER, 0.4f) : C_PRIMARY)
            : (hover ? C_SURFACE_3 : C_SURFACE_2);
        const unsigned tx = filled ? C_ON_PRIMARY : (hover ? C_PRIMARY : C_ON_SURFACE_DIM);
        dl->AddRectFilled({ r.x, r.y }, { r.z, r.w }, Col(bg), 999.0f);
        dl->AddRect({ r.x, r.y }, { r.z, r.w }, Col(tx, 0.30f), 999.0f, 0, 1.0f);
        ImVec2 ts = ImGui::CalcTextSize(t);
        dl->AddText({ r.x + (r.z - r.x - ts.x) * 0.5f, r.y + (r.w - r.y - ts.y) * 0.5f },
            Col(tx), t);
    };
    auto hitPill = [&](const ImVec4& r, const ImVec2& m) {
        return inRect(m, r.x, r.y, r.z - r.x, r.w - r.y);
    };
    auto pillWdt = [&](const char* t) { return ImGui::CalcTextSize(t).x + 28.0f; };

    
    auto drawCard = [&](const ImVec2& p, float w, float h, const char* title, const char* sub) {
        DrawShadow(dl, p, w, h, ROUNDING);
        dl->AddRectFilled({ p.x, p.y }, { p.x + w, p.y + h }, Col(C_SURFACE), ROUNDING, ImDrawFlags_RoundCornersAll);
        dl->AddRect({ p.x, p.y }, { p.x + w, p.y + h }, Col(C_OUTLINE), ROUNDING, ImDrawFlags_RoundCornersAll, 1.0f);
        dl->AddRectFilled({ p.x + 18.0f, p.y + 17.0f }, { p.x + 22.0f, p.y + 21.0f }, Col(C_PRIMARY), 4.0f);
        ImGui::PushFont(ImGui::GetFont(), 16.0f);
        ImVec2 ts = ImGui::CalcTextSize(title);
        dl->AddText({ p.x + 30.0f, p.y + 19.0f - ts.y * 0.5f }, Col(C_ON_SURFACE), title);
        ImGui::PopFont();
        if (sub && sub[0]) {
            dl->AddText({ p.x + 30.0f, p.y + 34.0f }, Col(C_ON_SURFACE_DIM), sub);
        }
    };

    const float rightX = io.DisplaySize.x - 32.0f;

    
    const float pillGap  = 12.0f;
    const float wHudB    = pillWdt("HUD Editor");
    const float wCfgB    = pillWdt("Configs");
    const float hPillB   = std::max(ImGui::CalcTextSize("HUD Editor").y,
                                    ImGui::CalcTextSize("Configs").y) + 12.0f;
    const float totalB   = wHudB + pillGap + wCfgB;
    const float barY     = io.DisplaySize.y - hPillB - 12.0f;
    const float barCX    = io.DisplaySize.x * 0.5f;
    const ImVec4 rBarHud = ImVec4(barCX - totalB * 0.5f, barY, barCX - totalB * 0.5f + wHudB, barY + hPillB);
    const ImVec4 rBarCfg = ImVec4(barCX - totalB * 0.5f + wHudB + pillGap, barY, barCX + totalB * 0.5f, barY + hPillB);
    drawPill(rBarHud, "HUD Editor", hitPill(rBarHud, mouse), m_showHudEditor);
    drawPill(rBarCfg, "Configs",    hitPill(rBarCfg, mouse), m_showConfigs);
    const bool onBar = hitPill(rBarHud, mouse) || hitPill(rBarCfg, mouse);
    if (mouseClicked && hitPill(rBarHud, mouse)) {
        if (m_showHudEditor) m_dragHud = -1;
        m_showHudEditor = !m_showHudEditor;
    }
    if (mouseClicked && hitPill(rBarCfg, mouse)) {
        if (!m_showConfigs) { m_cfgList = Config::List(); m_cfgScroll = 0.0f; m_cfgInputFocused = false; }
        m_showConfigs = !m_showConfigs;
    }

    
    const bool hudOpen = m_showHudEditor;
    const bool cfgOpen = m_showConfigs;

    if (!isMouseDownNow) m_dragWin = -1;

    ImVec2 hudP{ 0.0f, 0.0f };  const float hudW = 280.0f, hudH = 156.0f;
    if (hudOpen)
        hudP = { (m_hudPosX >= 0.0f ? m_hudPosX : rightX - hudW),
                 (m_hudPosY >= 0.0f ? m_hudPosY : (io.DisplaySize.y - hudH) * 0.5f) };

    const float rowH = 36.0f;
    const float listH = (float)std::min((int)m_cfgList.size(), 5) * rowH;
    const float cfgW = 310.0f;
    float cfgH = 56.0f + (ImGui::CalcTextSize("Preset name").y + 4.0f) + 30.0f + 12.0f + 34.0f + 20.0f + listH + 16.0f;
    if (m_cfgList.empty()) cfgH += 6.0f;
    ImVec2 cfgP{ 0.0f, 0.0f };
    if (cfgOpen) {
        if (m_cfgPosX >= 0.0f && m_cfgPosY >= 0.0f) {
            cfgP = { m_cfgPosX, m_cfgPosY };
        } else {
            cfgP = { rightX - cfgW, (io.DisplaySize.y - cfgH) * 0.5f };
            if (hudOpen) cfgP.x = rightX - cfgW - 16.0f - hudW;
        }
    }

    
    if (m_dragWin == 0 && hudOpen) {
        hudP.x = std::clamp(mouse.x - m_dragWinOX, 0.0f, std::max(0.0f, io.DisplaySize.x - hudW));
        hudP.y = std::clamp(mouse.y - m_dragWinOY, 0.0f, std::max(0.0f, io.DisplaySize.y - hudH));
        m_hudPosX = hudP.x; m_hudPosY = hudP.y;
    }
    if (m_dragWin == 1 && cfgOpen) {
        cfgP.x = std::clamp(mouse.x - m_dragWinOX, 0.0f, std::max(0.0f, io.DisplaySize.x - cfgW));
        cfgP.y = std::clamp(mouse.y - m_dragWinOY, 0.0f, std::max(0.0f, io.DisplaySize.y - cfgH));
        m_cfgPosX = cfgP.x; m_cfgPosY = cfgP.y;
    }

    
    if (hudOpen) {
        if (mouseClicked && inRect(mouse, hudP.x, hudP.y, hudW, 40.0f) && m_dragWin == -1 && !onBar &&
            m_dragHud == -1) {
            m_dragWin = 0; m_dragWinOX = mouse.x - hudP.x; m_dragWinOY = mouse.y - hudP.y;
            m_hudPosX = hudP.x; m_hudPosY = hudP.y;
        }
        int hudIdx = 0;
        for (auto& sp : ModuleManager::FeatureList) {
            Feature* hf = sp.get();
            if (!hf->IsHUD) { ++hudIdx; continue; }
            float hw = std::max(150.0f, hf->HudW);
            float hh = 26.0f;
            bool inside = inRect(mouse, hf->HudX, hf->HudY, hw, hh);
            if (mouseClicked && inside && m_dragHud == -1) {
                m_dragHud = hudIdx;
                m_dragOffX = mouse.x - hf->HudX;
                m_dragOffY = mouse.y - hf->HudY;
            }
            if (m_dragHud == hudIdx && isMouseDownNow) {
                hf->HudX = std::max(0.0f, mouse.x - m_dragOffX);
                hf->HudY = std::max(0.0f, mouse.y - m_dragOffY);
            }
            dl->AddRectFilled({ hf->HudX, hf->HudY }, { hf->HudX + hw, hf->HudY + hh },
                Col(C_PRIMARY), 8.0f);
            dl->AddRect({ hf->HudX, hf->HudY }, { hf->HudX + hw, hf->HudY + hh },
                Col(C_ON_SURFACE), 8.0f, ImDrawFlags_RoundCornersAll, 1.0f);
            ImVec2 ts = ImGui::CalcTextSize(hf->Name.c_str());
            dl->AddText({ hf->HudX + (hw - ts.x) * 0.5f, hf->HudY + (hh - ts.y) * 0.5f },
                Col(C_ON_PRIMARY), hf->Name.c_str());
            ++hudIdx;
        }
        if (!isMouseDownNow) m_dragHud = -1;

        drawCard(hudP, hudW, hudH, "HUD Editor", "");
        const float hix = hudP.x + 18.0f;
        float hay = hudP.y + 42.0f;
        dl->AddText({ hix, hay }, Col(C_ON_SURFACE_DIM), "Drag the accent handles to");
        hay += 18.0f;
        dl->AddText({ hix, hay }, Col(C_ON_SURFACE_DIM), "reposition HUD modules.");

        const ImVec4 rDone = pillRect("Done", hudP.x + hudW * 0.5f, hudP.y + hudH - 24.0f);
        const bool doneHov = hitPill(rDone, mouse);
        drawPill(rDone, "Done", doneHov, true);
        if (mouseClicked && doneHov) { m_showHudEditor = false; m_dragHud = -1; }
    }

    
    if (cfgOpen) {
        if (mouseClicked && inRect(mouse, cfgP.x, cfgP.y, cfgW, 40.0f) && m_dragWin == -1 && !onBar) {
            m_dragWin = 1; m_dragWinOX = mouse.x - cfgP.x; m_dragWinOY = mouse.y - cfgP.y;
            m_cfgPosX = cfgP.x; m_cfgPosY = cfgP.y;
        }
        const float iw = cfgW - 36.0f;
        const float ix = cfgP.x + 18.0f;

        drawCard(cfgP, cfgW, cfgH, "Configs", "Load/save your preset list");

        
        float ay = cfgP.y + 56.0f;
        ImVec2 lbl = ImGui::CalcTextSize("Preset name");
        dl->AddText({ ix, ay }, Col(C_ON_SURFACE_DIM), "Preset name");
        ay += lbl.y + 4.0f;
        const float boxH = 30.0f;
        const bool inBox = inRect(mouse, ix, ay, iw, boxH);
        const bool focused = m_cfgInputFocused;
        dl->AddRectFilled({ ix, ay }, { ix + iw, ay + boxH },
            Col(focused ? C_SURFACE_3 : C_SURFACE_2), 8.0f);
        dl->AddRect({ ix, ay }, { ix + iw, ay + boxH },
            Col(focused ? C_PRIMARY : C_OUTLINE), 8.0f, 0, focused ? 1.5f : 1.0f);
        if (mouseClicked && inBox) m_cfgInputFocused = true;

        std::string val = m_cfgName;
        if (focused && ((int)(ImGui::GetTime() * 2.0f) & 1)) val += "|";
        const float tW = iw - 12.0f;
        if (ImGui::CalcTextSize(val.c_str()).x > tW) {
            while (val.size() > 1 && ImGui::CalcTextSize(val.c_str()).x > tW) val.pop_back();
            val += "...";
        }
        ImVec2 vSz = ImGui::CalcTextSize(val.c_str());
        dl->AddText({ ix + 6.0f, ay + (boxH - vSz.y) * 0.5f }, Col(C_ON_SURFACE), val.c_str());

        if (focused) {
            if (Hooks::Keyboard::WasKeyPressed(VK_BACK)) {
                size_t l = strlen(m_cfgName);
                if (l > 0) m_cfgName[l - 1] = '\0';
            }
            if (Hooks::Keyboard::WasKeyPressed(VK_RETURN) || Hooks::Keyboard::WasKeyPressed(VK_ESCAPE))
                m_cfgInputFocused = false;
            bool shift = Hooks::Keyboard::IsKeyDown(VK_SHIFT) ||
                         Hooks::Keyboard::IsKeyDown(VK_LSHIFT) ||
                         Hooks::Keyboard::IsKeyDown(VK_RSHIFT);
            bool caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
            bool upper = shift ^ caps;
            auto append = [&](char ch) {
                size_t l = strlen(m_cfgName);
                if (l < sizeof(m_cfgName) - 1) { m_cfgName[l] = ch; m_cfgName[l + 1] = '\0'; }
            };
            for (int i = 0; i < 26; ++i)
                if (Hooks::Keyboard::WasKeyPressed('A' + i)) append(upper ? ('A' + i) : ('a' + i));
            for (int i = 0; i < 10; ++i)
                if (Hooks::Keyboard::WasKeyPressed('0' + i)) append('0' + i);
            if (Hooks::Keyboard::WasKeyPressed(VK_SPACE)) append(' ');
        }

        ay += boxH + 12.0f;

        
        const float rowCy   = ay + 17.0f;
        const float rowHgt  = ImGui::CalcTextSize("Save").y + 12.0f;
        const float saveW   = pillWdt("Save");
        const float refreshW = pillWdt("Refresh");
        const float refR    = ix + iw - 8.0f;
        const float refL    = refR - refreshW;
        const float savR    = refL - 8.0f;
        const float savL    = savR - saveW;
        const ImVec4 rSave  = { savL, rowCy - rowHgt * 0.5f, savR, rowCy + rowHgt * 0.5f };
        const ImVec4 rRef   = { refL, rowCy - rowHgt * 0.5f, refR, rowCy + rowHgt * 0.5f };
        const bool hSave = hitPill(rSave, mouse), hRef = hitPill(rRef, mouse);
        drawPill(rSave, "Save",    hSave, true);
        drawPill(rRef,  "Refresh", hRef,  false);
        if (mouseClicked && hSave) {
            if (m_cfgName[0]) { Config::Save(m_cfgName); m_cfgList = Config::List(); m_cfgScroll = 0.0f; }
            Notifications::Push("Config", std::string("Saved '") + m_cfgName + "'", Notifications::Type::Info);
        }
        if (mouseClicked && hRef) m_cfgList = Config::List();

        ay += 34.0f + 8.0f;
        dl->AddLine({ ix, ay }, { ix + iw, ay }, Col(C_OUTLINE));
        ay += 10.0f;
        const float listTop = ay;
        const float listBottom = ay + listH;

        if (m_cfgList.empty()) {
            dl->AddText({ ix, listTop }, Col(C_ON_SURFACE_DIM),
                "No presets yet - type a name, then Save.");
        }
        else {
            if (inRect(mouse, ix, listTop, iw, listH)) {
                m_cfgScroll -= io.MouseWheel * 36.0f;
                float maxScroll = (float)m_cfgList.size() * rowH - listH;
                if (maxScroll < 0.0f) maxScroll = 0.0f;
                m_cfgScroll = std::clamp(m_cfgScroll, 0.0f, maxScroll);
            }

            dl->PushClipRect({ ix, listTop }, { ix + iw, listBottom }, true);
            const auto snapshot = m_cfgList;   
            float ry = listTop - m_cfgScroll;
            for (size_t i = 0; i < snapshot.size(); i++) {
                const std::string& fn = snapshot[i];
                if (ry + rowH < listTop) { ry += rowH; continue; }
                if (ry > listBottom) break;

                const bool rowHov = inRect(mouse, ix, ry, iw, rowH);
                if (rowHov) dl->AddRectFilled({ ix, ry }, { ix + iw, ry + rowH }, Col(C_SURFACE_2), 8.0f);

                std::string nm = fn;
                const float maxNameW = iw - pillWdt("Del") - pillWdt("Load") - 40.0f;
                if (ImGui::CalcTextSize(nm.c_str()).x > maxNameW) {
                    while (nm.size() > 1 && ImGui::CalcTextSize(nm.c_str()).x > maxNameW) nm.pop_back();
                    nm += "...";
                }
                ImVec2 nSz = ImGui::CalcTextSize(nm.c_str());
                dl->AddText({ ix + 10.0f, ry + (rowH - nSz.y) * 0.5f }, Col(C_ON_SURFACE), nm.c_str());

                const float pillCy = ry + rowH * 0.5f;
                const float pillH = ImGui::CalcTextSize("Load").y + 12.0f;
                const float delW = pillWdt("Del");
                const float loadW = pillWdt("Load");
                const float delX2 = ix + iw - 8.0f;
                const float delX1 = delX2 - delW;
                const float loadX2 = delX1 - 8.0f;
                const float loadX1 = loadX2 - loadW;
                const ImVec4 rLoad{ loadX1, pillCy - pillH * 0.5f, loadX2, pillCy + pillH * 0.5f };
                const ImVec4 rDel { delX1,  pillCy - pillH * 0.5f, delX2,  pillCy + pillH * 0.5f };
                drawPill(rLoad, "Load", hitPill(rLoad, mouse), true);
                drawPill(rDel,  "Del",  hitPill(rDel,  mouse), false);
                if (mouseClicked && hitPill(rLoad, mouse)) {
                    Config::Load(fn);
                    Notifications::Push("Config", std::string("Loaded '") + fn + "'", Notifications::Type::Info);
                }
                if (mouseClicked && hitPill(rDel, mouse)) {
                    Config::Delete(fn);
                    m_cfgList = Config::List();
                    m_cfgScroll = 0.0f;
                    Notifications::Push("Config", std::string("Deleted '") + fn + "'", Notifications::Type::Warn);
                }

                ry += rowH;
            }
            dl->PopClipRect();
        }

        
        if (mouseClicked && !onBar &&
            !inRect(mouse, cfgP.x, cfgP.y, cfgW, cfgH) &&
            !(hudOpen && inRect(mouse, hudP.x, hudP.y, hudW, hudH))) {
            m_showConfigs = false;
            m_cfgInputFocused = false;
            m_cfgScroll = 0.0f;
        }
    }
}

std::vector<Feature*> ClickGui::GetFeatures(Category cat) const {
    std::vector<Feature*> result;
    for (auto& sp : ModuleManager::FeatureList)
        if (sp->ModuleCategory == cat && sp->Visibility)
            result.push_back(sp.get());
    return result;
}