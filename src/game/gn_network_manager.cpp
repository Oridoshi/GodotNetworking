#include "gn_network_manager.h"

#include <algorithm>
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
    UtilityFunctions::print_rich("[color=orange][WARNING][/color] ", msg);
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

void INFO_CORRECTION_LOCAL_PLAYER(String msg, int type)
{
    String start_str;
    String end_str = "[/color]";

    switch (type) {
        case 1:  start_str = "[color=pink] [INFO_CORRECTION_LOCAL_PLAYER]"; break;
        case 2:  start_str = "[color=magenta] [INFO_CORRECTION_LOCAL_PLAYER]"; break;
        case 3:  start_str = "[color=purple] [INFO_CORRECTION_LOCAL_PLAYER]"; break;
        default: start_str = "[color=orange] [INFO_CORRECTION_LOCAL_PLAYER]"; break;
    }

    UtilityFunctions::print_rich(start_str, msg, end_str);
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

        if (type == static_cast<int>(PacketType::LOGIN))
        {
            timestamp_login_send = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();
        }
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
    ClassDB::bind_method(D_METHOD("add_predict_pos", "new_x", "new_y"), &GDNetworkManager::add_predict_pos);
}


void GDNetworkManager::add_predict_pos(int new_x, int new_y)
{
    if (current_frame_id == -1)
    {
        WARN("Current frame ID is not initialized. Cannot add predicted position.");
        return;
    }

    for (const auto& entry : local_location_buffer)
    {
        if (entry.frame_id == current_frame_id)
        {
            WARN("Already have a predicted position for frame ID " + String::num_int64(current_frame_id) + ", skipping new prediction.");
            return;
        }
    }

    // verif si on a déjà une correction pour la frame précédente, si non ERR
    bool found_prev = false;
    for (const auto& entry : local_location_buffer)
    {
        if (entry.frame_id == current_frame_id - 1)
        {
            found_prev = true;
            break;
        }
    }

    if (!found_prev && !local_location_buffer.empty())
    {
        ERR("No predicted position found for previous frame ID " + String::num_int64(current_frame_id - 1) + ". This should not happen, skipping prediction for current frame ID " + String::num_int64(current_frame_id) + ".");
        return;
    }

    INFO("Added predicted position (" + String::num_int64(new_x) + ", " + String::num_int64(new_y) + ") for frame ID " + String::num_int64(current_frame_id) + " to the local location buffer.");

    // Si tout est bon, on push la nouvelle position prédite pour la frame actuelle
    local_location_buffer.push_back({current_frame_id, new_x, new_y});

    // On garde que les 20 dernières prédictions pour éviter que le buffer ne grossisse indéfiniment
    if (local_location_buffer.size() > LOCAL_LOCATION_BUFFER_SIZE)
    {
        size_t excess = local_location_buffer.size() - LOCAL_LOCATION_BUFFER_SIZE;
        local_location_buffer.erase(local_location_buffer.begin(), local_location_buffer.begin() + excess);
    }
}

