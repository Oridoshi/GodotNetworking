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

#include "../commun/protocol.hpp"

struct PredictionLocalLocation {
    int frame_id{}, x{},y{};
};

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

    struct RemotePlayer
    {
        Node* node = nullptr; // Node représentant le joueur distant dans la scène
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

        //String server_ip = "192.168.2.75";
        String server_ip = "127.0.0.1";

        int const server_port = 25555;

        // Dictionnaire pour stocker les joueurs distants connectés, avec leur ID et le node associé
        std::unordered_map <uint32_t, RemotePlayer> clientId_to_remotePlayer;

        // buffer de 20 input du joueur
        const int PACKET_SIZE = 13;
        std::vector<InputPacket> input_buffer;
        uint32_t next_sequence_id = 0;

        // Var pour le ping
        bool keep_pinging = true;
        int next_ping_id = 0;
        int ping_interval = 5; // en s

        //Var pour le WorldState
        std::vector<WorldStatePacket> world_state_buffer;
        float currentRanderServerFrameId = -1.0;
        const int SERVER_FPS = 60;
        const int WORLD_STATE_BUFFER_SIZE = 20;
        const int RENDER_DELAY = 2; // en secondes, pour compenser le délai de réception des paquets et lisser les mouvements

        //Var pour la correction de la position du joueur local
        int   current_frame_id = -1;
        float THRESHOLD_LOCAL_LOCATION = 5;
        float CORRECTION_RANGE = 20; // plus sa snap
        int   LOCAL_LOCATION_BUFFER_SIZE = 100;
        std::vector<PredictionLocalLocation> local_location_buffer;

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

        void ping_server();

        void send_ping();

        void update_player_location(int frameID, uint32_t client_id, int x, int y);

        void update_world_state(double delta);

    protected:
        static void _bind_methods();

        void correction_local_player(float frameID, int servX, int servY);

    public:
        GDNetworkManager();
        ~GDNetworkManager();

        void _process(double delta) override;

        void _physics_process(double delta);

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

        void send_input(bool up, bool down, bool left, bool right, float aim_x, float aim_y);

        void add_predict_pos(int new_x, int new_y);

        // Check for incoming packets (Call this in _process)
        bool poll();

        PackedByteArray serialize_snapshot();
    };
}