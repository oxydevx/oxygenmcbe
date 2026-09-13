#pragma once
#include "../level/Level.hpp"
#include "Timer.hpp"

class GameSession {
public:
    Level* getLevel() {
        Level** ptr = hat::member_at<Level**>(this, 0x40);
        return ptr ? *ptr : nullptr;
    }
};

class Minecraft {
public:
    GameSession* getGameSession() { return hat::member_at<GameSession*>(this, 0xB8); }
    Timer* getTimer() { return hat::member_at<Timer*>(this, 0xD0); }

    Level* getLevel() {
        GameSession* gs = getGameSession();
        if (!gs) return nullptr;
        return gs->getLevel();
    }
};
