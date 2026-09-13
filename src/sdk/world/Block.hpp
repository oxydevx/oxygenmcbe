#pragma once
#include "BlockLegacy.hpp"

class Block {
public:
    BlockLegacy* getLegacyBlock() { return hat::member_at<BlockLegacy*>(this, 0x68); }

    virtual ~Block() = default;
    virtual int getRenderLayer() const = 0;
};
