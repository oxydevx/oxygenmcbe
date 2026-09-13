#include "Hitbox.hpp"
#include <windows.h>
#include <tlhelp32.h>

#include "../../sdk/Signatures.hpp"


















namespace {

constexpr ptrdiff_t HWBP_OFFSET = 0x18;

Hitbox*   g_instance   = nullptr;
uintptr_t g_targetAddr = 0;
PVOID     g_vehHandle  = nullptr;
HANDLE    g_armThread  = nullptr;
volatile bool g_running    = true;
volatile bool g_armStarted = false;

bool IsWritable(uintptr_t p) {
    if (p < 0x10000) return false;
    MEMORY_BASIC_INFORMATION mbi = {0};
    if (VirtualQuery((LPCVOID)p, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
    if (mbi.State != MEM_COMMIT) return false;
    DWORD prot = mbi.Protect & 0xFF; 
    return prot == PAGE_READWRITE || prot == PAGE_WRITECOPY ||
           prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY;
}

LONG NTAPI HitboxVeh(PEXCEPTION_POINTERS info) {
    if (info->ExceptionRecord->ExceptionCode != STATUS_SINGLE_STEP)
        return EXCEPTION_CONTINUE_SEARCH;
    if ((uintptr_t)info->ExceptionRecord->ExceptionAddress != g_targetAddr)
        return EXCEPTION_CONTINUE_SEARCH;

    auto* ctx = info->ContextRecord;
    uintptr_t rax = ctx->Rax;

    
    
    ctx->EFlags |= 0x10000;

    if (g_instance && g_instance->Enabled && rax != 0) {
        uintptr_t target = rax + HWBP_OFFSET;
        if (IsWritable(target)) {
            float* p = reinterpret_cast<float*>(target);
            __try {
                *p = g_instance->TargetValue;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                
            }
        }
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}



void SetDrBreakpoint(HANDLE h, uintptr_t addr) {
    CONTEXT ctx; ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(h, &ctx)) return;
    ctx.Dr0 = addr; ctx.Dr1 = 0; ctx.Dr2 = 0; ctx.Dr3 = 0; ctx.Dr6 = 0;
    ctx.Dr7 = 1; 
    SetThreadContext(h, &ctx);
}


void ArmAllThreads(uintptr_t addr) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te; te.dwSize = sizeof(te);
    DWORD pid = GetCurrentProcessId();
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID != pid) continue;
            if (te.th32ThreadID == GetCurrentThreadId()) continue;
            HANDLE h = OpenThread(
                THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME,
                FALSE, te.th32ThreadID);
            if (!h) continue;
            if (SuspendThread(h) != (DWORD)-1) {
                SetDrBreakpoint(h, addr);
                ResumeThread(h);
            }
            CloseHandle(h);
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
}

DWORD WINAPI ArmLoopProc(LPVOID) {
    while (g_running && !g_targetAddr) Sleep(50);
    while (g_running) {
        __try {
            if (g_targetAddr) {
                SetDrBreakpoint(GetCurrentThread(), g_targetAddr);
                ArmAllThreads(g_targetAddr);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            
        }
        Sleep(2000);
    }
    return 0;
}

void Install() {
    if (g_vehHandle) return; 
    g_targetAddr = Signatures::Get("Hitbox");
    if (!g_targetAddr) return;

    g_vehHandle  = AddVectoredExceptionHandler(1, HitboxVeh);
    
    SetDrBreakpoint(GetCurrentThread(), g_targetAddr);
}



void ClearAllDebugRegisters() {
    __try {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) return;
        THREADENTRY32 te; te.dwSize = sizeof(te);
        DWORD pid = GetCurrentProcessId();
        if (Thread32First(snap, &te)) {
            do {
                if (te.th32OwnerProcessID != pid) continue;
                HANDLE h = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                                      FALSE, te.th32ThreadID);
                if (!h) continue;
                CONTEXT ctx; ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                if (GetThreadContext(h, &ctx)) {
                    ctx.Dr0 = ctx.Dr1 = ctx.Dr2 = ctx.Dr3 = 0;
                    ctx.Dr6 = 0; ctx.Dr7 = 0;
                    SetThreadContext(h, &ctx);
                }
                CloseHandle(h);
            } while (Thread32Next(snap, &te));
        }
        CloseHandle(snap);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void EnsureArmWorker() {
    if (g_armStarted) return;
    g_armStarted = true;
    g_armThread = CreateThread(nullptr, 0, ArmLoopProc, nullptr, 0, nullptr);
}

} 



Hitbox::Hitbox() : Feature("Hitbox", Category::Combat) {
    g_instance = this;
    this->Description = "Change hitbox width other players";
    this->Enabled     = false;
    this->Keybind     = 0;
    this->Visibility  = true;
    this->CallAllTime = true;
    AddSlider("Size", &TargetValue, 0.6f, 5.0f, "%.1f");
    Install();
}

void Hitbox::Cleanup() {
    g_running = false;
    if (g_armThread) {
        
        
        
        WaitForSingleObject(g_armThread, 3000);
        CloseHandle(g_armThread);
        g_armThread = nullptr;
    }
    g_armStarted = false;
    
    
    ClearAllDebugRegisters();
    if (g_vehHandle) {
        RemoveVectoredExceptionHandler(g_vehHandle);
        g_vehHandle = nullptr;
    }
    g_instance = nullptr;
}

void Hitbox::OnEnabled() {
    Install();
}

void Hitbox::OnDisabled() {
    Cleanup();
}

Hitbox::~Hitbox() {
    Cleanup();
}

void Hitbox::OnEvent() {
    
    EnsureArmWorker();
}
