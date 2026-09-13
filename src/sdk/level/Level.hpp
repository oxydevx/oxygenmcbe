#pragma once

class Level {
public:
    static void SetCurrent(Level* level) { s_current = level; }
    static Level* GetCurrent() { return s_current; }

private:
    static inline Level* s_current = nullptr;
};
