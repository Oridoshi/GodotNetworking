#pragma once

#include <unordered_map>
#include <vector>
#include <godot_cpp/classes/node.hpp>

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

#include "protocol.hpp"

namespace godot {
    struct GDReplicatedNode {
        ObjectID node_id;
        std::vector<StringName> properties;

        // Check if node is still valid
        bool is_valid() const {
            return Object::cast_to<Node>(ObjectDB::get_instance(node_id)) != nullptr;
        }

        Node* get_node() const {
            return Object::cast_to<Node>(ObjectDB::get_instance(node_id));
        }
    };

    struct Packet {
        sockaddr_in sender;
        std::vector<char> data;
    };

    class GDNetworkManager : public Node {
        GDCLASS(GDNetworkManager, Node)

    /********************************/
    /*           VARIABLE           */
    /********************************/
    private:
        socket_t udp_socket = INVALID_SOCKET;

        std::vector<GDReplicatedNode> replicated_nodes;

        std::string server_ip_and_port; // Format: "IP:Port"

        uint32_t idForServer = -1;

        String server_ip = "192.168.2.75";

        int const server_port = 25555;

        // dictionaire pour lier joueur en loc avec server
        std::unordered_map <uint32_t, Node*> client_id_to_node;

    protected:

    public:

    /********************************/
    /*           FONCTION           */
    /********************************/
    private:
        /**
         * Method to set the socket to non-blocking mode.
         * This is necessary to prevent blocking the main thread during network operations.
         * @param sock The socket to set to non-blocking mode.
         */
        void _set_non_blocking(socket_t sock);

        /**
         * Method to set close the socket safely on all platforms
         */
        void _close_socket();

        void _logout();

    protected:
        static void _bind_methods();

    public:
        GDNetworkManager();
        ~GDNetworkManager();

        void _process(double delta) override;

        void _ready() override;

        /**
         * methods to bind to a port to receive data.
         * This is used for both server and P2P peer.
         * It creates a UDP socket and binds it to the specified port.
         * @note The socket is set to non-blocking mode to prevent blocking the main thread during network operations.
         * @note No need a param because the port for a client is always 0
         * @return true if the socket was successfully created and bound, false otherwise.
         */
        bool bind_port();

        // Send a packet to a specific IP/Port
        void send_packet(int type, PackedByteArray data);

        // Check for incoming packets (Call this in _process)
        bool poll();

        PackedByteArray serialize_snapshot();
    };
}