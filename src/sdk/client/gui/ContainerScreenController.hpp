#pragma once
#include <string>
#include "../../MemoryUtil.hpp"

class ScreenController {
public:
    virtual ~ScreenController() = default;
};

class ContainerScreenController : public ScreenController {
public:
    void _handleTakePlace(const std::string& viewName, int slot, bool b) {
        memory::callVirtual<int>(this, 59, viewName, slot, b);
    }
    void* _getSelectedSlotInfo() {
        return memory::callVirtual<void*>(this, 63);
    }
};
