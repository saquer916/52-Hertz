extends Node3D

var holdTime := 0.0;
var pendingRays := 0;
var pendingReach := 0.0;
var pendingCone := 180.0;  # half-angle of the beam in degrees (180 = every direction)


func _process(delta: float) -> void:
	if (Input.is_action_just_pressed("echo")):
		holdTime = 0
	if (Input.is_action_pressed("echo")):
		holdTime += delta
	if (Input.is_action_just_released("echo")):
		var strength = clampf(holdTime/1.2, 0, 1)
		var reach = lerpf(30, 160, strength)
		var rayCount = int(lerpf(250, 1200, strength))
		# A tap clicks in every direction; a long call is a narrow beam straight ahead,
		# so its rays stay packed together even on far-away walls.
		var cone = lerpf(180, 30, strength)
		print("echo! ", strength, " ", reach, " ", rayCount, " ", cone)
		pendingRays = rayCount;
		pendingReach = reach;
		pendingCone = cone;
	# Dots get lighter the closer the whale is, so keep the shader told where we are.
	dotMesh.material.set_shader_parameter("whale_position", global_position);

func _physics_process(delta: float) -> void:
	if (pendingRays == 0):
		return;
	var space := get_world_3d().direct_space_state;
	var hits := 0;
	for i in pendingRays:
		var direction = random_direction_in_cone(pendingCone);
		var query := PhysicsRayQueryParameters3D.create(global_position, global_position + direction * pendingReach);
		query.exclude = [get_parent().get_rid()];
		var hit := space.intersect_ray(query);
		if hit:
			hits += 1;
			spawn_dot(hit.position, pendingReach);
	print("hits: ", hits, " / ", pendingRays);
	pendingRays = 0;

# A random direction within `half_angle` degrees of where the whale is facing,
# spread evenly over the cone (180 degrees = anywhere on a full sphere).
func random_direction_in_cone(half_angle: float) -> Vector3:
	var cos_angle := lerpf(1.0, cos(deg_to_rad(half_angle)), randf());
	var sin_angle := sqrt(1.0 - cos_angle * cos_angle);
	var spin := randf() * TAU;
	var local := Vector3(sin_angle * cos(spin), sin_angle * sin(spin), -cos_angle);  # -Z is forward
	return global_basis * local;

var dotMesh : SphereMesh;

func _ready() -> void:
	dotMesh = SphereMesh.new();
	dotMesh.radius = 0.15;
	dotMesh.height = 0.3;
	dotMesh.radial_segments = 6;
	dotMesh.rings = 3;
	var mat := ShaderMaterial.new();
	mat.shader = preload("res://shaders/echo_dot.gdshader");
	dotMesh.material = mat;

func spawn_dot(where: Vector3, reach: float) -> void:
	var dot := MeshInstance3D.new();
	dot.mesh = dotMesh;
	dot.transparency = 1.0;  # start invisible, then fade in
	get_tree().current_scene.add_child(dot);
	dot.global_position = where;
	var distance := where.distance_to(global_position);

	var tw := dot.create_tween();
	tw.tween_interval(distance / 60.0);  # far echoes take longer to come back
	tw.tween_property(dot, "transparency", 0.0, 0.8);  # slow fade in
	tw.tween_property(dot, "transparency", 1.0, 5.0);  # then fade out
	tw.tween_callback(dot.queue_free);
	
