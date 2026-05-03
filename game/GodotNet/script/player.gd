extends CharacterBody2D


const SPEED = 300
@onready var Sprite = $AnimatedSprite2D
@onready var camera_2d: Camera2D = $Camera2D
var bIsLocalPlayer = false

func _ready() -> void:
	if not bIsLocalPlayer:
		camera_2d.enabled = false

#func _physics_process(delta: float) -> void:
	#if(bIsLocalPlayer):
		#print("Local Player = X : " + str(position.x) + " Y : " + str(position.y))

func MoveHandle (direction: Vector2) -> void:
	RotationHandle(direction)
	AnimationHandle(direction)
	
	if direction.x:
		velocity.x = direction.x * SPEED
	else:
		velocity.x = move_toward(velocity.x, 0, SPEED)
		
	if direction.y:
		velocity.y = direction.y * SPEED
	else:
		velocity.y = move_toward(velocity.y, 0, SPEED)
	
	move_and_slide()

func RotationHandle (direction: Vector2) -> void:
	if direction.x > 0 :
		Sprite.flip_h = false
	elif direction.x < 0:
		Sprite.flip_h = true
		
	#if direction.y > 0 :
		
	#elif direction.y < 0:

func AnimationHandle (direction: Vector2) -> void:
	if direction.x == 0 and direction.y == 0 :
		Sprite.play("Idle")
	else :
		Sprite.play("Walk")

func SetAsLocalPlayer () -> void:
	bIsLocalPlayer = true
	
func getPosition() -> Vector2:
	return position
