#pragma once
#include "../../Feature.hpp"
#include <deque>
#include <mutex>
#include <string>





class Notifications : public Feature {
public:
    enum class Type : int {
        Info = 0,     
        ModuleOn = 1, 
        ModuleOff = 2,
        Warn = 3,     
    };

    Notifications();
    void OnEvent() override {}
    void OnRender() override;

    
    static void Push(const std::string& title, const std::string& message,
        Type type = Type::Info);

private:
    struct Item {
        std::string title;
        std::string message;
        Type        type    = Type::Info;
        float       life    = 0.0f; 
        float       maxLife = 0.0f; 
        float       age     = 0.0f; 
        float       alpha   = 0.0f; 
        float       y       = 0.0f; 
        float       x       = 0.0f; 
        bool        snapY   = true; 
    };

    static constexpr size_t MAX_ITEMS = 5;

    float m_dur = 3.0f; 

    std::deque<Item> m_items;
    std::mutex       m_mutex;
};