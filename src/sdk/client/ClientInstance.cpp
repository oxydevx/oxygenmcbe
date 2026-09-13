#include "ClientInstance.hpp"
#include "gui/ScreenView.hpp"
#include "../Signatures.hpp"
#include "../level/Level.hpp"
#include "../features/ModuleManager.hpp"
#include "../hooks/PacketHook.hpp"
#include "../utils/logger.hpp"
#include <iostream>
#include <atomic>

safetyhook::InlineHook ClientInstanceManager::m_SetupAndRenderHook{};
ClientInstance* ClientInstanceManager::m_pClientInstance = nullptr;
LocalPlayer* ClientInstanceManager::m_pLocalPlayer = nullptr;
Actor* ClientInstanceManager::m_pCrosshairTarget = nullptr;
ScreenView* ClientInstanceManager::m_pScreenView = nullptr;

namespace {
    
    
    
    std::atomic<bool> s_shuttingDown{false};
}

bool ClientInstanceManager::IsShuttingDown() {
    return s_shuttingDown.load(std::memory_order_acquire);
}

ClientInstance* ClientInstanceManager::getClientInstance() { return m_pClientInstance; }
LocalPlayer* ClientInstanceManager::getLocalPlayer() { return m_pLocalPlayer; }
Actor* ClientInstanceManager::getCrosshairTarget() { return m_pCrosshairTarget; }
ScreenView* ClientInstanceManager::getScreenView() { return m_pScreenView; }

void __fastcall ClientInstanceManager::hkSetupAndRender(void* a1, void* a2) {
    if (s_shuttingDown.load(std::memory_order_acquire)) {
        m_SetupAndRenderHook.call<void>(a1, a2);
        return;
    }
    static bool s_logHookFired = false;
    if (!s_logHookFired) {
        s_logHookFired = true;
        Logger::InfoTag("SetupAndRender", "hook firing: screenView=0x%p", a2);
    }
    if (a2 != nullptr) {
        m_pScreenView = reinterpret_cast<ScreenView*>(a2);
        ClientInstance* ci = *reinterpret_cast<ClientInstance**>(reinterpret_cast<char*>(a2) + 0x8);
        if (ci != nullptr) {
            m_pClientInstance = ci;

            static bool s_logCi = false;
            if (!s_logCi) {
                s_logCi = true;
                Logger::InfoTag("SetupAndRender", "clientInstance=0x%p", (void*)ci);
            }

            static bool packetHooked = false;
            if (!packetHooked) {
                packetHooked = PacketHook::Init();
            }

            Level* currentLevel = ci->getLevel();
            Level::SetCurrent(currentLevel);

            static bool s_logLevel = false;
            if (!s_logLevel) {
                s_logLevel = true;
                Logger::InfoTag("SetupAndRender", "level=0x%p", (void*)currentLevel);
            }

            LocalPlayer* localplayer = ci->getLocalPlayer();
            if (localplayer != nullptr) {
                m_pLocalPlayer = localplayer;

                static bool s_logLocal = false;
                if (!s_logLocal) {
                    s_logLocal = true;
                    Logger::InfoTag("SetupAndRender", "localPlayer=0x%p", (void*)localplayer);
                }

                Actor* currentTarget = reinterpret_cast<Actor*>(
                    *reinterpret_cast<uintptr_t*>(reinterpret_cast<char*>(localplayer) + 0x9E0)
                );
                if (currentTarget != nullptr && currentTarget != reinterpret_cast<Actor*>(localplayer)) {
                    m_pCrosshairTarget = currentTarget;
                } else {
                    m_pCrosshairTarget = nullptr;
                }
            }

            ModuleManager::CallEvent();
        }
    }

    m_SetupAndRenderHook.call<void>(a1, a2);
}

bool ClientInstanceManager::Init() {
    auto addr = Signatures::Get("ScreenView_setupAndRender");
    if (!addr) return false;

    try {
        m_SetupAndRenderHook = safetyhook::create_inline(reinterpret_cast<void*>(addr), &hkSetupAndRender);
    } catch (const std::exception& e) {
        return false;
    }

    return true;
}

void ClientInstanceManager::Shutdown() {
    s_shuttingDown.store(true, std::memory_order_release);
    
    
    
    Sleep(50);
    m_SetupAndRenderHook.reset();
    PacketHook::Shutdown();
}
