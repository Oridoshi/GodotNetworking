extends Node2D
@export var NET_MANAGER: GDNetworkManager;
@export var PLAYER_SCENE: PackedScene;
@onready var player: CharacterBody2D;

func _ready() -> void:
	spawnPlayer()
	NET_MANAGER.sendToServer(NET_MANAGER.PacketType.LOGIN, InitalLocationData())

func spawnPlayer() -> void:
	var scene_root = get_tree().current_scene
	
	player = PLAYER_SCENE.instantiate()
	player.SetAsLocalPlayer()
	
	var point_de_spawn = self.global_position
	player.position = point_de_spawn
	
	scene_root.call_deferred("add_child", player)

func _physics_process(delta: float) -> void:
	if player == null or not player.is_inside_tree():
		return

	var direction : Vector2
	direction.x = Input.get_axis("MoveLeft", "MoveRight")
	direction.y = Input.get_axis("MoveUp", "MoveDown")
	
	player.MoveHandle(direction)
	DataToServerHanlde(direction)

func InitalLocationData() -> PackedByteArray:
	var spb = StreamPeerBuffer.new()
	
	# Il gère le curseur et la taille tout seul
	spb.put_u32(position.x)
	spb.put_u32(position.y)
	
	return spb.data_array

func DataToServerHanlde (direction: Vector2) -> void:
	var bNeedToSend = false
	if direction.x > 0 or direction.x < 0 or direction.y > 0 or direction.y < 0 :
		bNeedToSend = true
	
	if(bNeedToSend):
		var mouse_pos = get_global_mouse_position()
		NET_MANAGER.send_input(direction.y < 0, direction.y > 0, direction.x < 0, direction.x > 0, mouse_pos.x, mouse_pos.y)
