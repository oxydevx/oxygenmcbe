#pragma once

#include "../../Feature.hpp"




class Interface : public Feature {
public:
    Interface();
    void OnEvent()    override;
    void OnEnabled()  override;
    void OnDisabled() override;

private:
    int   m_theme  = 0;   
    float m_alpha  = 1.0f; 
};
