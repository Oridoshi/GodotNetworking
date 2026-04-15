#include "gn_network_manager.h"

#include <chrono>
#include <string>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include "../commun/protocol.hpp"

using namespace godot;

/**
 * WSAS Struct for Windows Sockets API
 */
struct WSASocketInitializer
{
    WSADATA wsaData;

    WSASocketInitializer()
    {
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            UtilityFunctions::printerr("WSAStartup Failed");
        }
    }

    ~WSASocketInitializer()
    {
        WSACleanup();
    }
};

// Instance statique : elle s'initialisera une seule fois au lancement du jeu
#ifdef _WIN32
static WSASocketInitializer g_wsa_init;
#endif

void INFO(String msg)
{
    UtilityFunctions::print("[INFO] ", msg);
}

void INFO_SERVER(String msg, String sender_ip, int sender_port, PacketType type)
{
    String type_str;
    switch (type) {
        case PacketType::LOGIN:    type_str = "LOGIN"; break;
        case PacketType::LOCATION: type_str = "LOCATION"; break;
        case PacketType::INPUT:    type_str = "INPUT"; break;
        case PacketType::PING:     type_str = "PING"; break;
        case PacketType::LOGOUT:   type_str = "LOGOUT"; break;
        default:                   type_str = "UNKNOWN"; break;
    }

    UtilityFunctions::print("[INFO SERVER] ", msg, " | From: ", sender_ip, ":", sender_port, " | Type: ", type_str);
}

bool GDNetworkManager::bind_port()
{
    _close_socket(); // Close any existing socket before creating a new one to ensure we don't have multiple sockets open at the same time.

    // Creation of the UDP socket
    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket == INVALID_SOCKET)
    {
        UtilityFunctions::printerr("Failed to create the UDP socket");
        return false;
    }

    _set_non_blocking(udp_socket); // Set the socket to non-blocking mode to prevent blocking the main thread during network operations.
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all available interfaces
    server_addr.sin_port = htons(0); // Convert port to network byte order

    // Bind the socket to the specified port
    if (bind(udp_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        UtilityFunctions::printerr("Failed to bind the socket to port : ", 0);
        _close_socket();
        return false;
    }

    INFO("Socket successfully bound to port : " + String::num_int64(ntohs(server_addr.sin_port)));
    return true;
}

void GDNetworkManager::send_packet(int type, PackedByteArray data) {
    if (udp_socket == INVALID_SOCKET) {
        UtilityFunctions::printerr("Socket is not initialized. Please bind to a port first.");
        return;
    }

    if (data.is_empty() && type != static_cast<int>(PacketType::LOGOUT)) {
        INFO("Skipping empty packet.");
        return;
    }

    sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr)); // Initialisation propre
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(static_cast<uint16_t>(server_port));

    // Conversion de l'IP avec vérification
    CharString ip_utf8 = server_ip.utf8();
    int pton_res = inet_pton(AF_INET, ip_utf8.get_data(), &dest_addr.sin_addr);

    if (pton_res <= 0) {
        UtilityFunctions::printerr("Invalid IP address: ", server_ip);
        return;
    }

    // Utilisation de .read() pour accéder aux données sans copier
    const uint8_t* raw_data = data.ptr();
    size_t data_len = data.size();

    //ajout de l'id en premier
    uint32_t id = idForServer;
    std::vector<char> packet_data;
    packet_data.insert(packet_data.end(), reinterpret_cast<char*>(&id), reinterpret_cast<char*>(&id) + sizeof(uint32_t));

    //ajout du type du packet
    packet_data.insert(packet_data.end(), static_cast<char>(type));

    //ajout des données du packet
    packet_data.insert(packet_data.end(), raw_data, raw_data + data_len);

    data_len = packet_data.size();

    ssize_t sent_len = sendto(udp_socket, packet_data.data(),
                              static_cast<int>(data_len), 0,
                              (sockaddr*)&dest_addr, sizeof(dest_addr));

    if (sent_len == SOCKET_ERROR)
    {
        #ifdef _WIN32
                int err = WSAGetLastError();
        #else
                int err = errno;
        #endif
        UtilityFunctions::printerr("Failed to send packet to ", server_ip, ":", server_port, " | Error code: ", err);
    }
    else
    {
        UtilityFunctions::print("Packet sent (", sent_len, " bytes) to ", ip_utf8, ":", server_port);
    }
}

