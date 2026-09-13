#pragma once
#include <memory>
#include <type_traits>
#include "Packet.hpp"

class MinecraftPackets {
public:
    template<typename T, std::enable_if_t<std::is_base_of_v<Packet, T>, int> = 0>
    static std::shared_ptr<T> createPacket() {
        auto pkt = std::make_shared<T>();
        return pkt;
    }
};
