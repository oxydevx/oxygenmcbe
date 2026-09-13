#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

#include "Category.hpp"

class Setting {
public:
    std::string label;
    std::function<bool()> visibility = []() { return true; };

    Setting(const std::string& lbl) : label(lbl) {}
    virtual ~Setting() = default;

    Setting* SetVisibility(std::function<bool()> vis) {
        visibility = vis;
        return this;
    }
};

class SliderSetting : public Setting {
public:
    float* value;
    float  minVal;
    float  maxVal;
    std::string format;

    SliderSetting(const std::string& lbl, float* val, float mn, float mx, const std::string& fmt = "%.2f")
        : Setting(lbl), value(val), minVal(mn), maxVal(mx), format(fmt) {}
};

class CheckboxSetting : public Setting {
public:
    bool* value;

    CheckboxSetting(const std::string& lbl, bool* val)
        : Setting(lbl), value(val) {}
};

class ComboSetting : public Setting {
public:
    int* value;
    std::vector<std::string> options;

    ComboSetting(const std::string& lbl, int* val, const std::vector<std::string>& opts)
        : Setting(lbl), value(val), options(opts) {}
};

class TextSetting : public Setting {
public:
    char* buf;     
    int   maxLen;  

    TextSetting(const std::string& lbl, char* b, int mx)
        : Setting(lbl), buf(b), maxLen(mx) {}
};

class ButtonSetting : public Setting {
public:
    std::function<void()> onActivate;

    ButtonSetting(const std::string& lbl, std::function<void()> fn)
        : Setting(lbl), onActivate(std::move(fn)) {}
};

class Feature {
public:
    std::string Name;
    std::string Description;
    Category    ModuleCategory;
    bool        Enabled = false;
    bool        Visibility = true;
    bool        IsBackground = false;
    bool        CallAllTime = false;
    bool        HideFromList = false;   
    float       ArrayPercentage = 0.0f;
    int         Keybind = 0;

    
    
    bool        IsHUD = false;
    float       HudX = 20.0f;
    float       HudY = 20.0f;
    float       HudW = 0.0f;   
    float       HudH = 0.0f;   

    
    
    mutable float m_eventMs   = 0.0f;
    mutable float m_renderMs  = 0.0f;
    mutable int   m_skipEvent   = 0;
    mutable int   m_skipRender  = 0;

    std::vector<std::shared_ptr<Setting>> Settings;

    Feature(const std::string& name, Category cat)
        : Name(name), ModuleCategory(cat) {
    }

    virtual ~Feature() = default;

    virtual void OnEnabled() {}
    virtual void OnDisabled() {}
    virtual void OnEvent() = 0;
    virtual void OnRender() {}
    virtual std::string getSettingDisplay() { return ""; }

    virtual void OnKey(int key) {
        if (key == Keybind && Keybind != 0) {
            Enabled = !Enabled;
        }
    }

    void SetMode(const std::string& m) {
        std::lock_guard<std::mutex> lock(m_modeMutex);
        Mode = m;
    }

    std::string GetMode() const {
        std::lock_guard<std::mutex> lock(m_modeMutex);
        return Mode;
    }

    void AddSlider(const std::string& lbl, float* val, float mn, float mx, const std::string& fmt = "%.2f") {
        Settings.push_back(std::make_shared<SliderSetting>(lbl, val, mn, mx, fmt));
    }

    void AddCheckbox(const std::string& lbl, bool* val) {
        Settings.push_back(std::make_shared<CheckboxSetting>(lbl, val));
    }

    void AddCombo(const std::string& lbl, int* val, const std::vector<std::string>& opts) {
        Settings.push_back(std::make_shared<ComboSetting>(lbl, val, opts));
    }

    void AddText(const std::string& lbl, char* buf, int maxLen) {
        Settings.push_back(std::make_shared<TextSetting>(lbl, buf, maxLen));
    }

    void AddButton(const std::string& lbl, std::function<void()> fn) {
        Settings.push_back(std::make_shared<ButtonSetting>(lbl, std::move(fn)));
    }

public:
    std::string Mode;

    mutable std::mutex m_modeMutex;
};
