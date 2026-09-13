#pragma once
#include "../../Feature.hpp"
#include "../../../sdk/math/MathTypes.hpp"
#include <imgui.h>
#include <windows.h>


class ESP : public Feature {
public:
    ESP();

    void OnEvent() override;

    void OnEnabled() override;
    void OnDisabled() override;

    
    static void StopBackgroundScan();

    float Range = 40.0f;
};