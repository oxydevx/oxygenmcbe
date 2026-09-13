#pragma once
#include <safetyhook.hpp>
#include "../sdk/network/Packet.hpp"
#include "../sdk/network/LoopbackPacketSender.hpp"

class PacketHook {
public:
    static bool Init();
    static void Shutdown();

private:
    static safetyhook::InlineHook m_sendToServerHook;
    static void __fastcall hkSendToServer(LoopbackPacketSender* _this, Packet* packet);

    static safetyhook::InlineHook m_containerOpenHandleHook;
    static void __fastcall hkContainerOpenHandle(void* _this, void* netId, void* callback, void* packetPtr);
    static bool SetupContainerOpenHook();
};
