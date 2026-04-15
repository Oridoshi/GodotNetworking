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

func register_node(spawn_x: int, spawn_y: int) -> Node:
	var newPlayer = PLAYER_SCENE.instantiate()
	
	# ON DÉFINIT LA POSITION AVANT L'AJOUT À L'ARBRE
	# Comme ça, il n'apparaît jamais en 0,0 !
	newPlayer.position = Vector2(spawn_x, spawn_y)
	
	PlayerManager.add_child(newPlayer)
	
	return newPlayer
