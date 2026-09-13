#include "Loop.hpp"
#include <Windows.h>

namespace Utils {
    std::vector<std::function<void()>> Loop::callbacks;
    std::atomic<bool> Loop::running(false);
    HANDLE Loop::loopThread = nullptr;
    std::mutex Loop::mutex;

    void Loop::Register(std::function<void()> func) {
        std::lock_guard<std::mutex> lock(mutex);
        callbacks.push_back(func);
    }

    void Loop::Start() {
        if (running) return;
        running = true;
        loopThread = CreateThread(nullptr, 0, RunProc, nullptr, 0, nullptr);
    }

    void Loop::Stop() {
        running = false;
        if (loopThread) {
            HANDLE h = loopThread;
            loopThread = nullptr;
            
            
            
            WaitForSingleObject(h, 3000);
            CloseHandle(h);
        }
    }

    DWORD WINAPI Loop::RunProc(LPVOID) {
        Run();
        return 0;
    }

    void Loop::Run() {
        while (running) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                for (auto& callback : callbacks) {
                    callback();
                }
            }
            Sleep(1);
        }
    }
}