void GDNetworkManager::send_input(bool up, bool down, bool left, bool right, float aim_x, float aim_y)
{
    InputPacket input_pkt{};
    input_pkt.sequence_id = next_sequence_id++;
    input_pkt.keys = (up ? 1 : 0) | (down ? 2 : 0) | (left ? 4 : 0) | (right ? 8 : 0);
    input_pkt.aim_x = aim_x;
    input_pkt.aim_y = aim_y;

    // on met le packet dans le buffer d'input
    input_buffer.insert(input_buffer.begin(), input_pkt);

    //on retire les anciens input du buffer (on garde que les 20 derniers)
    if (input_buffer.size() > 20)
    {
        input_buffer.erase(input_buffer.begin() + 20, input_buffer.end());
    }

    PackedByteArray data;
    data.resize(input_buffer.size() * PACKET_SIZE); // On réserve toute la place d'un coup

    uint8_t* write_ptr = data.ptrw(); // Pointeur d'écriture directe

    for (const InputPacket& pkt : input_buffer)
    {
        std::memcpy(write_ptr, &pkt.sequence_id, 4);
        write_ptr += 4;

        *write_ptr = pkt.keys;
        write_ptr += 1;

        std::memcpy(write_ptr, &pkt.aim_x, 4);
        write_ptr += 4;

        std::memcpy(write_ptr, &pkt.aim_y, 4);
        write_ptr += 4;
    }

    INFO("Sending INPUT packet with sequence ID " + String::num_int64(input_pkt.sequence_id) + " | Keys: " + String::num_int64(input_pkt.keys) + " | Aim: (" + String::num_real(input_pkt.aim_x) + ", " + String::num_real(input_pkt.aim_y)+ ")");
    send_packet(static_cast<int>(PacketType::INPUT), data);
}

void GDNetworkManager::_close_socket()
{
    if (udp_socket != INVALID_SOCKET)
    {
        #ifdef _WIN32
            closesocket(udp_socket);
        #else
            close(udp_socket);
        #endif
            udp_socket = INVALID_SOCKET; // Reset the socket to an invalid state after closing it to prevent accidental reuse.
    }
}

