#pragma once
#include <string>
#include <cstdint>
#include "../MemoryUtil.hpp"

struct HashedString {
    uint64_t hash;
    std::string string;
};

class ItemStackBase;

class Item {
public:
    std::string& getAtlas() { return hat::member_at<std::string>(this, 0x10); }
    std::string& getTranslateName() { return hat::member_at<std::string>(this, 0xB0); }
    HashedString& getId() { return hat::member_at<HashedString>(this, 0xD0); }
    std::string& getNamespace() { return hat::member_at<std::string>(this, 0x100); }
    HashedString& getNamespacedId() { return hat::member_at<HashedString>(this, 0x120); }

    int getItemUseDuration(ItemStackBase* item) {
        using Fn = int (*)(Item*, ItemStackBase*);
        void** vtable = *reinterpret_cast<void***>(this);
        return reinterpret_cast<Fn>(vtable[5])(this, item);
    }

    bool isGlint(ItemStackBase* item) {
        using Fn = bool (*)(Item*, ItemStackBase*);
        void** vtable = *reinterpret_cast<void***>(this);
        return reinterpret_cast<Fn>(vtable[0x28])(this, item);
    }

    int getMaxDamage() {
        using Fn = int (*)(Item*);
        void** vtable = *reinterpret_cast<void***>(this);
        return reinterpret_cast<Fn>(vtable[0x24])(this);
    }

    virtual ~Item() = default;
};
