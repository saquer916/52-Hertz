extends Node3D

var holdTime := 0.0;
var pendingRays := 0;
var pendingReach := 0.0;


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
		pendingRays = rayCount;
		pendingReach = reach;
	
func _physics_process(delta: float) -> void:
	if (pendingRays == 0):
		return;
	var space := get_world_3d().direct_space_state;
	var hits := 0;
	for i in pendingRays:
		var direction = Vector3(randf_range(-1,1), randf_range(-1, 1), randf_range(-1, 1)).normalized();
		var query := PhysicsRayQueryParameters3D.create(global_position, global_position + direction * pendingReach);
		query.exclude = [get_parent().get_rid()];
		var hit := space.intersect_ray(query);
		if hit:
			hits += 1;
	print("hits: ", hits, " / ", pendingRays);
	pendingRays = 0;
	
