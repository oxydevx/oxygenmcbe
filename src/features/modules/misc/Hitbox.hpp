#pragma once
#include "../../Feature.hpp"

class Hitbox : public Feature {
public:
    float TargetValue = 0.6f;

    Hitbox();
    ~Hitbox();
    void OnEvent() override;
    void OnEnabled() override;
    void OnDisabled() override;

    
    
    
    
    static void Cleanup();
};
