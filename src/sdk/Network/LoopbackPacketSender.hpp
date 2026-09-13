#pragma once
#include "Packet.hpp"

class PacketSender {
public:
    virtual ~PacketSender() = default;
    virtual bool isInitialized() = 0;
    virtual void send(Packet*) = 0;
    virtual void sendTo(void* networkIdentifier, uint8_t subClientId, Packet* pkt) = 0;
    virtual void sendToServer(Packet* pkt) = 0;
};

class LoopbackPacketSender : public PacketSender {
};
