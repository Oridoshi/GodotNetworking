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
        UtilityFunctions::printerr("Failed to bind the socket to port %d", port);
        _close_socket();
        return false;
    }

    UtilityFunctions::print("Socket successfully bound to port %d", port);
    return true;
}

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

void GDNetworkManager::_bind_methods()
{

}

GDNetworkManager::GDNetworkManager()
{
    // Initialize any variables here.
}

GDNetworkManager::~GDNetworkManager()
{
    // Add your cleanup here.
}

void GDNetworkManager::_process(double delta)
{
    // Add tick computation there
}