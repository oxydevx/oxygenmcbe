#pragma once
#include <windows.h>

namespace GameUtils {




constexpr int kForEachPlayerVtableIndex = 0;

inline void SafeForEachPlayer(void* currentLevel, void** vtable, void* callbackPtr) {
    using ForEachPlayerFn = void(__fastcall*)(void*, void*);
    if (kForEachPlayerVtableIndex == 0) return;
    __try {
        ForEachPlayerFn oForEachPlayer = reinterpret_cast<ForEachPlayerFn>(vtable[kForEachPlayerVtableIndex]);
        oForEachPlayer(currentLevel, callbackPtr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

}