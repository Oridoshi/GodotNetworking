extends CharacterBody2D


const SPEED = 300
@onready var Sprite = $AnimatedSprite2D

func _physics_process(delta: float) -> void:

	# Get the input direction and handle the movement/deceleration.
	# As good practice, you should replace UI actions with dcustom gameplay actions.
	var direction : Vector2
	direction.x = Input.get_axis("MoveLeft", "MoveRight")
	direction.y = Input.get_axis("MoveUp", "MoveDown")
	
	AnimationHandle(direction)
	RotationHandle(direction)
	MoveHandle(direction)
	
	move_and_slide()

func MoveHandle (direction: Vector2) -> void:
	if direction.x:
		velocity.x = direction.x * SPEED
	else:
		velocity.x = move_toward(velocity.x, 0, SPEED)
		
	if direction.y:
		velocity.y = direction.y * SPEED
	else:
		velocity.y = move_toward(velocity.y, 0, SPEED)

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
