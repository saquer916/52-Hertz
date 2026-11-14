extends Node3D

var holdTime := 0.0

func _process(delta: float) -> void:
	if (Input.is_action_just_pressed("echo")):
		holdTime = 0
	if (Input.is_action_pressed("echo")):
		holdTime += delta
	if (Input.is_action_just_released("echo")):
		var strength = clampf(holdTime/1.2, 0, 1)
		var reach = lerpf(30, 150, strength)
		var rayCount = int(lerpf(200, 1000, strength))
		print("echo! ", strength, " ", reach, " ", rayCount)
	
