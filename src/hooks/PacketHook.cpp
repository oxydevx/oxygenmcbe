#include "PacketHook.hpp"
#include "../sdk/client/ClientInstance.hpp"
#include "../sdk/Signatures.hpp"
#include "../sdk/network/ContainerOpenPacket.hpp"
#include "../features/ModuleManager.hpp"
#include "../sdk/network/PlayerAuthInputPacket.hpp"
#include "../sdk/network/MovePlayerPacket.hpp"

safetyhook::InlineHook PacketHook::m_sendToServerHook{};
safetyhook::InlineHook PacketHook::m_containerOpenHandleHook{};

void __fastcall PacketHook::hkSendToServer(LoopbackPacketSender* _this, Packet* packet) {
    m_sendToServerHook.call<void>(_this, packet);
}

void __fastcall PacketHook::hkContainerOpenHandle(void* _this, void* netId, void* callback, void* packetPtr) {
    
    
    m_containerOpenHandleHook.call<void>(_this, netId, callback, packetPtr);
}

bool PacketHook::SetupContainerOpenHook() {
    using CreatePacketFn = std::shared_ptr<Packet>(*)(PacketID);
    auto addr = Signatures::Get("MinecraftPackets_createPacket");
    if (!addr) return false;
    auto createPacket = reinterpret_cast<CreatePacketFn>(addr);
    auto realPkt = createPacket(PacketID::CONTAINER_OPEN);
    if (!realPkt || !realPkt->handler || !*realPkt->handler) return false;

    void** handlerVtable = *realPkt->handler;
    void* handleFn = handlerVtable[1];
    if (!handleFn) return false;

    try {
        m_containerOpenHandleHook = safetyhook::create_inline(handleFn, &hkContainerOpenHandle);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool PacketHook::Init() {
    ClientInstance* ci = ClientInstanceManager::getClientInstance();
    if (ci == nullptr) return false;

    auto sender = static_cast<LoopbackPacketSender*>(ci->getPacketSender());
    if (sender == nullptr) return false;

    void** vtable = *(void***)sender;
    void* sendToServer = vtable[2];

    try {
        m_sendToServerHook = safetyhook::create_inline(sendToServer, &hkSendToServer);
    }
    catch (...) {
        return false;
    }

    SetupContainerOpenHook();

    return true;
}

void PacketHook::Shutdown() {
    m_sendToServerHook.reset();
    m_containerOpenHandleHook.reset();
}