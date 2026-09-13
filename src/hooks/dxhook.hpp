#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>

namespace Hooks::DX12 {
    bool Init();
    void Shutdown();

    extern bool g_imguiInit;
}
