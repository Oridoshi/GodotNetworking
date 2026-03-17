#include "gn_network_manager.h"

#include <string>
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
        case PacketType::LOGIN: type_str = "LOGIN"; break;
        case PacketType::INPUT: type_str = "VECTOR"; break;
        case PacketType::LOGOUT: type_str = "LOGOUT"; break;
        default: type_str = "UNKNOWN"; break;
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
            case PacketType::INPUT:
            {
                // 1. Sécurité : Vérifier si le client est connu
                if (clientId_to_remotePlayer.find(client_id) == clientId_to_remotePlayer.end()) {
                    INFO_SERVER("Received INPUT packet from unknown client ID " + String::num_int64(client_id) + ". Ignoring.", sender_ip.c_str(), sender_port, type);
                    break;
                }

                // 2. Récupération des inputs
                std::vector<InputPacket> receive_input_buffer;
                for (size_t offset = 0; offset + PACKET_SIZE <= dataWithoutPacketTypeAndClientID.size(); offset += PACKET_SIZE) {
                    InputPacket pkt;
                    std::memcpy(&pkt.sequence_id, dataWithoutPacketTypeAndClientID.data() + offset, 4);
                    pkt.keys = dataWithoutPacketTypeAndClientID[offset + 4];
                    std::memcpy(&pkt.aim_x, dataWithoutPacketTypeAndClientID.data() + offset + 5, 4);
                    std::memcpy(&pkt.aim_y, dataWithoutPacketTypeAndClientID.data() + offset + 9, 4);
                    receive_input_buffer.push_back(pkt);
                }

                if (receive_input_buffer.empty()) break; // Sécurité anti-crash

                Node* node = clientId_to_remotePlayer[client_id].node;
                if (node) {
                    int decalage = -1;
                    bool find_next = false;

                    // 3. Recherche de la séquence attendue
                    do {
                        decalage++;
                        InputPacket check_pkt = receive_input_buffer[decalage];
                        find_next = (check_pkt.sequence_id == clientId_to_remotePlayer[client_id].next_sequence_id);

                        INFO("Checking packet with sequence ID " + String::num_int64(check_pkt.sequence_id) + " against expected ID " + String::num_int64(clientId_to_remotePlayer[client_id].next_sequence_id) + " | Found: " + (find_next ? "YES" : "NO"));

                        if (decalage >= (int)receive_input_buffer.size() - 1 && !find_next) {
                            INFO("Expected sequence ID " + String::num_int64(clientId_to_remotePlayer[client_id].next_sequence_id) + " not found. Latest received ID is " + String::num_int64(check_pkt.sequence_id) + ". Assuming packet loss and skipping to the latest.");
                            // Trop de paquets perdus : on saute directement au plus récent
                            clientId_to_remotePlayer[client_id].next_sequence_id = check_pkt.sequence_id;
                            find_next = true;
                        }
                    } while (!find_next);

                    // 4. Traitement chronologique (du plus vieux au plus récent)
                    for (int i = decalage; i >= 0; i--) {
                        // ON UTILISE L'INDEX i ICI !
                        InputPacket& current_pkt = receive_input_buffer[i];
                        INFO("Processing packet with sequence ID " + String::num_int64(current_pkt.sequence_id) + " | Keys: " + String::num_int64(current_pkt.keys) + " | Aim: (" + String::num_real(current_pkt.aim_x) + ", " + String::num_real(current_pkt.aim_y)+ ")");

                        bool up    = (current_pkt.keys & 1) != 0;
                        bool down  = (current_pkt.keys & 2) != 0;
                        bool left  = (current_pkt.keys & 4) != 0;
                        bool right = (current_pkt.keys & 8) != 0;

                        // Mise à jour de la séquence
                        clientId_to_remotePlayer[client_id].next_sequence_id++;

                        // Calcul de la direction pour CE paquet spécifique
                        Vector2 direction = Vector2((right ? 1 : 0) - (left ? 1 : 0),
                                                    (down ? 1 : 0) - (up ? 1 : 0)).normalized();

                        // On applique le mouvement au node Godot
                        node->call("MoveHandle", direction);

                        // Si tu as besoin de la souris, passe-la aussi :
                        // node->call("AimHandle", Vector2(current_pkt.aim_x, current_pkt.aim_y));
                    }
                }
                break;
            }
            case PacketType::NEW_PLAYER:
            {
                if (clientId_to_remotePlayer.find(client_id) != clientId_to_remotePlayer.end())
                {
                    INFO_SERVER("Received NEW_PLAYER packet for an already registered client ID " + String::num_int64(client_id) + ". Ignoring.", sender_ip.c_str(), sender_port, type);
                    break;
                }

                INFO("Received a new client ID (" + String::num_int64(client_id) + ") with SEVER IP " + sender_ip.c_str() + " and port " + String::num_int64(sender_port));

                //mise à jour location du joueur
                int x, y;

                // on recupère X (4 premiers octets)
                std::memcpy(&x, dataWithoutPacketTypeAndClientID.data(), sizeof(int));

                // on recupère Y (4 octets suivants)
                std::memcpy(&y, dataWithoutPacketTypeAndClientID.data() + sizeof(int), sizeof(int));

                // Création d'un nouveau node pour ce client
                Variant result = call("register_node");
                Node* newNode = Object::cast_to<Node>(result);
                clientId_to_remotePlayer[client_id] = RemotePlayer{newNode, 0}; // On stocke le node associé à ce client
                // On suppose que les nodes ont une méthode set_position(Vector2) pour mettre à jour leur position
                Variant pos = Vector2(x, y);
                newNode->call("set_position", pos);

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

GDNetworkManager::GDNetworkManager()
{
    // Initialize any variables here.
}

GDNetworkManager::~GDNetworkManager()
{
    _logout();

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
}
