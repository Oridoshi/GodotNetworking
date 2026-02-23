//
// Created by tom18 on 19/02/2026.
//

#ifndef GODOTNETWORKING_PROTOCOL_H
#define GODOTNETWORKING_PROTOCOL_H

#include <cstdint>

enum class PacketType : uint8_t {
    LOGIN   = 0,
    VECTOR  = 1,
    ROTATOR = 2,
    INT     = 3,
    STRING  = 4,
    LOGOUT  = 5

};

#endif //GODOTNETWORKING_PROTOCOL_H
