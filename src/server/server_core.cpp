#include "server_core.hpp"

// Method to make some log
void OK(std::string msg)
{
    std::cout << "[OK] " << msg << std::endl;
}

void ERR(std::string msg)
{
    std::cerr << "[ERROR] " << msg << std::endl;
}

void WARN(std::string msg)
{
    std::cerr << "[WARNING] " << msg << std::endl;
}

void INFO(std::string msg)
{
    std::cout << "[INFO] " << msg << std::endl;
}

void INFO_FROM_CLIENT(std::string msg, std::string sender_ip, int sender_port, PacketType type)
{
    std::string type_str;
    switch (type) {
        case PacketType::LOGIN: type_str = "LOGIN"; break;
        case PacketType::VECTOR: type_str = "VECTOR"; break;
        case PacketType::ROTATOR: type_str = "ROTATOR"; break;
        case PacketType::INT: type_str = "INT"; break;
        case PacketType::STRING: type_str = "STRING"; break;
        case PacketType::LOGOUT: type_str = "LOGOUT"; break;
        default: type_str = "UNKNOWN"; break;
    }

    std::cout << "[INFO CLIENT] " << msg << " | From: " << sender_ip << ":" << sender_port << " | Type: " << type_str << std::endl;
}

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
            ERR("WSAStartup Failed");
        }
    }

    ~WSASocketInitializer() {
        WSACleanup();
    }
};

// Instance statique : elle s'initialisera une seule fois au lancement du jeu
#ifdef _WIN32
static WSASocketInitializer g_wsa_init;
#endif

void _close_socket()
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

void _set_non_blocking(socket_t sock) {
#ifdef _WIN32
    unsigned long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

bool bind_port(int port)
{
    _close_socket(); // Close any existing socket before creating a new one to ensure we don't have multiple sockets open at the same time.

    // Creation of the UDP socket
    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket == INVALID_SOCKET)
    {
        ERR("Failed to create the UDP socket");
        return false;
    }

    _set_non_blocking(udp_socket); // Set the socket to non-blocking mode to prevent blocking the main thread during network operations.
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all available interfaces
    server_addr.sin_port = htons(port); // Convert port to network byte order

    // Bind the socket to the specified port
    if (bind(udp_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        ERR("Failed to create the UDP socket on port : " + std::to_string(port));
        _close_socket();
        return false;
    }

    return true;
}

void send_packet(PacketType type, uint32_t id, std::string ip, int port, std::vector<char>& data)
{
    if (udp_socket == INVALID_SOCKET)
    {
        ERR("Socket is not initialized. Please bind to a port first.");
        return;
    }

    if (data.empty())
    {
        ERR("Skipping empty packet.");
        return;
    }

    //copy des data dans un nouveau vector pour ajouter l'id et le type du packet devant
    std::vector<char> packet = data;


    sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr)); // Initialisation propre
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(static_cast<uint16_t>(port));

    // Conversion de l'IP avec vérification
    int pton_res = inet_pton(AF_INET, ip.c_str(), &dest_addr.sin_addr);

    if (pton_res <= 0)
    {
        ERR("Invalid IP address: " + ip);
        return;
    }

    //ajout du type du packet en premier pour ajouté juste après l'id du client devant
    packet.insert(packet.begin(), static_cast<char>(type));

    //ajout de l'id en premier
    packet.insert(packet.begin(), reinterpret_cast<char*>(&id), reinterpret_cast<char*>(&id) + sizeof(uint32_t));

    // Utilisation de .read() pour accéder aux données sans copier
    const uint8_t* raw_data = reinterpret_cast<const uint8_t*>(packet.data());
    size_t data_len = packet.size();

    ssize_t sent_len = sendto(udp_socket, reinterpret_cast<const char*>(raw_data),
                              static_cast<int>(data_len), 0,
                              (sockaddr*)&dest_addr, sizeof(dest_addr));

    if (sent_len == SOCKET_ERROR) {
#ifdef _WIN32
        int err = WSAGetLastError();
#else
        int err = errno;
#endif
        ERR("Failed to send packet to " + ip + ":" + std::to_string(port) + " | Error code: " + std::to_string(err));
    } else {
        INFO("Packet sent (" + std::to_string(sent_len) + " bytes) to " + ip + ":" + std::to_string(port));

        /* Affichage des info envoyer
        std::string type_str;
        switch (type) {
            case PacketType::LOGIN: type_str = "LOGIN"; break;
            case PacketType::VECTOR: type_str = "VECTOR"; break;
            case PacketType::ROTATOR: type_str = "ROTATOR"; break;
            case PacketType::INT: type_str = "INT"; break;
            case PacketType::STRING: type_str = "STRING"; break;
            case PacketType::LOGOUT: type_str = "LOGOUT"; break;
            default: type_str = "UNKNOWN"; break;
        }

        INFO("Packet details | Type: " + type_str + " | ID: " + std::to_string(id) + " | Data size: " + std::to_string(data.size() - sizeof(uint32_t) - 1) + " bytes to " + ip + ":" + std::to_string(port));
        */
    }
}

