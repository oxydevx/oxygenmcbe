#define _CRT_SECURE_NO_WARNINGS
#include "ModuleManager.hpp"
#include "../hooks/Keyboard.hpp"
#include "../utils/Loop.hpp"
#include "../utils/logger.hpp"
#include <windows.h>   
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include "modules/visual/ClickGui.hpp"
#include "modules/visual/NameProtect.hpp"
#include "modules/misc/Test.hpp"
#include "modules/misc/BrewTimer.hpp"
#include "modules/misc/Interface.hpp"
#include "modules/misc/Hitbox.hpp"
#include "modules/movement/Sprint.hpp"
#include "modules/misc/Radio.hpp"
#include "modules/visual/ArrayList.hpp"
#include "modules/visual/ESP.hpp"
#include "modules/visual/Notifications.hpp"
#include <new>

std::vector<std::shared_ptr<Feature>> ModuleManager::FeatureList;
static std::atomic<bool> bInitialized{ false };
static std::atomic<bool> bReady{ false };   
static std::vector<bool> g_lastEnabled;   




static constexpr float MODULE_BUDGET_MS = 4.0f;
static constexpr int    MODULE_SKIP_FRAMES = 1;

static long long NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}





    __declspec(noinline) static void SafeOnEventImpl(Feature* f) {
        void* faultAddr = nullptr;
        __try {
            f->OnEvent();
        } __except (faultAddr = GetExceptionInformation()->ExceptionRecord->ExceptionAddress,
                    EXCEPTION_EXECUTE_HANDLER) {
            Logger::ErrorTag("ModuleManager", "feature '%s' crashed in OnEvent (ClickGui sect '%s', code 0x%08lX, rip 0x%p)",
                f->Name.c_str(), ClickGui::DebugSection(), (unsigned long)GetExceptionCode(),
                faultAddr);
        }
    }

    static void SafeOnEvent(Feature* f) {
        
        if (f->Name != "ClickGui" && f->m_skipEvent > 0) {
            f->m_skipEvent--;
            return;
        }
        long long t0 = NowMs();
        try {
            SafeOnEventImpl(f);
        } catch (const std::exception& e) {
            Logger::ErrorTag("ModuleManager", "feature '%s' threw in OnEvent: %s",
                f->Name.c_str(), e.what());
        } catch (...) {
            Logger::ErrorTag("ModuleManager", "feature '%s' threw unknown in OnEvent", f->Name.c_str());
        }
        if (f->Name != "ClickGui") {
            float ms = (float)(NowMs() - t0);
            f->m_eventMs = f->m_eventMs * 0.8f + ms * 0.2f;
            if (f->m_eventMs > MODULE_BUDGET_MS)
                f->m_skipEvent = MODULE_SKIP_FRAMES;
        }
    }

    __declspec(noinline) static void SafeOnRenderImpl(Feature* f) {
        __try {
            f->OnRender();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::ErrorTag("ModuleManager", "feature '%s' crashed in OnRender (code 0x%08lX)",
                f->Name.c_str(), (unsigned long)GetExceptionCode());
        }
    }

    static void SafeOnRender(Feature* f) {
        if (f->Name != "ClickGui" && f->m_skipRender > 0) {
            f->m_skipRender--;
            return;
        }
        long long t0 = NowMs();
        try {
            SafeOnRenderImpl(f);
        } catch (const std::exception& e) {
            Logger::ErrorTag("ModuleManager", "feature '%s' threw in OnRender: %s",
                f->Name.c_str(), e.what());
        } catch (...) {
            Logger::ErrorTag("ModuleManager", "feature '%s' threw unknown in OnRender", f->Name.c_str());
        }
        if (f->Name != "ClickGui") {
            float ms = (float)(NowMs() - t0);
            f->m_renderMs = f->m_renderMs * 0.8f + ms * 0.2f;
            if (f->m_renderMs > MODULE_BUDGET_MS)
                f->m_skipRender = MODULE_SKIP_FRAMES;
        }
    }

    
    
    
    
    
    
    
    
    template <typename T>
    static Feature* ConstructFn() {
        return new (std::nothrow) T();
    }

    __declspec(noinline) static Feature* SafeConstruct(Feature* (*ctor)(), unsigned long* codeOut) {
        Feature* raw = nullptr;
        __try {
            raw = ctor();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            *codeOut = GetExceptionCode();
            raw = nullptr;
        }
        return raw;
    }

    template <typename T>
    static std::shared_ptr<Feature> SafeNew(const char* name) {
        unsigned long code = 0;
        Feature* raw = SafeConstruct(&ConstructFn<T>, &code);
        if (!raw) {
            Logger::ErrorTag("ModuleManager",
                "module '%s' faulted during construction (SEH 0x%08lX); skipping",
                name, code);
            return nullptr;
        }
        return std::shared_ptr<Feature>(raw);
    }

    void ModuleManager::Initialize() {
    
    
    
    if (bInitialized.exchange(true)) return;

    
    if (auto m = SafeNew<ClickGui>("ClickGui")) FeatureList.push_back(m);

    
    if (auto m = SafeNew<Test>("Test")) FeatureList.push_back(m);

    
    if (auto m = SafeNew<BrewTimer>("BrewTimer")) FeatureList.push_back(m);

    
    if (auto m = SafeNew<NameProtect>("NameProtect")) FeatureList.push_back(m);

    
    if (auto m = SafeNew<Interface>("Interface")) FeatureList.push_back(m);

    
    if (auto m = SafeNew<Hitbox>("Hitbox")) FeatureList.push_back(m);

    
    if (auto m = SafeNew<Sprint>("Sprint")) FeatureList.push_back(m);

    
    if (auto m = SafeNew<Radio>("Radio")) FeatureList.push_back(m);

    
    if (auto m = SafeNew<ArrayList>("ArrayList")) FeatureList.push_back(m);

    
    if (auto m = SafeNew<ESP>("ESP")) FeatureList.push_back(m);

    
    if (auto m = SafeNew<Notifications>("Notifications")) FeatureList.push_back(m);

    
    for (auto& sp : FeatureList) {
        sp->AddCheckbox("Hide", &sp->HideFromList);
    }

    Utils::Loop::Register([]() {
        ModuleManager::CallBackgroundEvent();
        });
    Utils::Loop::Start();

    
    bReady = true;
}

