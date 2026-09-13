#include "NameProtect.hpp"
#include "../../ModuleManager.hpp"
#include "../../../utils/logger.hpp"
#include "../../../sdk/Signatures.hpp"

#include <windows.h>
#include <cstring>




PVOID     NameProtect::s_veh         = nullptr;
DWORD_PTR NameProtect::s_targets[MAX_TARGETS] = { 0 };
BYTE      NameProtect::s_origBytes[MAX_TARGETS] = { 0 };
DWORD     NameProtect::s_origProtect[MAX_TARGETS] = { 0 };
int       NameProtect::s_targetCount = 0;
bool      NameProtect::s_installed  = false;
DWORD_PTR NameProtect::s_armedTarget = 0;
DWORD_PTR NameProtect::s_armedRdi    = 0;   
DWORD_PTR NameProtect::s_armedRsi    = 0;   
DWORD_PTR NameProtect::s_nameAddr    = 0;   
bool      NameProtect::s_nameAddrValid = false;
DWORD_PTR NameProtect::s_nameAddr2   = 0;   
bool      NameProtect::s_nameAddr2Valid = false;
char      NameProtect::s_origName[16] = { 0 };
bool      NameProtect::s_origNameSaved = false;
char      NameProtect::s_nickBuf[16] = { 0 };