void GDNetworkManager::_set_non_blocking(socket_t sock) {
#ifdef _WIN32
    unsigned long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

void GDNetworkManager::_bind_methods() {
    // Enregistrement de send_packet
    ClassDB::bind_method(D_METHOD("send_packet", "data"), &GDNetworkManager::send_packet);
    ClassDB::bind_method(D_METHOD("send_input", "up", "down", "left", "right", "aim_x", "aim_y"), &GDNetworkManager::send_input);
}

bool GDNetworkManager::poll()
{
    if (udp_socket == INVALID_SOCKET) {
        return false; // No socket to poll
    }

    char buffer[1024];
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Attempt to receive data from the socket
    ssize_t recv_len = recvfrom(udp_socket, buffer, sizeof(buffer), 0, (sockaddr*)&client_addr, &addr_len);
    if (recv_len > 0)
    {
        /************* Form Packet *************/
        /* ID (type uint32_t) | Packet Type (type int) | Data (type vector<char>) */

        Packet pkt;
        pkt.sender = client_addr;
        pkt.data.assign(buffer, buffer + recv_len);
        UtilityFunctions::print("Packet received from ", inet_ntoa(client_addr.sin_addr), ":", ntohs(client_addr.sin_port), " | Size: ", recv_len, " bytes");

        //sender
        std::string sender_ip = inet_ntoa(pkt.sender.sin_addr);
        int sender_port = ntohs(pkt.sender.sin_port);

        // ID
        uint32_t client_id = -1;
        std::memcpy(&client_id, pkt.data.data(), sizeof(uint32_t));

        //Packet Type
        PacketType type = static_cast<PacketType>(pkt.data[sizeof(uint32_t)]);

        if (type < PacketType::LOGIN || type > PacketType::LOGOUT)
        {
            UtilityFunctions::printerr("Received packet with unknown type from ", sender_ip.c_str(), ":", sender_port);
            return false;
        }

        std::vector<char> dataWithoutPacketTypeAndClientID(pkt.data.begin() + sizeof(uint32_t) + 1, pkt.data.end());


        switch (type)
        {
            case PacketType::LOGIN:
            {
                idForServer = client_id;

                INFO_SERVER("Server send ID info, my id is " + String::num_int64(client_id), sender_ip.c_str(), sender_port, type);
                break;
            }
            case PacketType::LOCATION:
            {
                if (client_id == idForServer) {
                    break; // On ignore notre propre position
                }

                // 1. ON LIT X ET Y EN PREMIER !
                int x, y;
                std::memcpy(&x, dataWithoutPacketTypeAndClientID.data(), sizeof(int));
                std::memcpy(&y, dataWithoutPacketTypeAndClientID.data() + sizeof(int), sizeof(int));

                // 2. Si le joueur n'existe pas, on le crée en lui donnant X et Y directement
                if (clientId_to_remotePlayer.find(client_id) == clientId_to_remotePlayer.end())
                {
                    INFO("Received a new client ID (" + String::num_int64(client_id) + ")" + " with initial position (" + String::num_int64(x) + ", " + String::num_int64(y) + ")");

                    // On passe x et y à la fonction GDScript !
                    Variant result = call("register_node", x, y);

                    Node* newNode = Object::cast_to<Node>(result);
                    clientId_to_remotePlayer[client_id] = RemotePlayer{newNode};
                }
                else
                {
                    INFO("Received position update for existing client ID (" + String::num_int64(client_id) + ")" + " with new position (" + String::num_int64(x) + ", " + String::num_int64(y) + ")");

                    // 3. Mise à jour de la position pour les frames suivantes
                    Variant pos = Vector2(x, y);
                    Node* RemotePlayerNode = clientId_to_remotePlayer[client_id].node;

                    // call_deferred est plus sûr pour la physique que call tout court
                    RemotePlayerNode->call_deferred("set_position", pos);

                    break;
                }
            }
            case PacketType::PING:
            {
                if (dataWithoutPacketTypeAndClientID.size() < sizeof(PingRequestPacket))
                {
                    UtilityFunctions::printerr("Received PING packet with insufficient data from ", sender_ip.c_str(), ":", sender_port);
                    break;
                }

                PingResponsePacket ping_resp;
                std::memcpy(&ping_resp.id, dataWithoutPacketTypeAndClientID.data(), sizeof(uint32_t));
                std::memcpy(&ping_resp.timestamp0, dataWithoutPacketTypeAndClientID.data() + sizeof(uint32_t), sizeof(uint64_t));
                std::memcpy(&ping_resp.timestamp1, dataWithoutPacketTypeAndClientID.data() + sizeof(uint32_t) + sizeof(uint64_t), sizeof(uint64_t));

                //calcule du RTT (ping)
                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count();

                uint64_t rtt = now - ping_resp.timestamp0;

                INFO_SERVER("Received PING from client " + String::num_int64(client_id) + " | RTT: " + String::num_int64(rtt) + " ms", sender_ip.c_str(), sender_port, type);

                break;
            }
            case PacketType::LOGOUT:
            {
                INFO_SERVER("Deconnection du joueur " + String::num_int64(client_id), sender_ip.c_str(), sender_port, type);
                // On retire le client de la liste des clients connectés
                if (clientId_to_remotePlayer.find(client_id) != clientId_to_remotePlayer.end())
                {
                    Node* node = clientId_to_remotePlayer[client_id].node;
                    if (node)
                    {
                        node->queue_free(); // On supprime le node associé à ce client
                    }
                    else
                    {
                        UtilityFunctions::printerr("Node for client ID ", String::num_int64(client_id), " is null.");
                    }
                    clientId_to_remotePlayer.erase(client_id); // On retire l'entrée du dictionnaire
                }
                break;
            }
            default:
            {
                INFO_SERVER("Received unhandled packet type", server_ip, server_port, type);
                break;
            }
        }

        return true;
    }
    else if (recv_len == 0)
    {
        // Connection closed by the peer
        UtilityFunctions::print("Connection closed by peer");
        return false;
    }
    else
    {
        // Check for non-blocking error
        #ifdef _WIN32
            int err = WSAGetLastError();
            // 10035 = WSAEWOULDBLOCK (C'est normal, c'est que le buffer est vide)
            if (err != 10035 && err != 0) {
                UtilityFunctions::print("Erreur Socket Windows : ", err);
            }
        #endif
        return false;
    }

    return false;
}

void GDNetworkManager::_logout()
{
    if (udp_socket != INVALID_SOCKET)
    {
        // Envoi d'un packet de logout au serveur avant de fermer le socket
        PackedByteArray empty_data; // Pas de données nécessaires pour le logout
        send_packet(static_cast<int>(PacketType::LOGOUT), empty_data);
    }
    else
    {
        UtilityFunctions::printerr("Cannot send logout packet because the socket is not initialized.");
    }
}

void GDNetworkManager::ping_server()
{
    while (keep_pinging)
    {
        send_ping();

        std::this_thread::sleep_for(std::chrono::seconds(ping_interval));
    }
}

void GDNetworkManager::send_ping()
{
    INFO("Pinging server...");

    PingRequestPacket ping_req;
    ping_req.id = next_ping_id++;
    ping_req.timestamp0 = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();

    PackedByteArray data;
    data.resize(sizeof(PingRequestPacket));
    std::memcpy(data.ptrw(), &ping_req, sizeof(PingRequestPacket));

    send_packet(static_cast<int>(PacketType::PING), data);
}

GDNetworkManager::GDNetworkManager()
{
    // Initialize any variables here.
}

GDNetworkManager::~GDNetworkManager()
{
    _logout();

    keep_pinging = false; // Stop the ping thread if it's still running

    // Add your cleanup here.
    _close_socket();
}

void GDNetworkManager::_process(double delta)
{
    // Add tick computation there
    while (poll()) {
        // Process all incoming packets until there are no more to process
    }
}

void GDNetworkManager::_ready() {
    Node::_ready();

    bind_port();

    //lancement de la fonction loop ping dans un thread séparé pour ne pas bloquer le thread principal
    INFO("Demarrage du thread de ping ...");
    std::thread ping_thread(&GDNetworkManager::ping_server, this);
    ping_thread.detach(); // Detach the thread to allow it to run independently
}
