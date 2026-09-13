#pragma once
#include <Windows.h>
#include <mutex>
#include <string>
#include <cstdio>
#include <fstream>

namespace Logger {

    inline std::mutex& GetMutex() {
        static std::mutex mtx;
        return mtx;
    }

    inline bool& IsInitialized() {
        static bool init = false;
        return init;
    }

    inline HANDLE& GetConsoleHandle() {
        static HANDLE h = nullptr;
        return h;
    }

    inline std::ofstream& GetLogFile() {
        static std::ofstream file;
        return file;
    }

    inline std::string& GetLogPath() {
        static std::string path;
        return path;
    }

    inline void SetConsoleColor(WORD attr) {
        HANDLE h = GetConsoleHandle();
        if (h) SetConsoleTextAttribute(h, attr);
    }

    inline void Init() {
        std::lock_guard<std::mutex> lock(GetMutex());
        if (IsInitialized()) return;
        GetConsoleHandle() = GetStdHandle(STD_OUTPUT_HANDLE);

        
        
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

        char localAppData[MAX_PATH] = { 0 };
        if (!GetEnvironmentVariableA("LOCALAPPDATA", localAppData, sizeof(localAppData)))
            strcpy_s(localAppData, sizeof(localAppData), ".");

        const std::string oxygenDir = std::string(localAppData) + "\\Oxygen";
        const std::string logsDir = oxygenDir + "\\logs";
        CreateDirectoryA(localAppData, nullptr);
        CreateDirectoryA(oxygenDir.c_str(), nullptr);
        CreateDirectoryA(logsDir.c_str(), nullptr);

        SYSTEMTIME st;
        GetLocalTime(&st);
        char fname[64];
        snprintf(fname, sizeof(fname), "Oxygen_%04d-%02d-%02d_%02d-%02d-%02d.log",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

        const std::string fullPath = logsDir + "\\" + fname;
        GetLogPath() = fullPath;
        GetLogFile().open(fullPath, std::ios::out | std::ios::trunc);
        IsInitialized() = true;
    }

    inline std::string GetTimestamp() {
        char buf[32];
        SYSTEMTIME st;
        GetLocalTime(&st);
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        return buf;
    }

    enum class Level { Info, Warn, Error };

    template <typename... Args>
    inline void Log(Level level, const char* fmt, Args&&... args) {
        if (!fmt) return;

        std::lock_guard<std::mutex> lock(GetMutex());

        const char* levelStr = "";
        switch (level) {
        case Level::Info:  levelStr = "info";  break;
        case Level::Warn:  levelStr = "warn";  break;
        case Level::Error: levelStr = "error"; break;
        }

        std::string ts = GetTimestamp();
        char buf[2048];
        char msg[2048];
        if constexpr (sizeof...(args) == 0) {
            snprintf(msg, sizeof(msg), "%s", fmt);
        }
        else {
            snprintf(msg, sizeof(msg), fmt, std::forward<Args>(args)...);
        }
        snprintf(buf, sizeof(buf), "[%s] [%s] %s\n", ts.c_str(), levelStr, msg);

        
        HANDLE h = GetConsoleHandle();
        if (h) {
            WORD color = 7;
            switch (level) {
            case Level::Info:  color = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
            case Level::Warn:  color = FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY; break;
            case Level::Error: color = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
            }
            WriteConsoleA(h, "[", 1, nullptr, nullptr);
            WriteConsoleA(h, ts.c_str(), (DWORD)ts.size(), nullptr, nullptr);
            WriteConsoleA(h, "] [", 3, nullptr, nullptr);
            SetConsoleColor(color);
            WriteConsoleA(h, levelStr, (DWORD)strlen(levelStr), nullptr, nullptr);
            SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            WriteConsoleA(h, "] ", 2, nullptr, nullptr);
            WriteConsoleA(h, msg, (DWORD)strlen(msg), nullptr, nullptr);
            WriteConsoleA(h, "\n", 1, nullptr, nullptr);
        }

        
        std::ofstream& logFile = GetLogFile();
        if (logFile.is_open()) {
            logFile.write(buf, strlen(buf));
            logFile.flush();
        }
    }

    inline void Info(const char* fmt, auto... args) {
        Log(Level::Info, fmt, args...);
    }

    inline void Warn(const char* fmt, auto... args) {
        Log(Level::Warn, fmt, args...);
    }

    inline void Error(const char* fmt, auto... args) {
        Log(Level::Error, fmt, args...);
    }

    template <typename... Args>
    inline void Tag(Level level, const char* tag, const char* fmt, Args&&... args) {
        if (!fmt || !tag) return;

        std::lock_guard<std::mutex> lock(GetMutex());

        const char* levelStr = "";
        switch (level) {
        case Level::Info:  levelStr = "info";  break;
        case Level::Warn:  levelStr = "warn";  break;
        case Level::Error: levelStr = "error"; break;
        }

        std::string ts = GetTimestamp();
        char buf[2048];
        char msg[2048];
        if constexpr (sizeof...(args) == 0) {
            snprintf(msg, sizeof(msg), "%s", fmt);
        }
        else {
            snprintf(msg, sizeof(msg), fmt, std::forward<Args>(args)...);
        }
        snprintf(buf, sizeof(buf), "[%s] [%s] [%s] %s\n", ts.c_str(), levelStr, tag, msg);

        
        HANDLE h = GetConsoleHandle();
        if (h) {
            WORD levelColor = 7;
            switch (level) {
            case Level::Info:  levelColor = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
            case Level::Warn:  levelColor = FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY; break;
            case Level::Error: levelColor = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
            }
            const WORD TAG_COLOR = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            WriteConsoleA(h, "[", 1, nullptr, nullptr);
            WriteConsoleA(h, ts.c_str(), (DWORD)ts.size(), nullptr, nullptr);
            WriteConsoleA(h, "] [", 3, nullptr, nullptr);
            SetConsoleColor(levelColor);
            WriteConsoleA(h, levelStr, (DWORD)strlen(levelStr), nullptr, nullptr);
            SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            WriteConsoleA(h, "] [", 3, nullptr, nullptr);
            SetConsoleColor(TAG_COLOR);
            WriteConsoleA(h, tag, (DWORD)strlen(tag), nullptr, nullptr);
            SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            WriteConsoleA(h, "] ", 2, nullptr, nullptr);
            WriteConsoleA(h, msg, (DWORD)strlen(msg), nullptr, nullptr);
            WriteConsoleA(h, "\n", 1, nullptr, nullptr);
        }

        
        std::ofstream& logFile = GetLogFile();
        if (logFile.is_open()) {
            logFile.write(buf, strlen(buf));
            logFile.flush();
        }
    }

    inline void InfoTag(const char* tag, const char* fmt, auto... args) {
        Tag(Level::Info, tag, fmt, args...);
    }

    inline void WarnTag(const char* tag, const char* fmt, auto... args) {
        Tag(Level::Warn, tag, fmt, args...);
    }

    inline void ErrorTag(const char* tag, const char* fmt, auto... args) {
        Tag(Level::Error, tag, fmt, args...);
    }

}
