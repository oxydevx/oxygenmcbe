#pragma once
#include <string>
#include <cstdint>

struct StringHash {
    uint64_t hash;
    std::string string;
};

class BlockLegacy {
public:
    std::string& getTranslateName() { return *reinterpret_cast<std::string*>(reinterpret_cast<char*>(this) + 0x8); }
    StringHash& getName() { return *reinterpret_cast<StringHash*>(reinterpret_cast<char*>(this) + 0x88); }
    std::string& getNamespace() { return *reinterpret_cast<std::string*>(reinterpret_cast<char*>(this) + 0xB8); }
    StringHash& getNamespacedId() { return *reinterpret_cast<StringHash*>(reinterpret_cast<char*>(this) + 0xD8); }
    std::string& getItemGroup() { return *reinterpret_cast<std::string*>(reinterpret_cast<char*>(this) + 0x170); }

    bool isBlock() { return true; }

private:
    virtual ~BlockLegacy() = default;
};