bool ModuleManager::s_handledKeys = false;

    
    
    void ModuleManager::StopWorkers() {
        Utils::Loop::Stop();
        ESP::StopBackgroundScan();
    }

    
    
    
    void ModuleManager::Shutdown() {
        bReady = false;   
        Utils::Loop::Stop();
        for (auto& sp : FeatureList) {
            Feature* f = sp.get();
            if (f->Enabled) {
                f->Enabled = false;
                f->OnDisabled();
            }
        }
        
        
        
        
        
        for (auto& sp : FeatureList) {
            if (auto* sprint = dynamic_cast<Sprint*>(sp.get())) {
                sprint->StopThread();
                break;
            }
        }
        
        
        
        
        Hitbox::Cleanup();
    }

    void ModuleManager::CallEvent() {
        if (!bReady) return;  
        
    if (!ModuleManager::s_handledKeys) {
        const bool guiOpen = ClickGui::IsGUIActive();
        if (!ClickGui::IsListening() && !ClickGui::IsTextEditing()) {
            for (int i = 0x07; i < 0xFE; i++) {
                if (Hooks::Keyboard::WasKeyPressed(i)) {
                    for (auto& f : FeatureList) {
                        
                        
                        
                        
                        
                        if (guiOpen && f->Name != "ClickGui") continue;
                        f->OnKey(i);
                    }
                }
            }
        }
        ModuleManager::s_handledKeys = true;
    }

    auto& lastEnabled = g_lastEnabled;
    if (lastEnabled.empty()) {
        for (const auto& f : FeatureList)
            lastEnabled.push_back(f->Enabled);
    }

    bool toggled = false;
    for (size_t i = 0; i < FeatureList.size(); ++i) {
        auto& f = FeatureList[i];
        if (lastEnabled[i] != f->Enabled) {
            if (f->Enabled) {
                f->OnEnabled();
                if (f->Name != "ClickGui")  
                    Notifications::Push(f->Name, "Module enabled", Notifications::Type::ModuleOn);
            }
            else {
                f->OnDisabled();
                if (f->Name != "ClickGui")
                    Notifications::Push(f->Name, "Module disabled", Notifications::Type::ModuleOff);
            }
            lastEnabled[i] = f->Enabled;
            toggled = true;
        }
    }
    
    
    if (toggled)
        Config::Save("default");

    for (auto& f : FeatureList) {
        if (f->ModuleCategory == Category::Visual) continue;
        if ((f->Enabled || f->CallAllTime) && !f->IsBackground) {
            SafeOnEvent(f.get());
        }
    }
}

    void ModuleManager::CallRenderEvent() {
        if (!bReady) return;  
        ModuleManager::s_handledKeys = false;
    for (auto& f : FeatureList) {
        if (f->ModuleCategory == Category::Visual) {
            if ((f->Enabled || f->CallAllTime) && !f->IsBackground) {
                SafeOnEvent(f.get());
            }
        }
        
        if (f->Enabled || f->CallAllTime) {
            SafeOnRender(f.get());
        }
    }

    
    
    for (auto& sp : FeatureList) {
        if (sp->Name == "ClickGui") {
            ClickGui* cg = static_cast<ClickGui*>(sp.get());
            __try {
                cg->DrawOverlayUI();
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                Logger::ErrorTag("ModuleManager::CallRenderEvent", "ClickGui::DrawOverlayUI crashed (code 0x%08lX)",
                    (unsigned long)GetExceptionCode());
            }
            break;
        }
    }
}

void ModuleManager::CallBackgroundEvent() {
    for (auto& f : FeatureList) {
        if (f->Enabled && f->IsBackground) {
            __try {
                f->OnEvent();
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Logger::ErrorTag("ModuleManager::CallBackgroundEvent",
                    "feature '%s' crashed (code 0x%08lX)", f->Name.c_str(), (unsigned long)GetExceptionCode());
            }
        }
    }
}




