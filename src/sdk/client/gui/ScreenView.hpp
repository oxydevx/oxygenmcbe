#pragma once
#include "VisualTree.hpp"

class ClientInstance;
class ScreenController;

class ScreenView {
public:
    char pad_0000[8];
    ClientInstance* clientInstance;
    char pad_0010[0x28];
    ScreenController* screenController;
    char pad_0040[0x08];
    VisualTree* visualTree;
};
