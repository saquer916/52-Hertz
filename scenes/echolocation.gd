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
			spawn_dot(hit.position, pendingReach);
	print("hits: ", hits, " / ", pendingRays);
	pendingRays = 0;
	
var dotMesh : SphereMesh;

func _ready() -> void:
	dotMesh = SphereMesh.new();
	dotMesh.radius = 0.15;
	dotMesh.height = 0.3;
	dotMesh.radial_segments = 6;
	dotMesh.rings = 3;
	var mat := StandardMaterial3D.new();
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED;
	mat.albedo_color = Color(0.6, 0.9, 1.0);
	dotMesh.material = mat;
	
func spawn_dot(where: Vector3, reach: float) -> void:
	var dot := MeshInstance3D.new();
	dot.mesh = dotMesh;
	get_tree().current_scene.add_child(dot);
	dot.global_position = where;
	var distance := where.distance_to(global_position);
	dot.transparency = clampf(distance / reach, 0.0, 0.85);
	
	var tw := dot.create_tween();
	tw.tween_property(dot, "transparency", 1.0, 5.0);
	tw.tween_callback(dot.queue_free);
	
