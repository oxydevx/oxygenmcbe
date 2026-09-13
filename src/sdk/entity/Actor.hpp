#pragma once
#include <cstdint>
#include "../math/MathTypes.hpp"
#include "component/Components.hpp"




namespace Oxygen {
    namespace Offsets {
        constexpr ptrdiff_t Player_StateVectorComponent    = 0x218;
        constexpr ptrdiff_t Player_AABBShapeComponent      = 0x220;
        constexpr ptrdiff_t Player_ActorRotationComponent  = 0x228;
    }

    class Actor {
    public:
        StateVectorComponent* getStateVector() {
            return reinterpret_cast<StateVectorComponent*>(
                *reinterpret_cast<uintptr_t*>(reinterpret_cast<char*>(this) + Offsets::Player_StateVectorComponent)
            );
        }

        ActorRotationComponent* getActorRotation() {
            return reinterpret_cast<ActorRotationComponent*>(
                *reinterpret_cast<uintptr_t*>(reinterpret_cast<char*>(this) + Offsets::Player_ActorRotationComponent)
            );
        }

        AABBShapeComponent* getAABBShape() {
            return reinterpret_cast<AABBShapeComponent*>(
                *reinterpret_cast<uintptr_t*>(reinterpret_cast<char*>(this) + Offsets::Player_AABBShapeComponent)
            );
        }

        Vec3* getPos() {
            auto* sv = getStateVector();
            return sv ? &sv->pos : nullptr;
        }

        Vec2* getRotation() {
            auto* arc = getActorRotation();
            return arc ? &arc->rotation : nullptr;
        }

        AABB* getBoundingBox() {
            auto* aabb = getAABBShape();
            return aabb ? &aabb->boundingBox : nullptr;
        }
    };
}


class Player : public Oxygen::Actor {
};