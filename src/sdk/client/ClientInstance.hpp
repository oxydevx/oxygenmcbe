#pragma once
#include <Windows.h>
#include <safetyhook.hpp>

class ClientInstance;
class LocalPlayer;
class PacketSender;
class Level;
class ScreenView;

class ClientInstance {
public:
    PacketSender* getPacketSender() {
        using Fn = PacketSender * (*)(void*);
        void** vtable = *reinterpret_cast<void***>(this);
        return reinterpret_cast<Fn>(vtable[293])(this);
    }

    LocalPlayer* getLocalPlayer() {
        using Fn = LocalPlayer * (*)(void*);
        void** vtable = *reinterpret_cast<void***>(this);
        return reinterpret_cast<Fn>(vtable[31])(this);
    }

    Level* getLevel() {
        using Fn = Level * (*)(void*);
        void** vtable = *reinterpret_cast<void***>(this);
        return reinterpret_cast<Fn>(vtable[186])(this);
    }

    void grabMouse() {
        using Fn = void (*)(void*);
        void** vtable = *reinterpret_cast<void***>(this);
        reinterpret_cast<Fn>(vtable[310])(this);
    }

    void releaseMouse() {
        using Fn = void (*)(void*);
        void** vtable = *reinterpret_cast<void***>(this);
        reinterpret_cast<Fn>(vtable[311])(this);
    }
};

class ClientInstanceManager {
public:
    static bool Init();
    static void Shutdown();

    static bool IsShuttingDown();

    static ClientInstance* getClientInstance();
    static LocalPlayer* getLocalPlayer();
    static class Actor* getCrosshairTarget();
    static ScreenView* getScreenView();

private:
    static safetyhook::InlineHook m_SetupAndRenderHook;
    static void __fastcall hkSetupAndRender(void* a1, void* a2);

    static ClientInstance* m_pClientInstance;
    static LocalPlayer* m_pLocalPlayer;
    static class Actor* m_pCrosshairTarget;
    static ScreenView* m_pScreenView;
};