bool poll()
{
    if (udp_socket == INVALID_SOCKET)
    {
        return false; // No socket to poll
    }

    char buffer[1024];
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Attempt to receive data from the socket
    ssize_t recv_len = recvfrom(udp_socket, buffer, sizeof(buffer), 0, (sockaddr*)&client_addr, &addr_len);
    if (recv_len >= 0)
    {
        // On ajoute le packet à la file d'attente des packets entrants
        Packet pkt;
        pkt.sender = client_addr;
        pkt.data.assign(buffer, buffer + recv_len);
        INFO("Packet received from " + std::string(inet_ntoa(client_addr.sin_addr)) + ":" + std::to_string(ntohs(client_addr.sin_port)) + " | Size: " + std::to_string(recv_len) + " bytes");

        {
            std::lock_guard<std::mutex> lock(nctx->queue_mutex);
            nctx->incoming_packets.push(pkt);
        }

        INFO("Added packet to the incoming queue. Queue size: " + std::to_string(nctx->incoming_packets.size()));

        return true;
    }
    else
    {
        // Check for non-blocking error
#ifdef _WIN32
        int err = WSAGetLastError();
        // 10035 = WSAEWOULDBLOCK (C'est normal, c'est que le buffer est vide)
        if (err != 10035 && err != 0)
        {
            ERR("Erreur Socket Windows : " + std::to_string(err));
        }
#endif
        return false;
    }

    return false;
}

void network_worker(NetworkContext* ctx) {
    OK("Network worker started on port : " + std::to_string(ctx->port));

    while (ctx->running) {
        poll();
    }
}

