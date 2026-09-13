#pragma once
#include "Packet.hpp"
#include <memory>
#include <vector>

enum class InventorySourceType : int {
    Invalid = -1,
    ContainerInventory = 0,
    GlobalInventory = 1,
    WorldInteraction = 2,
    CreativeInventory = 3,
};

struct InventorySource {
    InventorySourceType mType = InventorySourceType::Invalid;
    int mContainerId = 0;
    int mBitFlags = 0;
};

class ItemStack;

struct InventoryAction {
    InventorySource mSource;
    int mSlot = 0;
    ItemStack* mFrom = nullptr;
    ItemStack* mTo = nullptr;

    InventoryAction() = default;
    InventoryAction(int slot, ItemStack* from, ItemStack* to)
        : mSlot(slot), mFrom(from), mTo(to) {}
};

struct InventoryTransaction {
    std::vector<InventoryAction> mActions;
    void addAction(const InventoryAction& action) { mActions.push_back(action); }
};

class ComplexInventoryTransaction {
public:
    enum class Type : int {
        Normal = 0,
        Mismatch = 1,
        ItemUse = 2,
        ItemUseOnEntity = 3,
        ReleaseItem = 4,
    };

    Type type = Type::Normal;
    InventoryTransaction data;
};

class InventoryTransactionPacket : public Packet {
public:
    std::unique_ptr<ComplexInventoryTransaction> mTransaction;
};