void GDNetworkManager::correction_local_player(float frameID, int servX, int servY)
{
    //on vas chercher la prev position prédite pour la frame précédente
    PredictionLocalLocation prevPrediction;
    PredictionLocalLocation nextPrediction;

    bool found_prev = false;
    bool found_next = false;

    for (const auto& entry : local_location_buffer)
    {
        if (static_cast<float>(entry.frame_id) <= frameID && static_cast<float>(entry.frame_id) > frameID - 1 && !found_prev)
        {
            prevPrediction = entry;
            found_prev = true;
        }

        if (found_prev)
        {
            if (static_cast<float>(entry.frame_id) > frameID)
            {
                nextPrediction = entry;
                found_next = true;
                break; // On peut s'arrêter une fois qu'on a trouvé le next
            }
        }
    }

    if (static_cast<float>(prevPrediction.frame_id) == frameID)
    {
        float distance = Vector2(servX, servY).distance_to(Vector2(prevPrediction.x, prevPrediction.y));

        if (distance > CORRECTION_RANGE)
        {
            INFO_CORRECTION_LOCAL_PLAYER("Applying correction for local player based on previous prediction. Server position: (" + String::num_int64(servX) + ", " + String::num_int64(servY) + ") | Previous prediction: (" + String::num_int64(prevPrediction.x) + ", " + String::num_int64(prevPrediction.y) + ") | Distance: " + String::num_real(distance), 2);

            // Si la distance est trop grande, on snap directement à la position du serveur
            call_deferred("set_local_player_position", servX, servY);
        }
        else if (distance > THRESHOLD_LOCAL_LOCATION)
        {
            INFO_CORRECTION_LOCAL_PLAYER("Applying smooth correction for local player based on previous prediction. Server position: (" + String::num_int64(servX) + ", " + String::num_int64(servY) + ") | Previous prediction: (" + String::num_int64(prevPrediction.x) + ", " + String::num_int64(prevPrediction.y) + ") | Distance: " + String::num_real(distance), 1);

            //TODO: CODE RESSORT ET AMORTISEUR
        }
        else
        {
            INFO_CORRECTION_LOCAL_PLAYER("No correction needed for local player based on previous prediction. Server position: (" + String::num_int64(servX) + ", " + String::num_int64(servY) + ") | Previous prediction: (" + String::num_int64(prevPrediction.x) + ", " + String::num_int64(prevPrediction.y) + ") | Distance: " + String::num_real(distance), 3);
        }
    }
    else if (found_next)
    {
        //calcul de la pos du joueur a frameID par rapport a prev et next
        float t = (frameID - prevPrediction.frame_id) / float(nextPrediction.frame_id - prevPrediction.frame_id);
        int interp_x = prevPrediction.x + t * (nextPrediction.x - prevPrediction.x);
        int interp_y = prevPrediction.y + t * (nextPrediction.y - prevPrediction.y);

        float distance = Vector2(servX, servY).distance_to(Vector2(interp_x, interp_y));

        if (distance > CORRECTION_RANGE)
        {
            INFO_CORRECTION_LOCAL_PLAYER("Applying correction for local player based on interpolated prediction. Server position: (" + String::num_int64(servX) + ", " + String::num_int64(servY) + ") | Interpolated prediction: (" + String::num_int64(interp_x) + ", " + String::num_int64(interp_y) + ") | Distance: " + String::num_real(distance), 2);

            // Si la distance est trop grande, on snap directement à la position du serveur
            call_deferred("set_local_player_position", servX, servY);
        }
        else if (distance > THRESHOLD_LOCAL_LOCATION)
        {
            INFO_CORRECTION_LOCAL_PLAYER("Applying smooth correction for local player based on interpolated prediction. Server position: (" + String::num_int64(servX) + ", " + String::num_int64(servY) + ") | Interpolated prediction: (" + String::num_int64(interp_x) + ", " + String::num_int64(interp_y) + ") | Distance: " + String::num_real(distance), 1);

            //TODO: CODE RESSORT ET AMORTISEUR
        }
        else
        {
            INFO_CORRECTION_LOCAL_PLAYER("No correction needed for local player based on interpolated prediction. Server position: (" + String::num_int64(servX) + ", " + String::num_int64(servY) + ") | Interpolated prediction: (" + String::num_int64(interp_x) + ", " + String::num_int64(interp_y) + ") | Distance: " + String::num_real(distance), 3);
        }
    }
    else
    {
        ERR("No predicted position found for next frame ID " + String::num_int64(frameID) + ". Cannot perform correction for frame ID " + String::num_int64(frameID) + ".");
        return;
    }
}