void read_incoming_packet()
{
    //INFO("Reading incoming packets...");

    std::queue<Packet> incoming_packets_local;

    {
        std::lock_guard<std::mutex> lock(nctx->queue_mutex);
        std::swap(incoming_packets_local, nctx->incoming_packets);
    }

    while (!incoming_packets_local.empty())
    {
        Packet pkt = incoming_packets_local.front();
        incoming_packets_local.pop();

        //sender
        std::string sender_ip = inet_ntoa(pkt.sender.sin_addr);
        int sender_port = ntohs(pkt.sender.sin_port);

        //Packet
        if (pkt.data.size() < sizeof(uint32_t) + 1) // On s'assure qu'on a au moins l'id du client et le type du packet
        {
            WARN("Received packet that is too small from " + sender_ip + ":" + std::to_string(sender_port));
            continue; // Skip processing this packet
        }

        // ID
        uint32_t client_id = 0;
        std::memcpy(&client_id, pkt.data.data(), sizeof(uint32_t));

        //Packet Type
        PacketType type = static_cast<PacketType>(pkt.data[sizeof(uint32_t)]);

        if (type < PacketType::LOGIN || type > PacketType::LOGOUT)
        {
            WARN("Received packet with unknown type from " + sender_ip + ":" + std::to_string(sender_port));
            continue; // Skip processing this packet
        }

        std::vector<char> dataWithoutPacketTypeAndClientID(pkt.data.begin() + sizeof(uint32_t) + 1, pkt.data.end());

        if (type == PacketType::LOGIN)
        {
            INFO_FROM_CLIENT("New client connected", sender_ip, sender_port, type);

            entt::entity new_entity = registry->create();
            INFO("Added new client to the registry: " + std::to_string(static_cast<uint32_t>(new_entity)));
            clients[idPlayerStatic++] = new_entity;

            //ajout des location du joueur
            int x, y;

            // on recupère X (4 premiers octets)
            std::memcpy(&x, dataWithoutPacketTypeAndClientID.data(), sizeof(int));

            // on recupère Y (4 octets suivants)
            std::memcpy(&y, dataWithoutPacketTypeAndClientID.data() + sizeof(int), sizeof(int));

            //set connection info
            registry->emplace<PlayerConnectionInfo>(new_entity, sender_ip, sender_port);
            
            //set inital location
            registry->emplace<Location>(new_entity, x, y, true);

            //print la position du joueur
            INFO("Initial position of the new player: (" + std::to_string(x) + ", " + std::to_string(y) + ")");

            //envois de l'id au joueur
            std::vector<char> packet_data;
            uint32_t client_id = idPlayerStatic - 1; // L'id du client est l'id de l'entité dans le registre, qui correspond à idPlayerStatic - 1
            packet_data.insert(packet_data.end(), reinterpret_cast<char*>(&client_id), reinterpret_cast<char*>(&client_id) + sizeof(uint32_t));
            send_packet(PacketType::LOGIN, client_id, sender_ip, sender_port, packet_data);
        }

        if (type == PacketType::VECTOR)
        {
            //mise à jour location du joueur
            int x, y;

            // on recupère X (4 premiers octets)
            std::memcpy(&x, dataWithoutPacketTypeAndClientID.data(), sizeof(int));

            // on recupère Y (4 octets suivants)
            std::memcpy(&y, dataWithoutPacketTypeAndClientID.data() + sizeof(int), sizeof(int));

            registry->emplace_or_replace<Location>(clients[client_id], x, y, true);
        }
        else if (type == PacketType::LOGOUT)
        {
            INFO_FROM_CLIENT("Client disconnected", sender_ip, sender_port, type);

            // On retire le client de la liste des clients connectés
            if (clients.find(client_id) != clients.end())
            {
                registry->destroy(clients[client_id]);
                clients.erase(client_id);
                INFO("Removed client " + std::to_string(client_id) + " from the registry.");
            }
        }
        else
        {
            INFO_FROM_CLIENT("Received unhandled packet type", sender_ip, sender_port, type);
        }

    }
}

void update()
{
    //envoie des positions de tous les joueurs à tous les joueurs
    for (const auto& [client_key, entity] : clients)
    {
        Location& loc = registry->get<Location>(entity);

        if (loc.needToBroadcast)
        {
            // On construit le packet à envoyer
            std::vector<char> packet_data;

            // Ajout de X et Y au packet
            int x = loc.x;
            int y = loc.y;
            packet_data.insert(packet_data.end(), reinterpret_cast<char*>(&x), reinterpret_cast<char*>(&x) + sizeof(int));
            packet_data.insert(packet_data.end(), reinterpret_cast<char*>(&y), reinterpret_cast<char*>(&y) + sizeof(int));

            // Envoi du packet à tous les clients
            for (const auto& [other_client_key, other_entity] : clients) {
                PlayerConnectionInfo& info = registry->get<PlayerConnectionInfo>(other_entity);
                std::string ip = info.ip;
                int port = info.port;

                send_packet(PacketType::VECTOR, client_key, ip, port, packet_data);
            }

            // On reset le flag de broadcast
            loc.needToBroadcast = false;
        }
    }
}

int main() {
    INFO("Demarrage du serveur...");

    //a mettre dans le thread const std::string prefixLog = " THREAD DU CLIENT - " + std::to_string(client_id) + " : ";

    const int PORT = 25555; // Port d'écoute du serveur
    bool running = false;
    nctx = new NetworkContext{PORT, &running};
    registry = new entt::registry();

    if (bind_port(PORT))
    {
        OK("Server is running on port : " + std::to_string(PORT));
        running = true;
    }
    else
    {
        ERR("Failed to start the server on port : " + std::to_string(PORT));
        return 1;
    }

    INFO("Demarrage du thread reseau...");
    std::thread net_thread(network_worker, nctx);

    while (running) {
        // Read all incoming packets
        read_incoming_packet();

        update();

        // On dort un peu pour ne pas brûler le CPU (60 FPS)
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    net_thread.join(); // Wait for the network thread to finish before exiting the main thread

    _close_socket();
    return 0;
}