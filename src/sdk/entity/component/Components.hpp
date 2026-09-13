#pragma once
#include <bitset>
#include <cstdint>
#include <array>
#include <entt/entt.hpp>
#include "../../math/MathTypes.hpp"

struct IEntityComponent {};

namespace Oxygen {
    struct StateVectorComponent : IEntityComponent {
        static constexpr uint32_t type_hash = 0x1B5D5238;
        Vec3 pos;
        Vec3 posOld;
        Vec3 velocity;
    };

    struct ActorRotationComponent : IEntityComponent {
        static constexpr uint32_t type_hash = 0x75DF36B7;
        Vec2 rotation;
        Vec2 rotationOld;
    };

    struct AABBShapeComponent : IEntityComponent {
        static constexpr uint32_t type_hash = 0xBAC1B3CF;
        AABB boundingBox;
        Vec2 size;
    };

    struct ActorDataFlagComponent : IEntityComponent {
        static constexpr uint32_t type_hash = 0xC67426F3;
        std::bitset<119> flags;
    };

    struct MoveInputState {
        bool sneakDown : 1, sneakToggleDown : 1, wantDownSlow : 1, wantUpSlow : 1;
        bool blockSelectDown : 1, ascendBlock : 1, descendBlock : 1, jumpDown : 1;
        bool sprintDown : 1, upLeft : 1, upRight : 1, downLeft : 1;
        bool downRight : 1, up : 1, down : 1, left : 1;
        bool right : 1, ascend : 1, descend : 1, changeHeight : 1;
        bool lookCenter : 1, sneakInputCurrentlyDown : 1, sneakInputWasReleased : 1, sneakInputWasPressed : 1;
        bool jumpInputWasReleased : 1, jumpInputWasPressed : 1, jumpInputCurrentlyDown : 1;
        Vec2 analogMoveVector;
        uint8_t lookSlightDirField, lookNormalDirField, lookSmoothDirField;
    };

    struct MoveInputComponent : IEntityComponent {
        static constexpr uint32_t type_hash = 0x018B1887;
        MoveInputState inputState;
        MoveInputState rawInputState;
        int8_t holdAutoJumpInWaterTicks;
        Vec2 move;
        Vec2 lookDelta;
        Vec2 interactDir;
        Vec3 displacement;
        Vec3 displacementDelta;
        Vec3 cameraOrientation;
        bool sneaking : 1, sprinting : 1, wantUp : 1, wantDown : 1;
        bool jumping : 1, autoJumpingInWater : 1, moveInputStateLocked : 1, persistSneak : 1;
        bool autoJumpEnabled : 1, isCameraRelativeMovementEnabled : 1, isRotControlledByMoveDirection : 1;
        std::array<bool, 2> isPaddling;
    };
}
