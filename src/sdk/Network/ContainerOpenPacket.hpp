#pragma once
#include "Packet.hpp"
#include "../math/MathTypes.hpp"

class ContainerOpenPacket : public Packet {
public:
    ContainerID mContainerId;
    ContainerType mType;
    Vec3i mPos;
    int64_t mEntityUniqueId;
};