void GDNetworkManager::update_player_location(int frameID, uint32_t client_id, int x, int y)
{
    if (client_id == idForServer)
    {
        INFO_CORRECTION_LOCAL_PLAYER("Received position update for local player with client ID (" + String::num_int64(client_id) + ")" + " with new position (" + String::num_int64(x) + ", " + String::num_int64(y) + ")", 1);
        correction_local_player(frameID, x, y);
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
        Vector2 pos = Vector2(x, y);
        Node* RemotePlayerNode = clientId_to_remotePlayer[client_id].node;

        Vector2 prevPos = RemotePlayerNode->call("get_position");
        Vector2 direction = pos - prevPos;
        RemotePlayerNode->call("AnimationHandle", direction);
        RemotePlayerNode->call("RotationHandle", direction);

        // call_deferred est plus sûr pour la physique que call tout court
        RemotePlayerNode->call_deferred("set_position", pos);

        //::OK("Updated position of player with client ID: " + String::num_int64(client_id) + " to (" + String::num_int64(x) + ", " + String::num_int64(y) + ")");
    }
}

#include <unordered_map>

void GDNetworkManager::update_world_state(double delta)
{
    currentRanderServerFrameId += SERVER_FPS * delta;

    WorldStatePacket prev_packet;
    WorldStatePacket next_packet;
    bool found_prev = false;
    bool found_next = false;

    for (int i = 0; i < world_state_buffer.size(); i++)
    {
        //INFO("Checking world state packet with frame ID: " + String::num_real(world_state_buffer[i].frame_id) + " against current render frame ID: " + String::num_real(currentRanderFrameId));

        if (world_state_buffer[i].frame_id <= currentRanderServerFrameId)
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
        if (!world_state_buffer.empty() && currentRanderServerFrameId < world_state_buffer[0].frame_id)
        {
            // On est en train d'attendre (ex: l'horloge est à 98, la 1ère frame est 100).
            // On quitte la fonction silencieusement pour cette frame. Les joueurs restent invisibles ou figés.
            return;
        }

        ERR("Not enough world state packets for interpolation or identical frame IDs. For frame ID : " + String::num_real(currentRanderServerFrameId) + " | Found prev: " + (found_prev?"TRUE":"FALSE") + " | Found next: " + (found_next?"TRUE":"FALSE")  + " | Prev frame ID: " + String::num_real(prev_packet.frame_id) + " | Next frame ID: " + String::num_real(next_packet.frame_id));
        ERR("Network desynchronization or fatal error. Quitting game...");

        _logout();

        get_tree()->quit();

        return;
    }

    float t = (currentRanderServerFrameId - prev_packet.frame_id) / float(next_packet.frame_id - prev_packet.frame_id);

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

            INFO("Found client ID " + String::num_int64(client_id) + " in previous world state with position (" + String::num_int64(xPrev) + ", " + String::num_int64(yPrev) + ")");

            // Avons-nous trouvé sa destination dans le next_packet ?
            if (next_positions.find(client_id) != next_positions.end())
            {
                int xNext = next_positions[client_id].first;
                int yNext = next_positions[client_id].second;

                // INTERPOLATION O(1) !
                int xInterpolated = xPrev + t * (xNext - xPrev);
                int yInterpolated = yPrev + t * (yNext - yPrev);

                update_player_location(currentRanderServerFrameId, client_id, xInterpolated, yInterpolated);
            }
            else
            {
                // Si on a le joueur en prev mais pas en next, on applique juste prev en attendant
                update_player_location(currentRanderServerFrameId, client_id, xPrev, yPrev);
            }
        }
        else break;
    }

    while (world_state_buffer.size() > 2 && world_state_buffer[1].frame_id <= currentRanderServerFrameId)
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
        //UtilityFunctions::print("Packet received from ", inet_ntoa(client_addr.sin_addr), ":", ntohs(client_addr.sin_port), " | Size: ", recv_len, " bytes");

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

                if (dataWithoutPacketTypeAndClientID.size() < sizeof(uint32_t))
                {
                    UtilityFunctions::printerr("Received LOGIN packet with insufficient data from ", sender_ip.c_str(), ":", sender_port);
                    break;
                }

                uint32_t serverFrameId = 0;
                std::memcpy(&serverFrameId, dataWithoutPacketTypeAndClientID.data(), sizeof(uint32_t));
                currentRanderServerFrameId = (int)serverFrameId - RENDER_DELAY;

                INFO_SERVER("Server send login info, my id is " + String::num_int64(client_id) + " | Initial frame ID: " + String::num_int64(currentRanderServerFrameId), sender_ip.c_str(), sender_port, type);

                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count();

                uint64_t rtt = now - timestamp_login_send;

                float nbFramePassed = (static_cast<float>(rtt) / 2.f) / (1000.f / static_cast<float>(SERVER_FPS));
                current_frame_id = (int)(static_cast<float>(serverFrameId) + nbFramePassed);

                INFO("Estimated current frame ID based on RTT: " + String::num_int64(current_frame_id) + " | timestamp login send: " + String::num_int64(timestamp_login_send) + " | timestamp now: " + String::num_int64(now) + " | RTT: " + String::num_int64(rtt) + " ms | Frames passed: " + String::num_real(nbFramePassed));

                break;
            }
            case PacketType::PING:
            {
                if (dataWithoutPacketTypeAndClientID.size() < sizeof(PingResponsePacket))
                {
                    UtilityFunctions::printerr("Received PING packet with insufficient data from ", sender_ip.c_str(), ":", sender_port);
                    break;
                }

                PingResponsePacket ping_resp;
                std::memcpy(&ping_resp.id,         dataWithoutPacketTypeAndClientID.data(),                                        sizeof(uint32_t));
                std::memcpy(&ping_resp.timestamp0, dataWithoutPacketTypeAndClientID.data() + sizeof(uint32_t),                     sizeof(uint64_t));
                std::memcpy(&ping_resp.timestamp1, dataWithoutPacketTypeAndClientID.data() + sizeof(uint32_t) + sizeof(uint64_t),  sizeof(uint64_t));

                uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count();

                uint64_t rtt = now - ping_resp.timestamp0;

                // One-way delay client → serveur (utile pour détecter une asymétrie réseau)
                // timestamp1 = moment où le serveur a reçu/répondu
                uint64_t oneWayDelay = ping_resp.timestamp1 - ping_resp.timestamp0;

                float oneWayReturn = static_cast<float>(now - ping_resp.timestamp1);

                INFO_SERVER("Received PING | RTT: " + String::num_int64(rtt) + " ms | One-way: " + String::num_int64(oneWayDelay) + " ms | One-way return: " + String::num_int64((uint64_t)oneWayReturn) + " ms", sender_ip.c_str(), sender_port, type);


                float nbFramePassed = oneWayReturn / (1000.f / static_cast<float>(SERVER_FPS));
                int expectedFrameId = currentRanderServerFrameId + (int)nbFramePassed;

                int frameDiff = expectedFrameId - current_frame_id;

                if (frameDiff > 5)
                {
                    INFO_SERVER("Client is behind by " + String::num_int64(frameDiff) + " frames. Expected: " + String::num_int64(expectedFrameId) + " | Current: " + String::num_int64(current_frame_id), sender_ip.c_str(), sender_port, type);
                    // Accélération ici
                }
                else if (frameDiff < -5)
                {
                    INFO_SERVER("Client is ahead by " + String::num_int64(-frameDiff) + " frames. Expected: " + String::num_int64(expectedFrameId) + " | Current: " + String::num_int64(current_frame_id), sender_ip.c_str(), sender_port, type);
                    // Ralentissement ici
                }
                else
                {
                    INFO_SERVER("Client frame is ok | diff: " + String::num_int64(frameDiff), sender_ip.c_str(), sender_port, type);
                }

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

void GDNetworkManager::_physics_process(double delta)
{
    if (current_frame_id == -1)
    {
        // On n'a pas encore reçu de frame ID du serveur, on ne peut pas faire de prédiction
        return;
    }

    current_frame_id++;
}

void GDNetworkManager::_ready() {
    Node::_ready();

    bind_port();

    //lancement de la fonction loop ping dans un thread séparé pour ne pas bloquer le thread principal
    INFO("Demarrage du thread de ping ...");
    std::thread ping_thread(&GDNetworkManager::ping_server, this);
    ping_thread.detach(); // Detach the thread to allow it to run independently
}
