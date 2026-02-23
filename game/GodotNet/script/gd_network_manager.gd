extends GDNetworkManager
@onready var PlayerManager = $"../Game/Map/PlayerManager"
# On précharge la scène pour la performance
const PLAYER_SCENE = preload("res://GodotNet/player_dist.tscn")

enum PacketType {
	LOGIN   = 0,
	VECTOR  = 1,
	ROTATOR = 2,
	INT     = 3,
	STRING  = 4,
	LOGOUT  = 5
}

func sendToServer(type: PacketType, data: PackedByteArray):
	send_packet(type, data);

func register_node () -> Node:
	var newPlayer = PLAYER_SCENE.instantiate()
	PlayerManager.add_child(newPlayer)
	
	return newPlayer
