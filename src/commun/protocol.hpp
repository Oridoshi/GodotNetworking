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
    uint32_t player_net_id;
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

struct LoginPacket
{
    uint32_t frame_id;
};

struct WorldStatePacket
{
    uint32_t frame_id; // = "id"
    std::vector<char> data;
};

#pragma pack(pop)

//---- COULEUR LOG ----//

const std::string RESET   = "\033[0m";
const std::string RED     = "\033[31m";
const std::string GREEN   = "\033[32m";
const std::string YELLOW  = "\033[33m";
const std::string BLUE    = "\033[34m";
const std::string CYAN    = "\033[36m";

//---------------------//

enum class PacketType : uint8_t
{
    LOGIN      = 0,
    WORLDSTATE = 1,
    INPUT      = 2,
    LOCATION   = 3,
    PING       = 4,
    LOGOUT     = 5
};

#endif //GODOTNETWORKING_PROTOCOL_H

