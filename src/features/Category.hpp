#pragma once

enum class Category {
    Combat,
    Visual,
    Movement,
    Player,
    Misc
};

inline const char* GetCategoryName(Category cat) {
    switch (cat) {
    case Category::Combat:   return "Combat";
    case Category::Visual:   return "Visual";
    case Category::Movement: return "Movement";
    case Category::Player:   return "Player";
    case Category::Misc:     return "Misc";
    default:                 return "Unknown";
    }
}