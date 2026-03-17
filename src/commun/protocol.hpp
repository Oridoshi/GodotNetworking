//
// Created by tom18 on 19/02/2026.
//

#ifndef GODOTNETWORKING_PROTOCOL_H
#define GODOTNETWORKING_PROTOCOL_H

#include <cstdint>
#include <vector>

struct Packet {
    sockaddr_in sender;
    std::vector<char> data;
};

struct InputPacket {
    uint32_t sequence_id; // Identifiant de séquence pour le suivi des paquets
    uint8_t  keys;
    float    aim_x; // Position de visée X (si nécessaire)
    float    aim_y; // Position de visée Y (si nécessaire)
};

struct LocationPacket
{
    int x;
    int y;
};

enum class PacketType : uint8_t {
    LOGIN      = 0,
    INPUT      = 1,
    NEW_PLAYER = 2,
    LOGOUT     = 3
};

#endif //GODOTNETWORKING_PROTOCOL_H
