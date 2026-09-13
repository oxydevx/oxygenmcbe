#include <Windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <filesystem>
#include <exception>
#include "hooks/dxhook.hpp"
#include "utils/logger.hpp"
#include "sdk/Signatures.hpp"
#include "sdk/client/ClientInstance.hpp"
#include "utils/Loop.hpp" 
#include "utils/ProcUtils.hpp"
#include "features/ModuleManager.hpp"
#include "features/modules/visual/Notifications.hpp"

HMODULE g_hModule = nullptr;
HANDLE  g_sentinelEvent = nullptr;   



static bool g_secondInstance = false;




static std::string g_crashLogPath;
static LPTOP_LEVEL_EXCEPTION_FILTER g_prevExceptionFilter = nullptr;   
static std::terminate_handler        g_prevTerminateHandler = nullptr; 

static void WriteCrashLog(const char* msg) {
    if (g_crashLogPath.empty()) return;
    HANDLE h = CreateFileA(g_crashLogPath.c_str(), FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    WriteFile(h, msg, (DWORD)strlen(msg), &w, nullptr);
    CloseHandle(h);
}

static LONG WINAPI CrashExceptionFilter(EXCEPTION_POINTERS* ep) {
    char buf[256];
    snprintf(buf, sizeof(buf),
        "\n[CRASH] SEH exception code=0x%08lX address=0x%p\n",
        (unsigned long)(ep ? ep->ExceptionRecord->ExceptionCode : 0),
        (void*)(ep ? ep->ExceptionRecord->ExceptionAddress : nullptr));
    WriteCrashLog(buf);
    return EXCEPTION_CONTINUE_SEARCH; 
}

static void CrashTerminate() {
    WriteCrashLog("\n[CRASH] std::terminate called (uncaught C++ exception)\n");
    abort();
}



DWORD WINAPI MainThread(LPVOID lpParam) {
#ifdef _DEBUG
    FILE* fDummy;
    AllocConsole();
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    std::cout << "[*] Oxygen loading..." << std::endl;
#endif

    
    {
        Logger::Init();
        g_crashLogPath = Logger::GetLogPath();
    }

    if (g_secondInstance) {
        Logger::WarnTag("MainThread",
            "first copy is still mapped (module 0x%p); unloading this duplicate instance",
            (void*)GetModuleHandleW(L"Oxygen.dll"));
        FreeLibraryAndExitThread(g_hModule, 0);
        return 0; 
    }

    
    g_prevExceptionFilter = SetUnhandledExceptionFilter(CrashExceptionFilter);
    g_prevTerminateHandler = std::set_terminate(CrashTerminate);

    
    Signatures::Init();

    
    
    
    if (ClientInstanceManager::Init()) {
        Logger::InfoTag("MainThread", "ClientInstanceManager initialized");
    } else {
        Logger::ErrorTag("MainThread",
            "Failed to initialize ClientInstanceManager (ScreenView_setupAndRender signature did not resolve)");
    }

    Logger::InfoTag("MainThread", "Initializing DX12 Hooks...");
    if (!Hooks::DX12::Init()) {
        Logger::ErrorTag("MainThread", "Failed to initialize DX12 Hooks");
        return 1;
    }

    Logger::InfoTag("MainThread", "Initializing ModuleManager...");
    ModuleManager::Initialize();

    
    
    
    if (Config::Exists("default")) {
        Config::Load("default");
    } else {
        Config::Save("default");
        Logger::InfoTag("MainThread", "created default config");
    }

    
    
    for (auto& f : ModuleManager::FeatureList) {
        if (f->Name != "NameProtect" && f->Name != "Test" && f->Name != "ESP")
            continue;
        if (f->Enabled) {
            f->Enabled = false;
            f->OnDisabled();
        }
    }

    Notifications::Push("Oxygen", "Successfully injected!",
        Notifications::Type::Info);
    Logger::InfoTag("MainThread", "Oxygen Ready");

    
    while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
        Sleep(100);
    }

#ifdef _DEBUG
    std::cout << "[*] Shutting down..." << std::endl;
#endif

    Logger::InfoTag("MainThread", "unload requested (End key)");

    
    
    Config::Save("default");

    
    
    
    ModuleManager::Shutdown();
    
    ClientInstanceManager::Shutdown();
    
    
    Hooks::DX12::Shutdown();

    
    
    SetUnhandledExceptionFilter(g_prevExceptionFilter);
    std::set_terminate(g_prevTerminateHandler);

#ifdef _DEBUG
    if (fDummy) fclose(fDummy);
    FreeConsole();
#endif
    (void)lpParam;

    
    
    
    
    
    Logger::InfoTag("MainThread", "teardown complete; releasing sentinel + unloading module");
    WriteCrashLog("\n[SHUTDOWN] module unloaded after End key (hooks released, threads stopped)\n");
    
    
    
    
    if (g_sentinelEvent) { CloseHandle(g_sentinelEvent); g_sentinelEvent = nullptr; }
    FreeLibraryAndExitThread(g_hModule, 0);
    return 0; 
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        
        
        
        
        const HANDLE h = CreateEventW(nullptr, TRUE, FALSE, L"Local\\OxygenLoaded");
        if (h && GetLastError() == ERROR_ALREADY_EXISTS) {
            g_secondInstance = true;
            CloseHandle(h);
        } else {
            g_sentinelEvent = h;
        }
        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        
        
        
        
        
        ModuleManager::StopWorkers();
        
        
        if (g_sentinelEvent) { CloseHandle(g_sentinelEvent); g_sentinelEvent = nullptr; }
        
        
        if (!g_crashLogPath.empty())
            WriteCrashLog("\n[SHUTDOWN] DLL_PROCESS_DETACH: module unloaded\n");
    }
    return TRUE;
}