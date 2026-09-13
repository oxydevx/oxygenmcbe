#pragma once
#include <vector>
#include <memory>
#include <filesystem>
#include "Feature.hpp"

class ModuleManager {
public:

    static std::vector<std::shared_ptr<Feature>> FeatureList;
    static bool s_handledKeys;

    static void Initialize();

    
    
    
    static void Shutdown();

    
    
    static void StopWorkers();

    static void CallEvent();

    static void CallRenderEvent();

    static void CallBackgroundEvent();
};



namespace Config {
    
    std::filesystem::path Directory();

    
    
    bool Save(const std::string& name);

    
    bool Load(const std::string& name);

    
    bool Delete(const std::string& name);

    
    bool Exists(const std::string& name);

    
    std::vector<std::string> List();
}