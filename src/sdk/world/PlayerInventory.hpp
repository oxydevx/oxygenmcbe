#pragma once
#include "Inventory.hpp"

class PlayerInventory {
public:
    int getSelectedSlot() { return hat::member_at<int>(this, 0x10); } 
    void setSelectedSlot(int slot) { hat::member_at<int>(this, 0x10) = slot; } 

    Inventory* getInventory() { return hat::member_at<Inventory*>(this, 0xB8); } 

    ItemStack* getItem(int slot);
    void selectSlot(int slot);
};
