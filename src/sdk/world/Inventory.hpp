#pragma once
#include "ItemStack.hpp"
#include "../math/MathTypes.hpp"
#include <vector>

class Inventory {
public:
    virtual ~Inventory() = default;
    virtual void init() = 0;
    virtual void unk() = 0;
    virtual void addContentChangeListener(class ContainerContentChangeListener*) = 0;
    virtual void removeContentChangeListener(class ContainerContentChangeListener*) = 0;
    virtual void addRemovedListener(class ContainerRemovedListener*) = 0;
    virtual void removeRemovedListener(class ContainerRemovedListener*) = 0;
    virtual ItemStack* getItem(int slot) = 0;
    virtual bool hasRoomForItem(ItemStack const&) = 0;
    virtual void addItem(ItemStack&) = 0;
    virtual char addItemToFirstEmptySlot(ItemStack&) = 0;
    virtual void setItem(int, ItemStack const&) = 0;
    virtual void setItemWithForceBalance(int, ItemStack const&, bool) = 0;
    virtual void removeItem(int, int) = 0;
    virtual void removeAllItems() = 0;
    virtual void dropContents(class BlockSource&, Vec3 const&, bool) = 0;
    virtual int getContainerSize() = 0;
    virtual long long getMaxStackSize() = 0;
    virtual void nullsub_7() = 0;
    virtual void nullsub_8() = 0;
    virtual void getSlotCopies() = 0;
    virtual std::vector<ItemStack const*> getSlots() = 0;
    virtual int getItemCount(ItemStack const&) = 0;
    virtual int findFirstSlotForItem(ItemStack const&) = 0;
    virtual bool nullsub_9() = 0;
    virtual bool nullsub_10() = 0;
    virtual void setContainerChanged(int) = 0;
    virtual void setContainerMoved() = 0;
    virtual void** setCustomName(std::string const&) = 0;
    virtual bool hasCustomName() = 0;
    virtual void readAdditionalSaveData(class CompoundTag const&) = 0;
    virtual void addAdditionalSaveData(class CompoundTag&) = 0;
    virtual void createTransactionContext(std::function<void(Inventory&, int, ItemStack const&, ItemStack const&)>, std::function<void()>) = 0;
    virtual void nullsub_11() = 0;
    virtual bool isEmpty() = 0;
};
