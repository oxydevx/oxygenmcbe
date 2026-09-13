#pragma once
#include "Packet.hpp"

class ContainerClosePacket : public Packet {
public:
    ContainerID mContainerId;
    bool mServerInitiatedClose = false;
};
