#include "Interface.hpp"
#include "../../GuiTheme.hpp"
#include "../../ModuleManager.hpp"
#include "../../../utils/logger.hpp"

Interface::Interface() : Feature("Interface", Category::Visual) {
    this->Description = "Interface theme & transparency";
    this->Enabled     = true;    
    this->CallAllTime = true;    
    this->Keybind     = 0;
    this->Visibility  = true;
    this->IsBackground = false;

    m_theme = (int)GuiTheme::Theme::Berry;
    m_alpha = 1.0f;

    AddCombo("Theme", &m_theme, { "Berry", "Earth", "Botanical", "Aquatic" });
    AddSlider("Transparency", &m_alpha, 0.2f, 1.0f, "%.2f");
}

void Interface::OnEvent() {
    if (!Enabled) {
        
        m_theme = (int)GuiTheme::Theme::Berry;
        m_alpha = 1.0f;
        GuiTheme::g_theme = (int)GuiTheme::Theme::Neutral;
        GuiTheme::g_alpha = 1.0f;
        return;
    }
    
    GuiTheme::g_theme = m_theme;
    GuiTheme::g_alpha = m_alpha;
}

void Interface::OnEnabled() {
    Logger::InfoTag("Interface", "theme=%d alpha=%.2f", m_theme, m_alpha);
}

void Interface::OnDisabled() {
    
    
}
