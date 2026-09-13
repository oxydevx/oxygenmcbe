#pragma once
#include <cstdint>

namespace memory {
    template <typename TRet, typename... TArgs>
    TRet callVirtual(void* thisptr, size_t index, TArgs... argList) {
        using TFunc = TRet(__fastcall*)(void*, TArgs...);
        TFunc* vtable = *reinterpret_cast<TFunc**>(thisptr);
        return vtable[index](thisptr, argList...);
    }
}

namespace hat {
    template <typename T, typename U>
    constexpr T& member_at(U* ptr, size_t offset) {
        return *reinterpret_cast<T*>(reinterpret_cast<char*>(ptr) + offset);
    }
}
