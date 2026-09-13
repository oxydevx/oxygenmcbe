#include "ItemStackBase.hpp"
#include "Item.hpp"
#include "Block.hpp"
#include "../Signatures.hpp"

std::string ItemStackBase::getHoverName() {
    using Fn = std::string& (*)(ItemStackBase*);
    auto addr = Signatures::Get("ItemStackBase_getHoverName");
    if (!addr) return {};
    return reinterpret_cast<Fn>(addr)(this);
}

short ItemStackBase::getDamageValue() {
    using Fn = short (*)(ItemStackBase*);
    auto addr = Signatures::Get("ItemStackBase_getDamageValue");
    if (!addr) return 0;
    return reinterpret_cast<Fn>(addr)(this);
}

Item* ItemStackBase::getItem() {
    if (!item) return nullptr;
    return *item;
}

bool ItemStackBase::isBlock() {
    return block != nullptr;
}
