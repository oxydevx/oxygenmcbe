#include "PlayerInventory.hpp"
#include "ItemStack.hpp"

ItemStack* PlayerInventory::getItem(int slot) {
    Inventory* inv = getInventory();
    if (!inv) return nullptr;
    return inv->getItem(slot);
}

void PlayerInventory::selectSlot(int slot) {
    setSelectedSlot(slot);
}
