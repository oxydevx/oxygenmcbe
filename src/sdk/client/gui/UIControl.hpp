#pragma once
#include <string>

class UIControl {
public:
    char pad_0000[16];
    char pad_0010[8];
    uint64_t flags;
    std::string name;
};
