class_name Whale
extends CharacterBody3D
## The player. Mouse/trackpad steers and the whale always swims forward.
## Underwater it moves freely in 3D. Above the surface (y > water_level) it is
## out of the water; see the TODO in _physics_process for breaching.

signal surfaced
signal dived

@export_group("Swimming")
@export var cruise_speed := 8.0
@export var boost_speed := 18.0
@export var slow_speed := 3.0
## How quickly the speed changes, in m/s per second.
@export var acceleration := 6.0

@export_group("Steering")
@export var mouse_sensitivity := 0.0025
## Higher = snappier turns, lower = heavier and more whale-like.
@export var turn_responsiveness := 3.0
@export_range(10, 85) var max_pitch_degrees := 70.0
## How far the body leans into turns.
@export var bank_amount := 0.35

## Height of the water surface. The ocean's waves are visual only.
@export var water_level := 0.0

var target_yaw := 0.0
var target_pitch := 0.0
var speed := 0.0
var is_underwater := true

@onready var model: Node3D = $Model


func _ready() -> void:
	target_yaw = rotation.y
	target_pitch = rotation.x
	speed = cruise_speed
	is_underwater = global_position.y < water_level
	Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	_stay_visible_in_the_dark()


## Deep water is pitch black, but the player still needs to see their whale.
## The whale ignores the fog, and goes on render layer 2 so the WhaleLight
## (which only lights layer 2) can light it without lighting the walls.
func _stay_visible_in_the_dark() -> void:
	for mesh: MeshInstance3D in model.find_children("*", "MeshInstance3D", true, false):
		mesh.layers = 2
		for i in mesh.get_surface_override_material_count():
			var material := mesh.get_active_material(i)
			if material is BaseMaterial3D:
				material = material.duplicate()
				material.disable_fog = true
				mesh.set_surface_override_material(i, material)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		target_yaw -= event.relative.x * mouse_sensitivity
		var max_pitch := deg_to_rad(max_pitch_degrees)
		target_pitch = clampf(target_pitch - event.relative.y * mouse_sensitivity, -max_pitch, max_pitch)
	elif event.is_action_pressed("release_mouse"):
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	elif event is InputEventMouseButton and event.pressed:
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	elif event.is_action_pressed("toggle_fullscreen"):
		var fullscreen := DisplayServer.window_get_mode() == DisplayServer.WINDOW_MODE_FULLSCREEN
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED if fullscreen else DisplayServer.WINDOW_MODE_FULLSCREEN)


func _physics_process(delta: float) -> void:
	var target_speed := cruise_speed
	if Input.is_action_pressed("boost"):
		target_speed = boost_speed
	elif Input.is_action_pressed("slow"):
		target_speed = slow_speed
	speed = move_toward(speed, target_speed, acceleration * delta)

	# Ease the body toward where the mouse is pointing, so turns feel heavy.
	var t := 1.0 - exp(-turn_responsiveness * delta)
	var previous_yaw := rotation.y
	rotation.y = lerp_angle(rotation.y, target_yaw, t)
	rotation.x = lerp_angle(rotation.x, target_pitch, t)

	# Lean into turns (only the model, so the camera stays level).
	var turn_rate := angle_difference(previous_yaw, rotation.y) / delta
	model.rotation.z = lerpf(model.rotation.z, turn_rate * bank_amount, t)

	var was_underwater := is_underwater
	is_underwater = global_position.y < water_level
	if is_underwater and not was_underwater:
		dived.emit()
	elif was_underwater and not is_underwater:
		surfaced.emit()

	if is_underwater:
		velocity = -global_basis.z * speed
	else:
		# TODO (your breaching step): out of the water the whale should fly in an
		# arc and fall back in. Keep velocity.x and velocity.z as they are, and pull
		# velocity.y down with gravity each frame, e.g. velocity.y -= 9.8 * delta.
		# Until then it keeps swimming straight, so it can "fly".
		velocity = -global_basis.z * speed

	move_and_slide()
