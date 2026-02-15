#include "gn_network_manager.h"
#include <godot_cpp/core/class_db.hpp>

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

    ~WSASocketInitializer() {
        WSACleanup();
    }
};

// Instance statique : elle s'initialisera une seule fois au lancement du jeu
#ifdef _WIN32
static WSASocketInitializer g_wsa_init;
#endif

bool GDNetworkManager::bind_port(int port)
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
    server_addr.sin_port = htons(port); // Convert port to network byte order

    // Bind the socket to the specified port
    if (bind(udp_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        UtilityFunctions::printerr("Failed to bind the socket to port : ", port);
        _close_socket();
        return false;
    }

    UtilityFunctions::print("Socket successfully bound to port : ", port);
    return true;
}

void GDNetworkManager::send_packet(String ip, int port, PackedByteArray data) {
    if (udp_socket == INVALID_SOCKET) {
        UtilityFunctions::printerr("Socket is not initialized. Please bind to a port first.");
        return;
    }

    if (data.is_empty()) {
        UtilityFunctions::print("Skipping empty packet.");
        return;
    }

    sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr)); // Initialisation propre
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(static_cast<uint16_t>(port));

    // Conversion de l'IP avec vérification
    CharString ip_utf8 = ip.utf8();
    int pton_res = inet_pton(AF_INET, ip_utf8.get_data(), &dest_addr.sin_addr);

    if (pton_res <= 0) {
        UtilityFunctions::printerr("Invalid IP address: ", ip);
        return;
    }

    // Utilisation de .read() pour accéder aux données sans copier
    const uint8_t* raw_data = data.ptr();
    size_t data_len = data.size();

    ssize_t sent_len = sendto(udp_socket, reinterpret_cast<const char*>(raw_data),
                              static_cast<int>(data_len), 0,
                              (sockaddr*)&dest_addr, sizeof(dest_addr));

    if (sent_len == SOCKET_ERROR) {
        #ifdef _WIN32
                int err = WSAGetLastError();
        #else
                int err = errno;
        #endif
        UtilityFunctions::printerr("Failed to send packet to ", ip, ":", port, " | Error code: ", err);
    } else {
        UtilityFunctions::print("Packet sent (", sent_len, " bytes) to ", ip_utf8, ":", port);
    }
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
    // Enregistrement de la méthode bind_port
    // D_METHOD prend le nom de la fonction côté Godot, puis le nom des arguments
    ClassDB::bind_method(D_METHOD("bind_port", "port"), &GDNetworkManager::bind_port);

    // Enregistrement de send_packet
    ClassDB::bind_method(D_METHOD("send_packet", "ip", "port", "data"), &GDNetworkManager::send_packet);

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
        // On convertit le buffer brut en String Godot
        String message_contenu = String::utf8(buffer, (int)recv_len);

        String sender_ip = inet_ntoa(client_addr.sin_addr);
        int sender_port = ntohs(client_addr.sin_port);

        UtilityFunctions::print(">> RECU de ", sender_ip, ":", sender_port, " | Contenu: ", message_contenu);
        return true;
    } else if (recv_len == 0) {
        // Connection closed by the peer
        UtilityFunctions::print("Connection closed by peer");
        return false;
    } else {
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

GDNetworkManager::GDNetworkManager()
{
    // Initialize any variables here.
}

GDNetworkManager::~GDNetworkManager()
{
    // Add your cleanup here.
    _close_socket();
}

void GDNetworkManager::_process(double delta)
{
    // Add tick computation there
    while (poll());
}