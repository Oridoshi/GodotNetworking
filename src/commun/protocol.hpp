//
// Created by tom18 on 19/02/2026.
//

#ifndef GODOTNETWORKING_PROTOCOL_H
#define GODOTNETWORKING_PROTOCOL_H

#include <cstdint>
#include <vector>

#pragma pack(push, 1)

// Type de Packet
struct Packet
{
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

struct PingRequestPacket
{
    uint32_t id;
    uint64_t timestamp0;
};

struct PingResponsePacket
{
    uint32_t id;
    uint64_t timestamp0;
    uint64_t timestamp1;
};

#pragma pack(pop)

enum class PacketType : uint8_t
{
    LOGIN      = 0,
    INPUT      = 1,
    LOCATION   = 2,
    PING       = 3,
    LOGOUT     = 4
};

#endif //GODOTNETWORKING_PROTOCOL_H
