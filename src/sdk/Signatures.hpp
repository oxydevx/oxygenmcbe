#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Signatures {

#define FOR_EACH_SIG(X) \
    X("ItemStackBase_getHoverName",    "55 41 57 41 56 41 54 56 57 53 48 81 EC ? ? ? ? 48 8D AC 24 ? ? ? ? 48 C7 45 ? ? ? ? ? 48 89 D6 48 8D 55") \
    X("ItemStackBase_getDamageValue",  "56 57 48 83 EC ? 48 8B 05 ? ? ? ? 48 31 E0 48 89 44 24 ? 48 8B 41 ? 48 85 C0 74 ? 48 83 38") \
    X("ScreenView_setupAndRender",     "55 41 57 41 56 41 55 41 54 56 57 53 B8 ? ? ? ? E8 ? ? ? ? 48 29 C4 48 8D AC 24 ? ? ? ? 44 0F 29 BD ? ? ? ? 44 0F 29 B5 ? ? ? ? 44 0F 29 AD ? ? ? ? 44 0F 29 A5 ? ? ? ? 44 0F 29 9D ? ? ? ? 44 0F 29 95 ? ? ? ? 44 0F 29 8D ? ? ? ? 44 0F 29 85 ? ? ? ? 0F 29 BD ? ? ? ? 0F 29 B5 ? ? ? ? 48 C7 85 ? ? ? ? ? ? ? ? 48 89 95 ? ? ? ? 48 89 CE") \
    X("MinecraftPackets_createPacket", "56 48 83 EC ? 48 89 CE 81 FA") \
    X("BrewTimer",   "8B 86 48 02 00 00 85 C0 ? ? DD ? ?") \
    X("NameProtect", "48 89 CF 0F 57 C0 0F 11 42 10 0F 11 02 48 8B 99 E8 01 00 00 48 83 B9 F0 01 00 00 10 72 09 48 8B BF D8 01 00 00 EB 07 48 81 C7 D8 01 00 00 48 85 DB 0F 88 ? ? ? ? 48 83 FB 0F 77 17 48 89 5E 10 48 C7 46 18 0F 00 00 00 0F 10 07 0F 11 06") \
    X("Hitbox",     "F3 0F 10 40 18 48 83 C4 20 5E C3 CC CC CC CC")

struct SignatureDef {
    const char* name;
    const char* pattern;
};

inline const SignatureDef g_signatures[] = {
#define X(n, p) { n, p },
    FOR_EACH_SIG(X)
#undef X
};

    void Init();

    uintptr_t Get(const std::string& name);

    const std::vector<uintptr_t>& GetAll(const std::string& name);

} 
