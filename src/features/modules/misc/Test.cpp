#include "Test.hpp"
#include "../../ModuleManager.hpp"
#include "../../../utils/logger.hpp"

Test::Test() : Feature("Test", Category::Misc) {
    this->Description = "Test module with GUI animation";
    this->Enabled = false;
    this->Keybind = 0;
    this->Visibility = false;
    this->IsBackground = false;

    AddCheckbox("Smooth Animation", &m_smoothAnimation);
    AddCheckbox("Motion Blur", &m_motionBlur);
}

void Test::OnEvent() {
    
    if (m_motionBlur) {
        
        
    }
}

void Test::OnEnabled() {
    Logger::InfoTag("Test", "Enabled smooth GUI animation");
    if (m_motionBlur) {
        Logger::InfoTag("Test", "Motion blur is also enabled");
    }
}

void Test::OnDisabled() {
    Logger::InfoTag("Test", "Disabled");
    if (m_motionBlur) {
        Logger::InfoTag("Test", "Motion blur disabled");
    }
}