static void PatchBuffer(DWORD_PTR addr, const char* nick) {
    if (!addr || !nick || !nick[0]) return;
    __try {
        DWORD old = 0;
        if (VirtualProtect((LPVOID)addr, 16, PAGE_EXECUTE_READWRITE, &old)) {
            memcpy((void*)addr, nick, 16);
            VirtualProtect((LPVOID)addr, 16, old, &old);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {   }
}




NameProtect::NameProtect() : Feature("NameProtect", Category::Visual) {
    this->Description = "Spoofs player names with your chosen nickname";
    this->Enabled     = false;   
    this->Keybind     = 0;
    this->Visibility  = false;
    
    this->AddText("Nickname", m_nick, 16);
}

void NameProtect::OnEnabled()  { InstallHook(); }
void NameProtect::OnDisabled() { RemoveHook(); }

void NameProtect::OnEvent() {
    if (!Enabled) return;
    
    
    for (int i = 0; i < 16; i++)
        s_nickBuf[i] = (i < 15 && m_nick[i] != 0) ? m_nick[i] : 0;

    
    static bool s_diag = false;
    if (!s_diag) {
        s_diag = true;
        Logger::InfoTag("NameProtect",
            "OnEvent diag: nick='%s' rdiAddr=%p rsiAddr=%p",
            s_nickBuf, (void*)s_nameAddr, (void*)s_nameAddr2);
    }

    
    
    
    
    
    
    
}






LONG WINAPI NameProtect::VehHandler(PEXCEPTION_POINTERS ep) {
    if (!ep || !ep->ExceptionRecord || !ep->ContextRecord)
        return EXCEPTION_CONTINUE_SEARCH;

    DWORD code = ep->ExceptionRecord->ExceptionCode;

    if (code == STATUS_BREAKPOINT) {
        DWORD_PTR ip, rdi, rsi;
#ifdef _WIN64
        ip  = ep->ContextRecord->Rip;
        rdi = ep->ContextRecord->Rdi;
        rsi = ep->ContextRecord->Rsi;
#else
        ip  = ep->ContextRecord->Eip;
        rdi = ep->ContextRecord->Edi;
        rsi = ep->ContextRecord->Esi;
#endif

        for (int i = 0; i < s_targetCount; i++) {
            if (s_targets[i] == ip) {
                
                
                *(BYTE*)s_targets[i] = s_origBytes[i];
                s_armedTarget = s_targets[i];
                s_armedRdi    = rdi;   
                s_armedRsi    = rsi;   

                
                
                
                if (rdi != 0 && !s_nameAddrValid) {
                    memcpy(s_origName, (void*)rdi, 16);  
                    s_origNameSaved = true;
                    s_nameAddr = rdi; s_nameAddrValid = true;
                }
                if (rsi != 0 && !s_nameAddr2Valid) {
                    s_nameAddr2 = rsi; s_nameAddr2Valid = true;
                }

                static bool s_logged = false;
                if (!s_logged) {
                    s_logged = true;
                    Logger::InfoTag("NameProtect",
                        "live name-copy: ip=%p rdi=%p rsi=%p",
                        (void*)ip, (void*)rdi, (void*)rsi);
                }

                ep->ContextRecord->EFlags |= 0x100; 
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
    else if (code == STATUS_SINGLE_STEP) {
        if (s_armedTarget != 0) {
            DWORD_PTR tgt  = s_armedTarget;
            DWORD_PTR src  = s_armedRdi;
            DWORD_PTR dest = s_armedRsi;
            s_armedTarget = 0;
            s_armedRdi    = 0;
            s_armedRsi    = 0;

            
            if (s_nickBuf[0] != 0) {
                ep->ContextRecord->Xmm0.Low  = *(ULONG64*)&s_nickBuf[0];
                ep->ContextRecord->Xmm0.High = *(ULONG64*)&s_nickBuf[8];
                if (src != 0)  { s_nameAddr = src;  s_nameAddrValid = true; }
                if (dest != 0) { s_nameAddr2 = dest; s_nameAddr2Valid = true; }
            }

            
            *(BYTE*)tgt = 0xCC;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}




void NameProtect::InstallHook() {
    if (s_installed) return;

    
    
    
    
    const DWORD_PTR HOOK_OFFSET = 0x49;

    const auto& matches = Signatures::GetAll("NameProtect");
    s_targetCount = 0;
    for (auto m : matches) {
        if (s_targetCount >= MAX_TARGETS) break;
        s_targets[s_targetCount++] = m + HOOK_OFFSET;
    }
    if (s_targetCount == 0) {
        Logger::WarnTag("NameProtect", "name-copy pattern not found; module disabled");
        return;
    }

    for (int i = 0; i < s_targetCount; i++) {
        s_origBytes[i] = *(BYTE*)s_targets[i];
        
        
        
        VirtualProtect((LPVOID)s_targets[i], 1, PAGE_EXECUTE_READWRITE, &s_origProtect[i]);
        *(BYTE*)s_targets[i] = 0xCC; 
    }

    s_veh = AddVectoredExceptionHandler(1, VehHandler);
    s_installed = (s_veh != nullptr);

    if (!s_installed) {
        Logger::ErrorTag("NameProtect", "failed to register VEH");
        return;
    }
    Logger::InfoTag("NameProtect", "hooked %d name-copy site(s)", s_targetCount);
}

void NameProtect::RemoveHook() {
    if (!s_installed) return;

    if (s_veh) RemoveVectoredExceptionHandler(s_veh);
    s_veh = nullptr;

    for (int i = 0; i < s_targetCount; i++) {
        if (s_targets[i]) {
            *(BYTE*)s_targets[i] = s_origBytes[i];
            DWORD old;
            VirtualProtect((LPVOID)s_targets[i], 1, s_origProtect[i], &old);
        }
    }

    
    if (s_origNameSaved && s_nameAddr) {
        DWORD old = 0;
        if (VirtualProtect((LPVOID)s_nameAddr, 16, PAGE_EXECUTE_READWRITE, &old)) {
            memcpy((void*)s_nameAddr, s_origName, 16);
            VirtualProtect((LPVOID)s_nameAddr, 16, old, &old);
        }
    }

    s_targetCount    = 0;
    s_armedTarget    = 0;
    s_nameAddr       = 0;
    s_nameAddrValid  = false;
    s_nameAddr2      = 0;
    s_nameAddr2Valid = false;
    s_origNameSaved  = false;
    s_installed      = false;
}
