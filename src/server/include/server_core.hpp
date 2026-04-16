//
// Created by tom18 on 15/02/2026.
//
#pragma once

#include <iostream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <BaseTsd.h>
    typedef SOCKET socket_t;
    typedef int socklen_t;
    typedef SSIZE_T ssize_t;
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    typedef int socket_t;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

#include "../../commun/protocol.hpp"
#include <entt/entt.hpp>
#include <chrono>
#include <thread>
#include <string>
#include "mutex"
#include "queue"

struct NetworkContext {
    int const port;
    bool* running;

    // La file et son verrou
    std::mutex queue_mutex;
    std::queue<Packet> incoming_packets;
};

//*************** STRUC POUR ENTT *****************//

/**
 * @brief Représente un joueur connecté au serveur, avec sa position et un flag indiquant s'il faut diffuser sa position aux autres clients.
 */
struct PlayerConnectionInfo {
    std::string ip;
    int port;
};

/**
 * @brief Représente la position d'un joueur dans le monde du jeu, avec un flag indiquant s'il faut diffuser cette position aux autres clients.
 */
struct Location {
    int x{}, y{};
};

/**
 * @brief Représente les données d'input d'un joueur, avec un buffer pour stocker les paquets d'input reçus et des identifiants de séquence pour le suivi des paquets.
 */
struct Input {
    // Ajoutez ici les données spécifiques au joueur (ex: santé, score, etc.)
    int next_sequence_id = 0;
    int sequence_id_treat = 0; // Le dernier sequence_id traité pour éviter de traiter plusieurs fois le même paquet
    const int PACKET_SIZE = 13;

    std::vector<InputPacket> input_buffer;

    Input(std::vector<InputPacket> buffer)
        : input_buffer(std::move(buffer)) {}
};

//********************************************//

//---- MOVE VALUE ---- //

int const SPEED = 300; // Vitesse de déplacement du joueur (en unités par seconde)
int const TICK_RATE = 60; // Nombre de ticks par seconde pour la logique du serveur

//---------------------//


//---- WORLD STATE ----//

inline uint32_t frame_id = 0;

//---------------------//

inline socket_t udp_socket = INVALID_SOCKET;
inline NetworkContext* nctx;
inline entt::registry* registry;

// bibliothèques de logging
void OK(std::string msg);
void ERR(std::string msg);
void WARN(std::string msg);
void INFO(std::string msg);
void INFO_FROM_CLIENT(std::string msg, std::string sender_ip, int sender_port, PacketType type);

// Dictionaire pour stocker les clients connectés, id de l'entité et comme valeur l'entité correspondante dans le registre
inline std::unordered_map<uint32_t, entt::entity> clients;

inline int net_id = 100; // ID statique pour les joueurs, à incrémenter à chaque nouveau joueur