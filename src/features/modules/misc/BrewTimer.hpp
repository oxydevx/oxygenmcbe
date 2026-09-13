#pragma once

#include "../../Feature.hpp"
#include <imgui.h>
#include <windows.h>





class BrewTimer : public Feature {
public:
    BrewTimer();
    void OnEvent()    override;
    void OnEnabled()  override;
    void OnDisabled() override;

private:
    
    static constexpr unsigned C_SURFACE        = 0xFF1D1B20;
    static constexpr unsigned C_OUTLINE        = 0x2A49454F;
    static constexpr unsigned C_ON_SURFACE      = 0xFFE6E1E5;
    static constexpr unsigned C_ON_SURFACE_DIM  = 0xFFCAC4D0;
    static constexpr unsigned C_PRIMARY        = 0xFF6750A4;

    
    static constexpr DWORD OFFSET = 0x248;
    static constexpr int   MAX_STANDS = 64;

    float m_alpha = 0.0f;

    
    
    
    static PVOID               s_veh;
    static DWORD_PTR           s_targetAddr;
    static BYTE                s_originalByte;
    static DWORD               s_origProtect;   
    static bool                s_armed;        
    static bool                s_installed;
    static LONG                s_standCount;
    static DWORD_PTR           s_stands[MAX_STANDS]; 

    static LONG WINAPI VehHandler(PEXCEPTION_POINTERS ep);
    static uint32_t  ReadRaw(DWORD_PTR base);
    static ImU32     ApplyAlpha(ImU32 color, float a);

    void InstallHook();
    void RemoveHook();
};
