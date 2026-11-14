extends Camera3D
## Follows the whale (its parent) from behind and slightly above, smoothing out
## its motion. Pulls in closer when a wall gets between the camera and the whale.

@export var distance := 16.0
@export var height := 4.0
## The camera aims at a point this far ahead of the whale.
@export var look_ahead := 8.0
## Higher = the camera catches up faster.
@export var follow_speed := 4.0
@export_flags_3d_physics var wall_mask := 1

@onready var whale: Node3D = get_parent()

var _ray := PhysicsRayQueryParameters3D.new()


func _ready() -> void:
	top_level = true # Move on our own; we follow the whale ourselves.
	_ray.collision_mask = wall_mask
	global_position = _desired_position()
	look_at(_aim_point(), Vector3.UP)
	reset_physics_interpolation()


func _physics_process(delta: float) -> void:
	var desired := _desired_position()

	_ray.from = whale.global_position
	_ray.to = desired
	var hit := get_world_3d().direct_space_state.intersect_ray(_ray)
	if hit:
		desired = hit.position + hit.normal * 0.5

	global_position = global_position.lerp(desired, 1.0 - exp(-follow_speed * delta))
	look_at(_aim_point(), Vector3.UP)


func _desired_position() -> Vector3:
	# The whale faces -Z, so "behind" is +Z in its local space.
	return whale.global_transform * Vector3(0.0, height, distance)


func _aim_point() -> Vector3:
	return whale.global_position - whale.global_basis.z * look_ahead
