extends CharacterBody2D


const SPEED = 300
@onready var Sprite = $AnimatedSprite2D

#func _ready() -> void:

#func _physics_process(delta: float) -> void:

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
