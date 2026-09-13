#pragma once
#include <dxgi1_4.h>
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace Hooks::D2D {
    void DrawMotionBlur(float intensity);
}