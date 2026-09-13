#include "Keyboard.hpp"
#include <array>

namespace Hooks::Keyboard {

    static std::array<bool, 256> s_prev = {};
    static std::array<bool, 256> s_curr = {};

    void Update() {
        s_prev = s_curr;
        for (int i = 0; i < 256; i++) {
            s_curr[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
        }
    }

    bool IsKeyDown(int vk) {
        if (vk < 0 || vk >= 256) return false;
        return s_curr[vk];
    }

    bool WasKeyPressed(int vk) {
        if (vk < 0 || vk >= 256) return false;
        return s_curr[vk] && !s_prev[vk];
    }

    bool WasKeyReleased(int vk) {
        if (vk < 0 || vk >= 256) return false;
        return !s_curr[vk] && s_prev[vk];
    }

}
