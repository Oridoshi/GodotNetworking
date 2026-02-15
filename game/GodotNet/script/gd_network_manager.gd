extends GDNetworkManager

const PORT = 5252
const IP_TO_SEND = "127.0.0.1"
const MESSAGE = "HELLO WORLD!"

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	bind_port(PORT)
	
	var data = PackedByteArray(MESSAGE.to_ascii_buffer())
	send_packet(IP_TO_SEND, PORT, data)
