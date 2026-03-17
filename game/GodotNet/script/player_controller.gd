extends Node2D
@export var NET_MANAGER: GDNetworkManager;
@onready var player: CharacterBody2D = $Player

func _ready() -> void:
	NET_MANAGER.sendToServer(NET_MANAGER.PacketType.LOGIN, InitalLocationData())

func _physics_process(delta: float) -> void:
	var direction : Vector2
	direction.x = Input.get_axis("MoveLeft", "MoveRight")
	direction.y = Input.get_axis("MoveUp", "MoveDown")
	
	player.MoveHandle(direction)
	DataToServerHanlde(direction)

func InitalLocationData() -> PackedByteArray:
	var spb = StreamPeerBuffer.new()
	
	# Il gère le curseur et la taille tout seul
	spb.put_u32(player.position.x)
	spb.put_u32(player.position.y)
	
	return spb.data_array

func DataToServerHanlde (direction: Vector2) -> void:
	var bNeedToSend = false
	if direction.x > 0 or direction.x < 0 or direction.y > 0 or direction.y < 0 :
		bNeedToSend = true
	
	if(bNeedToSend):
		var mouse_pos = get_global_mouse_position()
		NET_MANAGER.send_input(direction.y < 0, direction.y > 0, direction.x < 0, direction.x > 0, mouse_pos.x, mouse_pos.y)
