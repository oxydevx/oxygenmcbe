#pragma once
#include "../../Feature.hpp"
#include <windows.h>
#include <string>
#include <thread>
#include <atomic>

class Sprint : public Feature {
public:
    Sprint();
    ~Sprint();
    void OnEvent() override;
    
    
    
    
    
    void StopThread();

private:
    int sprintKeyVirtualCode = 0;
    std::atomic<bool> isRunning{ false };
    std::thread workerThread;

    void LoadSprintKeyFromOptions();
    void KeySpammerLoop();
    int MapMinecraftKeyToVK(int mcKeyCode);
};
