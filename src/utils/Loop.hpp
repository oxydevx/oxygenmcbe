#pragma once
#include <functional>
#include <vector>
#include <atomic>
#include <mutex>
#include <Windows.h>

namespace Utils {
    class Loop {
    public:
        static void Register(std::function<void()> func);

        static void Start();

        
        
        
        static void Stop();

    private:
        static DWORD WINAPI RunProc(LPVOID lpParam);
        static void Run();
        static std::vector<std::function<void()>> callbacks;
        static std::atomic<bool> running;
        static HANDLE loopThread;
        static std::mutex mutex;
    };
}