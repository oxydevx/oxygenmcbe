#pragma once
#include "ItemStackBase.hpp"

class ItemStack : public ItemStackBase {
private:
    uintptr_t netIds;
};
