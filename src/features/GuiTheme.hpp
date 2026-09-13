#pragma once
#include <cstdint>




namespace GuiTheme {

    enum class Theme : int {
        Berry = 0,     
        Earth = 1,     
        Botanical = 2, 
        Aquatic = 3,   
        Neutral = 4,   
    };

    inline int   g_theme = (int)Theme::Berry; 
    inline float g_alpha = 1.0f;              

    
    
    
    struct AccentColors {
        unsigned SURFACE;             
        unsigned ON_SURFACE;          
        unsigned OUTLINE;             
        unsigned PRIMARY;             
        unsigned PRIMARY_CONTAINER;   
        unsigned ON_PRIMARY;          
        unsigned ON_PRIMARY_CONTAINER;
        unsigned KEYBIND_ACTIVE;
        unsigned MOD_FLASH_ON;
    };

    
    
    
    
    
    inline const AccentColors& GetAccent() {
        static const AccentColors accents[] = {
            
              { 0xF2261A1E, 0xFFF2E6EC, 0x293B2F35, 0xFFF7A7C5, 0xFF6E2F44, 0xFFFFFFFF, 0xFFFFDDE7, 0xFF6E2F44, 0xFFFFC4D9 },
              { 0xF2171B21, 0xFFD5E0EB, 0x29282F3A, 0xFF698DB0, 0xFF263040, 0xFFE9F3FF, 0xFFC7D9EA, 0xFF263040, 0xFFA8C9E8 },
              { 0xF220221C, 0xFFE2EAE2, 0x292C352A, 0xFFA3CC8C, 0xFF223B10, 0xFFFFFFFF, 0xFFCEE7B6, 0xFF223B10, 0xFFB6D69F },
              { 0xF229211D, 0xFFF4EDE6, 0x293C322A, 0xFFE6B16F, 0xFF5C3A14, 0xFFFFFFFF, 0xFFF7E6CD, 0xFF5C3A14, 0xFFF0D2A9 },
              { 0xF21A1A1C, 0xFFE5E2E2, 0x2946464B, 0xFFB2AAAA, 0xFF403A3A, 0xFFFFFFFF, 0xFFE3DEDE, 0xFF403A3A, 0xFFCEC8C8 },
        };
        int i = g_theme;
        if (i < 0 || i > 4) i = 0;
        return accents[i];
    }

    inline float Alpha() { return g_alpha; }

} 
