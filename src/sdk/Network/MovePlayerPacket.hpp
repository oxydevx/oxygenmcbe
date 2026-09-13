#pragma once
#include "Packet.hpp"
#include "../math/MathTypes.hpp"

class MovePlayerPacket : public Packet {
public:
    uint64_t mPlayerID;
    Oxygen::Vec3 mPos;
    Oxygen::Vec2 mRot;
    float mYHeadRot;
};
