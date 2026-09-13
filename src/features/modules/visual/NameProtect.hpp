#pragma once

#include "../../Feature.hpp"
#include <windows.h>




class NameProtect : public Feature {
public:
    NameProtect();
    void OnEvent()    override;
    void OnEnabled()  override;
    void OnDisabled() override;

private:
    char m_nick[17] = { 0 };   

    static constexpr int MAX_TARGETS = 16;

    
    static PVOID     s_veh;
    static DWORD_PTR s_targets[MAX_TARGETS];
    static BYTE      s_origBytes[MAX_TARGETS];
    static DWORD     s_origProtect[MAX_TARGETS];
    static int       s_targetCount;
    static bool      s_installed;
    static DWORD_PTR s_armedTarget;   
    static DWORD_PTR s_armedRdi;      
    static DWORD_PTR s_armedRsi;      
    static DWORD_PTR s_nameAddr;      
    static bool      s_nameAddrValid;
    static DWORD_PTR s_nameAddr2;     
    static bool      s_nameAddr2Valid;
    static char      s_origName[16];  
    static bool      s_origNameSaved;
    static char      s_nickBuf[16];   

    static LONG WINAPI VehHandler(PEXCEPTION_POINTERS ep);
    static void      InstallHook();
    static void      RemoveHook();
};
