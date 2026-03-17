extends GDNetworkManager
@export var PlayerManager : Node;
@export var PLAYER_SCENE: PackedScene;

enum PacketType {
	LOGIN      = 0,
	INPUT      = 1,
	NEW_PLAYER = 2,
	LOGOUT     = 3
}

func sendToServer(type: PacketType, data: PackedByteArray):
	send_packet(type, data);

func register_node () -> Node:
	var newPlayer = PLAYER_SCENE.instantiate()
	PlayerManager.add_child(newPlayer)
	
	return newPlayer
