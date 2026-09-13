#pragma once
#include "../../Feature.hpp"
#include <unordered_map>
#include <string>

struct ArrayListAnim {
    float alpha = 0.0f; 
    float y = 0.0f;     
};

class ArrayList : public Feature {
public:
    ArrayList();
    void OnEvent() override {}
    void OnRender() override;

private:
    std::unordered_map<std::string, ArrayListAnim> m_rowAnim;
};
