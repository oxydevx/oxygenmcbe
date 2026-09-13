#pragma once
#include <cstdint>
#include "../math/MathTypes.hpp"

class Player;

class GameMode {
public:
    Player* plr;
private:
    char pad[16];
public:
    float lastBreakProgress;
    float breakProgress;

    virtual ~GameMode() = default;

    bool startDestroyBlock(BlockPos const& pos, uint8_t face, bool& hasItem) {
        return memory::callVirtual<bool>(this, 1, pos, face, hasItem);
    }
    bool destroyBlock(BlockPos const& pos, uint8_t face) {
        return memory::callVirtual<bool>(this, 2, pos, face);
    }
    bool continueDestroyBlock(BlockPos const& pos, uint8_t face, bool& hasItem) {
        return memory::callVirtual<bool>(this, 3, pos, face, hasItem);
    }
    void stopDestroyBlock(BlockPos const& pos) {
        memory::callVirtual<void>(this, 4, pos);
    }
    void startBuildBlock(BlockPos const& pos, uint8_t face) {
        memory::callVirtual<void>(this, 5, pos, face);
    }
    bool buildBlock(BlockPos const& pos, uint8_t face) {
        return memory::callVirtual<bool>(this, 6, pos, face);
    }
    void continueBuildBlock(BlockPos const& pos, uint8_t face) {
        memory::callVirtual<void>(this, 7, pos, face);
    }
    void stopBuildBlock() {
        memory::callVirtual<void>(this, 8);
    }
    void tick() {
        memory::callVirtual<void>(this, 9);
    }
    void getPickRange(void* a, bool b) {
        memory::callVirtual<void>(this, 10, a, b);
    }
    void useItem(class ItemStack* item) {
        memory::callVirtual<void>(this, 11, item);
    }
    void useItemOn(ItemStack* item, BlockPos const& pos, unsigned char face, Vec3 const& vec, class Block const* block) {
        memory::callVirtual<void>(this, 13, item, pos, face, vec, block);
    }
    void interact(class Actor* actor, Vec3 const& vec) {
        memory::callVirtual<void>(this, 14, actor, vec);
    }
    void attack(class Actor* target) {
        memory::callVirtual<void>(this, 15, target);
    }
    void releaseUsingItem() {
       
    }
};
