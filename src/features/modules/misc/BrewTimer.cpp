#include "BrewTimer.hpp"

#include "../../GuiTheme.hpp"
#include "../../../utils/logger.hpp"
#include "../../../sdk/Signatures.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>


PVOID                  BrewTimer::s_veh         = nullptr;
DWORD_PTR              BrewTimer::s_targetAddr  = 0;
BYTE                   BrewTimer::s_originalByte = 0;
DWORD                   BrewTimer::s_origProtect  = 0;
bool                   BrewTimer::s_armed       = false;
bool                   BrewTimer::s_installed   = false;
LONG                   BrewTimer::s_standCount  = 0;
DWORD_PTR              BrewTimer::s_stands[MAX_STANDS] = { 0 };

BrewTimer::BrewTimer() : Feature("Brewtimer", Category::Visual) {
    this->Description = "Shows all brewing stands and their timers";
    this->Enabled     = false;  
    this->CallAllTime = false;
    this->Keybind     = 0;      
    this->Visibility  = true;
    this->IsBackground = false;
    this->IsHUD       = true;   
    for (int i = 0; i < MAX_STANDS; i++) s_stands[i] = 0;
}






LONG WINAPI BrewTimer::VehHandler(PEXCEPTION_POINTERS ep) {
    if (!ep || !ep->ExceptionRecord || !ep->ContextRecord)
        return EXCEPTION_CONTINUE_SEARCH;

    DWORD code = ep->ExceptionRecord->ExceptionCode;

    if (code == STATUS_BREAKPOINT) {
        DWORD_PTR ip, rsi;
#ifdef _WIN64
        ip  = ep->ContextRecord->Rip;
        rsi = ep->ContextRecord->Rsi;
#else
        ip  = ep->ContextRecord->Eip;
        rsi = ep->ContextRecord->Esi;
#endif
        
        
        
        
        
        
        if (ip == s_targetAddr) {
            
            
            
            
            if (rsi != 0) {
                DWORD_PTR key = rsi + OFFSET;

                
                
                constexpr uint32_t MAX_BREW_TICKS = 1000;
                uint32_t sample = 0xFFFFFFFF;
                __try { sample = *(uint32_t*)(rsi + OFFSET); }
                __except (EXCEPTION_EXECUTE_HANDLER) { sample = 0xFFFFFFFF; }

                if (sample <= MAX_BREW_TICKS) {
                    bool known = false;
                    for (int i = 0; i < MAX_STANDS; i++) {
                        if (s_stands[i] == key) { known = true; break; }
                    }
                    if (!known) {
                        for (int i = 0; i < MAX_STANDS; i++) {
                            if (InterlockedCompareExchangePointer(
                                    (PVOID*)&s_stands[i], (PVOID)key, nullptr) == nullptr) {
                                InterlockedIncrement(&s_standCount);
                                break;
                            }
                        }
                    }
                }
            }

            
            
            
            *(BYTE*)s_targetAddr = s_originalByte;

            ep->ContextRecord->EFlags |= 0x100; 
            s_armed = true;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    else if (code == STATUS_SINGLE_STEP) {
        if (s_armed) {
            s_armed = false;
            
            *(BYTE*)s_targetAddr = 0xCC; 
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}





uint32_t BrewTimer::ReadRaw(DWORD_PTR fieldAddr) {
    uint32_t v = 0xFFFFFFFF;
    __try {
        v = *(uint32_t*)fieldAddr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        v = 0xFFFFFFFF;
    }
    return v;
}

ImU32 BrewTimer::ApplyAlpha(ImU32 color, float a) {
    unsigned char ca = (unsigned char)((color >> 24) & 0xFF);
    ca = (unsigned char)(ca * a);
    return (color & 0x00FFFFFF) | ((ImU32)ca << 24);
}




void BrewTimer::InstallHook() {
    if (s_installed) return;

    
    s_targetAddr = Signatures::Get("BrewTimer");
    if (s_targetAddr == 0) {
        Logger::WarnTag("BrewTimer", "brew-time pattern not found; timer disabled");
        return;
    }

    s_originalByte = *(BYTE*)s_targetAddr;

    
    
    
    
    VirtualProtect((LPVOID)s_targetAddr, 1, PAGE_EXECUTE_READWRITE, &s_origProtect);
    *(BYTE*)s_targetAddr = 0xCC; 

    s_veh = AddVectoredExceptionHandler(1, VehHandler);
    s_installed = (s_veh != nullptr);
    Logger::InfoTag("BrewTimer", "hook installed at 0x%p", (void*)s_targetAddr);
}

void BrewTimer::RemoveHook() {
    if (!s_installed) return;

    if (s_targetAddr) {
        
        
        *(BYTE*)s_targetAddr = s_originalByte;
        DWORD old;
        VirtualProtect((LPVOID)s_targetAddr, 1, s_origProtect, &old);
    }
    if (s_veh) RemoveVectoredExceptionHandler(s_veh);
    s_veh = nullptr;
    s_targetAddr = 0;
    s_installed = false;

    for (int i = 0; i < MAX_STANDS; i++) s_stands[i] = 0;
    s_standCount = 0;
}

void BrewTimer::OnEnabled()  { InstallHook(); }
void BrewTimer::OnDisabled() { RemoveHook(); }




void BrewTimer::OnEvent() {
    if (!Enabled && m_alpha <= 0.0f) return;

    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    float dt = io.DeltaTime;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;

    
    float target = Enabled ? 1.0f : 0.0f;
    m_alpha += (target - m_alpha) * std::min(10.0f * dt, 1.0f);
    if (std::fabs(m_alpha - target) < 0.01f) m_alpha = target;
    if (m_alpha <= 0.01f) return;

    const float pad     = 14.0f;
    const float rowH    = 22.0f;
    const float titleH  = 30.0f;
    const float panelW  = 200.0f;

    
    
    
    
    
    uint32_t  liveRaw[MAX_STANDS];
    uint32_t  seenRaw[MAX_STANDS];
    int       liveCount = 0;
    int       seenCount = 0;
    for (int i = 0; i < MAX_STANDS; i++) {
        DWORD_PTR base = s_stands[i];
        if (base == 0) continue;
        uint32_t raw = ReadRaw(base);
        if (raw == 0xFFFFFFFF) {
            
            InterlockedExchangePointer((PVOID*)&s_stands[i], nullptr);
            InterlockedDecrement(&s_standCount);
            continue;
        }
        bool dup = false;
        
        
        
        
        constexpr int DEDUP_TOL = 3;
        for (int k = 0; k < seenCount; k++) {
            uint32_t d = (raw > seenRaw[k]) ? (raw - seenRaw[k]) : (seenRaw[k] - raw);
            if (d <= (uint32_t)DEDUP_TOL) { dup = true; break; }
        }
        if (dup) continue;
        if (liveCount < MAX_STANDS) {
            seenRaw[seenCount++] = raw;
            liveRaw[liveCount++] = raw;
        }
    }

    
    float panelH = titleH + pad * 2.0f + (liveCount > 0 ? liveCount * rowH : rowH);

    
    
    if (HudW <= 0.0f) {
        HudX = io.DisplaySize.x - panelW - 20.0f;
        HudY = 20.0f;
    }
    HudW = panelW;
    HudH = panelH;
    float x = HudX;
    float y = HudY;

    ImU32 surf = ApplyAlpha(C_SURFACE, m_alpha * GuiTheme::Alpha());
    ImU32 outl = ApplyAlpha(C_OUTLINE, m_alpha * GuiTheme::Alpha());
    ImU32 txt  = ApplyAlpha(C_ON_SURFACE, m_alpha * GuiTheme::Alpha());

    
    dl->AddRectFilled({ x, y }, { x + panelW, y + panelH }, surf, 14.0f, ImDrawFlags_RoundCornersAll);
    dl->AddRect({ x, y }, { x + panelW, y + panelH }, outl, 14.0f, ImDrawFlags_RoundCornersAll, 1.0f);

    
    ImGui::PushFont(ImGui::GetFont(), 16.0f);
    ImVec2 titleSz = ImGui::CalcTextSize("BrewingStands");
    dl->AddText({ x + pad, y + (titleH - titleSz.y) * 0.5f }, txt, "BrewingStands");
    ImGui::PopFont();

    float curY = y + titleH + pad;
    if (liveCount == 0) {
        dl->AddText({ x + pad, curY }, txt, "Empty");
    }
    else {
        for (int i = 0; i < liveCount; i++) {
            uint32_t raw = liveRaw[i];
            int      ticks = (int)raw;
            float    secs = ticks / 20.0f;   
            char buf[64];
            snprintf(buf, sizeof(buf), "B.Stand %.0fc", secs);
            dl->AddText({ x + pad, curY }, txt, buf);
            curY += rowH;
        }
    }
}
