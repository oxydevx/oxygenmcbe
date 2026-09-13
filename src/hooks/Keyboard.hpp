#pragma once
#include <Windows.h>

namespace Hooks::Keyboard {
    void Update();
    bool IsKeyDown(int vk);
    bool WasKeyPressed(int vk);
    bool WasKeyReleased(int vk);
}