namespace Config {

std::filesystem::path Directory() {
    std::filesystem::path dir;
    const char* local = std::getenv("LOCALAPPDATA");
    if (local && *local)
        dir = std::filesystem::path(local) / "Oxygen";
    else
        dir = std::filesystem::path("Oxygen"); 
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}




static std::string NormalizeCfgName(const std::string& name) {
    std::string fname = name;
    if (fname.size() < 4 ||
        fname.compare(fname.size() - 4, 4, ".cfg") != 0)
        fname += ".cfg";
    return fname;
}

static std::filesystem::path ConfigFilePath(const std::string& name) {
    return Directory() / NormalizeCfgName(name);
}

bool Save(const std::string& name) {
    std::filesystem::path p = ConfigFilePath(name);
    std::ofstream out(p, std::ios::out | std::ios::trunc);
    if (!out) { Logger::ErrorTag("Config", "cannot write %s", p.string().c_str()); return false; }

    out << "# Oxygen config: " << name << "\n";
    for (auto& sp : ModuleManager::FeatureList) {
        Feature* f = sp.get();
        if (f->Name == "ClickGui") continue; 
        out << f->Name << ".Enabled=" << (f->Enabled ? 1 : 0) << "\n";
        out << f->Name << ".Keybind=" << f->Keybind << "\n";
        out << f->Name << ".HudX=" << f->HudX << "\n";
        out << f->Name << ".HudY=" << f->HudY << "\n";
        for (auto& s : f->Settings) {
            if (auto* cb = dynamic_cast<CheckboxSetting*>(s.get()))
                out << f->Name << ".Setting." << s->label << "=" << (*cb->value ? 1 : 0) << "\n";
            else if (auto* sl = dynamic_cast<SliderSetting*>(s.get()))
                out << f->Name << ".Setting." << s->label << "=" << *sl->value << "\n";
            else if (auto* cm = dynamic_cast<ComboSetting*>(s.get()))
                out << f->Name << ".Setting." << s->label << "=" << *cm->value << "\n";
            else if (auto* ts = dynamic_cast<TextSetting*>(s.get()))
                out << f->Name << ".Setting." << s->label << "=" << ts->buf << "\n";
        }
    }
    out.close();
    return true;
}

bool Load(const std::string& name) {
    std::filesystem::path p = ConfigFilePath(name);
    std::ifstream in(p);
    if (!in) { Logger::ErrorTag("Config", "cannot open %s", p.string().c_str()); return false; }

    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(line.substr(0, eq));
        std::string val = Trim(line.substr(eq + 1));
        size_t dot = key.find('.');
        if (dot == std::string::npos) continue;
        std::string mod = key.substr(0, dot);
        std::string field = key.substr(dot + 1);

        Feature* f = nullptr;
        for (auto& sp : ModuleManager::FeatureList) {
            if (sp->Name == mod) { f = sp.get(); break; }
        }
        if (!f || f->Name == "ClickGui") continue;

        if (field == "Enabled") {
            bool v = (val == "1" || val == "true" || val == "1.0");
            bool was = f->Enabled;
            f->Enabled = v;
            if (v && !was) f->OnEnabled();
            else if (!v && was) f->OnDisabled();
        }
        else if (field == "Keybind") {
            f->Keybind = std::atoi(val.c_str());
        }
        else if (field == "HudX") {
            f->HudX = (float)std::atof(val.c_str());
        }
        else if (field == "HudY") {
            f->HudY = (float)std::atof(val.c_str());
        }
        else if (field.rfind("Setting.", 0) == 0) {
            std::string label = field.substr(8);
            for (auto& s : f->Settings) {
                if (s->label != label) continue;
                if (auto* cb = dynamic_cast<CheckboxSetting*>(s.get()))
                    *cb->value = (val == "1" || val == "true" || val == "1.0");
                else if (auto* sl = dynamic_cast<SliderSetting*>(s.get()))
                    *sl->value = (float)std::atof(val.c_str());
                else if (auto* cm = dynamic_cast<ComboSetting*>(s.get()))
                    *cm->value = std::atoi(val.c_str());
                else if (auto* ts = dynamic_cast<TextSetting*>(s.get())) {
                    std::strncpy(ts->buf, val.c_str(), (size_t)ts->maxLen);
                    ts->buf[ts->maxLen] = '\0';
                }
                break;
            }
        }
    }
    in.close();
    Logger::InfoTag("Config", "loaded %s", p.string().c_str());
    return true;
}

bool Delete(const std::string& name) {
    std::filesystem::path p = ConfigFilePath(name);
    std::error_code ec;
    return std::filesystem::remove(p, ec);
}

bool Exists(const std::string& name) {
    std::error_code ec;
    return std::filesystem::exists(ConfigFilePath(name), ec);
}

std::vector<std::string> List() {
    std::vector<std::string> out;
    std::error_code ec;
    auto dir = Directory();
    if (!std::filesystem::exists(dir, ec)) return out;
    for (auto& e : std::filesystem::directory_iterator(dir, ec)) {
        if (e.is_regular_file(ec))
            out.push_back(e.path().filename().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

} 