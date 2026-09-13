#pragma once
#include "Packet.hpp"
#include "../math/MathTypes.hpp"

class PlayerAuthInputPacket : public Packet {
public:
    Oxygen::Vec2 mRot;
    Oxygen::Vec3 mPos;
    float mYHeadRot;
};
