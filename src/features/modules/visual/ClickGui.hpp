#pragma once

#include "../../Feature.hpp"
#include "../../GuiTheme.hpp"
#include "../../../utils/RenderUtil.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>


class ClickGui : public Feature {
public:
    ClickGui();
    void OnEvent()    override;
    void OnEnabled()  override;
    void OnDisabled() override;

private:

    static constexpr float COL_W = 200.0f;
    static constexpr float HEADER_H = 38.0f;
    static constexpr float MODULE_H = 35.0f;
    static constexpr float SETTING_H = 38.0f;
    static constexpr float KEYBIND_H = 30.0f;

    static constexpr float SETTING_PAD = 4.0f;
    static constexpr float SLIDER_PAD = 8.0f;

    static constexpr float ANIM_SPEED = 10.0f;
    static constexpr float OPEN_DURATION = 0.45f; 
    static constexpr float EXPAND_DURATION = 0.22f; 
    static constexpr float TOOLTIP_HOLD    = 0.5f;   
    static constexpr float FLASH_DURATION = 0.18f;
    static constexpr float ROUNDING = 20.0f;       
    static constexpr float ROW_RADIUS = 12.0f;      
    static constexpr float SETTINGS_RADIUS = 12.0f;  
    static constexpr float PILL_RADIUS = 999.0f;    
    static constexpr float MODULE_PAD = 10.0f;      
    static constexpr float PANEL_PAD_TOP = 10.0f;   
    static constexpr float PANEL_PAD_BOTTOM = 10.0f;
    static constexpr float SETTINGS_PAD = 14.0f;    
    static constexpr float KEYBIND_GAP = 8.0f;      

    
    
    
    unsigned C_SURFACE            = 0xFF26222B; 
    unsigned C_SURFACE_2          = 0xFF302B33; 
    unsigned C_SURFACE_3          = 0xFF3A353F; 
    unsigned C_ON_SURFACE         = 0xFFE7E0EC; 
    unsigned C_ON_SURFACE_DIM     = 0xFFBDB7C2; 
    unsigned C_OUTLINE            = 0x294F4549; 
    unsigned C_TRACK              = 0x3A49454F; 
    unsigned C_SETTINGS           = 0xFF1F1B22; 
    unsigned C_KEYBIND            = 0x14FFFFFF; 
    unsigned C_SHADOW             = 0x00000000; 

    static constexpr unsigned C_MOD_FLASH_OFF = 0xFF2B2930;

    
    unsigned C_PRIMARY            = 0xFFC5A7F7;
    unsigned C_PRIMARY_CONTAINER  = 0xFF442F6E;
    unsigned C_ON_PRIMARY         = 0xFFFFFFFF;
    unsigned C_ON_PRIMARY_CONTAINER = 0xFFE7DDFF;
    unsigned C_KEYBIND_ACTIVE     = 0xFF442F6E;
    unsigned C_MOD_FLASH_ON       = 0xFFD9C4FF;

    struct Panel {
        Category    cat;
        std::string name;
        float       x, y;
        bool        collapsed = false;
        float       collapseAnim = 1.0f;
        bool        drag = false;
        float       dragOX = 0.0f;
        float       dragOY = 0.0f;
    };

    std::vector<Panel> m_panels;

    std::unordered_map<std::string, bool>  m_expanded;
    std::unordered_map<std::string, float> m_expandAnim;
    std::unordered_map<Category, std::unique_ptr<ImTextureData>> m_categoryIcons;
    std::unique_ptr<ImTextureData> m_arrowTex;   

    struct FlashState {
        float  timer = 0.0f;
        bool   enabling = true;
    };
    std::unordered_map<std::string, FlashState> m_flash;

    struct SliderDrag {
        Feature* feature = nullptr;
        std::string label;
        bool        active = false;
    } m_sliderDrag;

    struct KeybindListen {
        Feature* feature = nullptr;
        bool     active = false;
    };
    KeybindListen m_keybindListen;

    struct TextEdit {
        Feature*   feature = nullptr;
        std::string label;
        bool       active = false;
    };
    TextEdit m_textEdit;

    static bool s_isListening;
    static bool s_textEditing;
    static bool s_guiOpen;     

    
    static float s_animSpeed;     

    
    
    static const char* s_dbgSection;
    static char        s_dbgBuf[160];
    float m_guiAlpha = 0.0f;
    float m_guiScale = 0.8f;
    float m_animT = 0.0f;   
    float m_framesSinceIconLoad = 10.0f;  
    bool  m_iconsLoaded = false;           
    std::unordered_map<std::string, float> m_toggleAnim; 
    std::unordered_map<std::string, float> m_checkAnim;   
    std::unordered_map<std::string, float> m_hoverAnim;  
    mutable float m_contentFade = 1.0f;   

    Feature* m_tooltipFeature = nullptr;  
    float    m_tooltipAnim    = 0.0f;     
    float    m_tooltipHover   = 0.0f;     

    float m_animSpeed    = 1.0f; 

    
    
    
    bool  m_blurEnabled  = true;  
    float m_blurStrength  = 1.20f; 

    
    bool  m_showHudEditor = false;
    bool  m_showConfigs   = false;
    int   m_dragHud       = -1;     
    float m_dragOffX = 0.0f, m_dragOffY = 0.0f;
    char  m_cfgName[64] = {};       
    std::vector<std::string> m_cfgList;
    bool  m_cfgInputFocused = false; 
    float m_cfgScroll       = 0.0f;  
    bool  m_cfgWasOpen = false;

    
    float m_hudPosX = -1.0f, m_hudPosY = -1.0f;
    float m_cfgPosX = -1.0f, m_cfgPosY = -1.0f;
    int   m_dragWin   = -1;          
    float m_dragWinOX = 0.0f, m_dragWinOY = 0.0f;   

    static std::string CategoryAssetName(Category cat);
    void LoadCategoryIcons();
    void SyncTheme();   
    void DrawShadow(ImDrawList* dl, ImVec2 p, float w, float h, float rad) const;

    
    static ImU32 ApplyAlpha(ImU32 color, float alpha) {
        unsigned char a = (color >> 24) & 0xFF;
        a = (unsigned char)(a * alpha);
        return (color & 0x00FFFFFF) | (a << 24);
    }

    
    ImU32 Col(ImU32 color, float a = 1.0f) const {
        return ApplyAlpha(color, m_guiAlpha * m_contentFade * GuiTheme::Alpha() * a);
    }

public:
    static bool IsListening() { return s_isListening; }
    static bool IsTextEditing() { return s_textEditing; }
    static bool IsGUIActive() { return s_guiOpen; }
    static const char* DebugSection() { return s_dbgSection; }
    static float GetAnimSpeed() { return s_animSpeed; }

private:
    std::vector<Feature*> GetFeatures(Category cat) const;

    float PanelContentHeight(const Panel& p) const;
    float PanelHeight(const Panel& p) const;

    float ExpandedHeight(const Feature* f, float anim) const;

    int   SettingRows(const Feature* f) const;
    void  DrawSettings(ImDrawList* dl, Feature* f,
        float x, float baseY,
        bool  isMouseDown, bool mouseClicked,
        ImVec2 mouse, bool handleInput = true, float contentFade = 1.0f);

    void  UpdateAnimations(float dt);

public:
    
    
    void  DrawOverlayUI();

    static std::string KeyName(int vk);
    static void        PollKeybindListen(KeybindListen& listen);

    
    bool  IsBlurEnabled() const { return m_blurEnabled; }
    float GetBlurStrength() const { return m_blurStrength; }
    
    
    float GetMenuFade() const { return m_guiAlpha; }
};