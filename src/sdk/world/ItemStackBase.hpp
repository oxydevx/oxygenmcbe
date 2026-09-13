#pragma once
#include <string>
#include <memory>
#include <vector>
#include "BlockLegacy.hpp"
#include "../Signatures.hpp"

class Block;
class CompoundTag;

class ItemStackBase {
public:
    class Item** item;       
    std::unique_ptr<CompoundTag> tag; 
    class Block* block;      
    short aux;               
    uint8_t itemCount;       
    bool valid;              
    char _pad0x24[0x5C];     

    std::string getHoverName();
    short getDamageValue();
    class Item* getItem();
    bool isBlock();

    virtual ~ItemStackBase() = default;
};

static_assert(sizeof(ItemStackBase) == 0x80);
