#include "gn_network_manager.h"

#include <chrono>
#include <string>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include "../commun/protocol.hpp"
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
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

void INFO(const String& msg)
{
    // Affiche [INFO] en bleu cyan
    UtilityFunctions::print_rich("[color=blue][INFO][/color] ", msg);
}

void OK(const String& msg)
{
    // Affiche [OK] en vert et en gras ([b])
    UtilityFunctions::print_rich("[color=green][b][OK][/b][/color] ", msg);
}

void WARN(const String& msg)
{
    // Affiche [WARNING] en jaune
    UtilityFunctions::print_rich("[color=yellow][WARNING][/color] ", msg);
}

void ERR(const String& msg)
{
    UtilityFunctions::print_rich("[color=red][b][ERROR][/b][/color] ", msg);
}

void INFO_SERVER(String msg, String sender_ip, int sender_port, PacketType type)
{
    String type_str;
    switch (type) {
        case PacketType::LOGIN:      type_str = "[color=cyan]LOGIN[/color]"; break;
        case PacketType::WORLDSTATE: type_str = "[color=blue]WORLDSTATE[/color]"; break;
        case PacketType::INPUT:      type_str = "[color=purple]INPUT[/color]"; break;
        case PacketType::PING:       type_str = "[color=pink]PING[/color]"; break;
        case PacketType::LOGOUT:     type_str = "[color=red]LOGOUT[/color]"; break;
        default:                     type_str = "[color=orange]UNKNOWN[/color]"; break;
    }

    UtilityFunctions::print_rich("[color=cyan][INFO SERVER][/color] ", msg, " | From: ", sender_ip, ":", sender_port, " | Type: ", type_str);
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

void GDNetworkManager::update_player_location(uint32_t client_id, int x, int y)
{
    INFO("Update Loc");
    if (client_id == idForServer) {
        return; // On ignore notre propre position
    }

    // 2. Si le joueur n'existe pas, on le crée en lui donnant X et Y directement
    if (clientId_to_remotePlayer.find(client_id) == clientId_to_remotePlayer.end())
    {
        INFO("Received a new client ID (" + String::num_int64(client_id) + ")" + " with initial position (" + String::num_int64(x) + ", " + String::num_int64(y) + ")");

        // On passe x et y à la fonction GDScript !
        Variant result = call("register_node", x, y);

        Node* newNode = Object::cast_to<Node>(result);
        clientId_to_remotePlayer[client_id] = RemotePlayer{newNode};


        ::OK("New player registered with client ID: " + String::num_int64(client_id));
    }
    else
    {
        //INFO("Received position update for existing client ID (" + String::num_int64(client_id) + ")" + " with new position (" + String::num_int64(x) + ", " + String::num_int64(y) + ")");

        // 3. Mise à jour de la position pour les frames suivantes
        Variant pos = Vector2(x, y);
        Node* RemotePlayerNode = clientId_to_remotePlayer[client_id].node;

        // call_deferred est plus sûr pour la physique que call tout court
        RemotePlayerNode->call_deferred("set_position", pos);

        //::OK("Updated position of player with client ID: " + String::num_int64(client_id) + " to (" + String::num_int64(x) + ", " + String::num_int64(y) + ")");
    }
}

#include <unordered_map>

void GDNetworkManager::update_world_state(double delta)
{
    currentRanderFrameId += SERVER_FPS * delta;

    WorldStatePacket prev_packet;
    WorldStatePacket next_packet;
    bool found_prev = false;
    bool found_next = false;

    for (int i = 0; i < world_state_buffer.size(); i++)
    {
        //INFO("Checking world state packet with frame ID: " + String::num_real(world_state_buffer[i].frame_id) + " against current render frame ID: " + String::num_real(currentRanderFrameId));

        if (world_state_buffer[i].frame_id <= currentRanderFrameId)
        {
            prev_packet = world_state_buffer[i];
            found_prev = true;
        }
        else
        {
            next_packet = world_state_buffer[i];
            found_next = true;
            break;
        }
    }

    if (!found_prev || !found_next || next_packet.frame_id == prev_packet.frame_id)
    {
        if (!world_state_buffer.empty() && currentRanderFrameId < world_state_buffer[0].frame_id)
        {
            // On est en train d'attendre (ex: l'horloge est à 98, la 1ère frame est 100).
            // On quitte la fonction silencieusement pour cette frame. Les joueurs restent invisibles ou figés.
            return;
        }

        ERR("Not enough world state packets for interpolation or identical frame IDs. For frame ID : " + String::num_real(currentRanderFrameId) + " | Found prev: " + (found_prev?"TRUE":"FALSE") + " | Found next: " + (found_next?"TRUE":"FALSE")  + " | Prev frame ID: " + String::num_real(prev_packet.frame_id) + " | Next frame ID: " + String::num_real(next_packet.frame_id));
        ERR("Network desynchronization or fatal error. Quitting game...");

        _logout();

        get_tree()->quit();

        return;
    }

    float t = (currentRanderFrameId - prev_packet.frame_id) / float(next_packet.frame_id - prev_packet.frame_id);

    std::unordered_map<uint32_t, std::pair<int, int>> next_positions;
    int j = 0;
    while (j < next_packet.data.size())
    {
        PacketType type = static_cast<PacketType>(next_packet.data[j]);
        j++;
        if (type == PacketType::LOCATION)
        {
            if (j + sizeof(uint32_t) + sizeof(int) * 2 > next_packet.data.size()) break;

            uint32_t id;
            int next_x, next_y;
            std::memcpy(&id, next_packet.data.data() + j, sizeof(uint32_t)); j += sizeof(uint32_t);
            std::memcpy(&next_x, next_packet.data.data() + j, sizeof(int)); j += sizeof(int);
            std::memcpy(&next_y, next_packet.data.data() + j, sizeof(int)); j += sizeof(int);

            next_positions[id] = {next_x, next_y}; // On range pour plus tard
        }
        else break;
    }

    int i = 0;
    while (i < prev_packet.data.size())
    {
        PacketType type = static_cast<PacketType>(prev_packet.data[i]);
        i++;

        if (type == PacketType::LOCATION)
        {
            //INFO("Processing LOCATION packet in world state interpolation...");

            if (i + sizeof(uint32_t) + sizeof(int) * 2 > prev_packet.data.size()) break;

            uint32_t client_id;
            int xPrev, yPrev;
            std::memcpy(&client_id, prev_packet.data.data() + i, sizeof(uint32_t)); i += sizeof(uint32_t);
            std::memcpy(&xPrev, prev_packet.data.data() + i, sizeof(int)); i += sizeof(int);
            std::memcpy(&yPrev, prev_packet.data.data() + i, sizeof(int)); i += sizeof(int);

            if (client_id == idForServer)
            {
                INFO("Skipping interpolation for our own client ID: " + String::num_int64(client_id));
                continue; // On ignore notre propre position
            }

            INFO("Found client ID " + String::num_int64(client_id) + " in previous world state with position (" + String::num_int64(xPrev) + ", " + String::num_int64(yPrev) + ")");

            // Avons-nous trouvé sa destination dans le next_packet ?
            if (next_positions.find(client_id) != next_positions.end())
            {
                int xNext = next_positions[client_id].first;
                int yNext = next_positions[client_id].second;

                // INTERPOLATION O(1) !
                int xInterpolated = xPrev + t * (xNext - xPrev);
                int yInterpolated = yPrev + t * (yNext - yPrev);

                update_player_location(client_id, xInterpolated, yInterpolated);
            }
            else
            {
                // Si on a le joueur en prev mais pas en next, on applique juste prev en attendant
                update_player_location(client_id, xPrev, yPrev);
            }
        }
        else break;
    }

    while (world_state_buffer.size() > 2 && world_state_buffer[1].frame_id <= currentRanderFrameId)
    {
        world_state_buffer.erase(world_state_buffer.begin());
    }
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

                //recup de frame id du serveur
                if (dataWithoutPacketTypeAndClientID.size() < sizeof(uint32_t))
                {
                    UtilityFunctions::printerr("Received LOGIN packet with insufficient data from ", sender_ip.c_str(), ":", sender_port);
                    break;
                }

                int currentRanderFrameIdTemp = 0;
                std::memcpy(&currentRanderFrameIdTemp, dataWithoutPacketTypeAndClientID.data(), sizeof(uint32_t));
                currentRanderFrameId = currentRanderFrameIdTemp - RENDER_DELAY;

                INFO_SERVER("Server send login info, my id is " + String::num_int64(client_id) + " | Initial frame ID: " + String::num_real(currentRanderFrameId), sender_ip.c_str(), sender_port, type);
                break;
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
            case PacketType::WORLDSTATE:
            {
                WorldStatePacket world_state_pkt;
                std::memcpy(&world_state_pkt.frame_id, dataWithoutPacketTypeAndClientID.data(), sizeof(uint32_t));
                world_state_pkt.data.assign(dataWithoutPacketTypeAndClientID.begin() + sizeof(uint32_t), dataWithoutPacketTypeAndClientID.end());

                world_state_buffer.push_back(world_state_pkt);

                INFO_SERVER("Received WORLDSTATE packet with frame ID " + String::num_int64(world_state_pkt.frame_id) + " and data size " + String::num_int64(world_state_pkt.data.size()) + " bytes", sender_ip.c_str(), sender_port, type);

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
    while (poll()) {/*Process all incoming packets until there are no more to process*/}

    if (world_state_buffer.size() > WORLD_STATE_BUFFER_SIZE)
    {
        update_world_state(delta);
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
