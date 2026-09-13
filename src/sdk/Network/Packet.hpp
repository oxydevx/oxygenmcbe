#pragma once
#include <cstdint>
#include <string>

enum class ContainerID : uint8_t {
    None = 0,
    Inventory = 0,
    First = 1,
    Last = 100,
    Offhand = 119,
    Armor = 120,
    SelectSlot = 121,
    Chest = 127,
    UI = 124,
};

enum class ContainerType : int {
    None = 0,
    Inventory = 1,
    Chest = 2,
    CraftingTable = 3,
    Furnace = 4,
    EnchantingTable = 5,
    BrewingStand = 6,
    Anvil = 7,
    Dispenser = 8,
    Dropper = 9,
    Hopper = 10,
    Cauldron = 11,
    MinecartChest = 12,
    MinecartHopper = 13,
    HorseInventory = 14,
    Beacon = 15,
};

enum class PacketID : uint8_t {
    NONE = 0,
    LOGIN = 0x1,
    TEXT = 0x9,
    ACTOR_EVENT = 0x1B,
    SET_ENTITY_DATA = 0x27,
    MOVE_PLAYER = 0x13,
    CHANGE_DIMENSION = 0x3D,
    TRANSFER = 0x55,
    SET_TITLE = 0x58,
    COMMAND_REQUEST = 0x4D,
    TOAST_REQUEST = 0xBA,
    MODAL_FORM_REQUEST = 0x64,
    SET_SCORE = 0x6C,
    PLAYER_AUTH_INPUT = 0x90,
    CONTAINER_OPEN = 0x2F,
    CONTAINER_CLOSE = 0x30,
    INVENTORY_TRANSACTION = 0x4B,
    COUNT,
};

class Packet {
public:
    int32_t priority = 2;
    int32_t reliability = 1;
    uint8_t subClientId = 0;
    bool isHandled = false;
    void* unknown = nullptr;
    void*** handler = nullptr;
    int32_t compressibility = 0;

    virtual ~Packet() = default;
    virtual PacketID getID() { return PacketID::NONE; }
    virtual std::string getName() { return ""; }
    virtual void write(void* stream) {}
    virtual void readExtended(void* stream) {}
    virtual bool allowBatch() { return false; }
    virtual void _read(void* stream) {}
};
