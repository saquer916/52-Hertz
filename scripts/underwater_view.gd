extends Node
## Underwater look for the whale's camera (put this node under the Camera3D).
## The deeper the camera, the darker the water and the less you can see.
## Near the surface it's blue and you can see far; above the water the level's
## normal sky and lighting come back. Echo dots ignore fog, so they always show.

@export var water_level := 0.0
## How deep (in metres) before the water is at its darkest.
@export var dark_depth := 25.0

@export_group("Near the surface")
@export var shallow_color := Color(0.05, 0.17, 0.22)
## How far you can see just under the surface, in metres.
@export var shallow_visibility := 140.0

@export_group("Deep")
@export var deep_color := Color(0.0, 0.0, 0.0)
## How far you can see at the darkest depth. Tiny = pitch black (the whale and echo dots ignore the fog).
@export var deep_visibility := 1.5
## Fraction of sunlight that still reaches the deepest water.
@export var deep_light := 0.0

var _below: Environment
var _sun: DirectionalLight3D
var _sun_energy := 1.0

@onready var camera: Camera3D = get_parent()


func _ready() -> void:
	# Start from the level's own environment so colours and glow match above water.
	var above := camera.get_world_3d().environment
	_below = above.duplicate() if above else Environment.new()
	_below.fog_enabled = true
	_below.fog_mode = Environment.FOG_MODE_DEPTH
	_below.fog_density = 1.0
	_below.fog_depth_curve = 1.0
	_below.fog_sky_affect = 1.0
	_below.fog_aerial_perspective = 0.0
	_below.fog_sun_scatter = 0.0
	_below.volumetric_fog_enabled = false # The sea-mist volume is for above the water.

	var suns := get_tree().root.find_children("*", "DirectionalLight3D", true, false)
	if not suns.is_empty():
		_sun = suns[0]
		_sun_energy = _sun.light_energy


func _process(_delta: float) -> void:
	var depth := water_level - camera.global_position.y
	# The surface can't be seen from below, so pause and hide the ocean (and its
	# spray) while underwater. That's a big speed-up on laptop graphics.
	_set_ocean_active(depth <= 0.0)
	if depth <= 0.0:
		camera.environment = null # Above water: use the level's normal look.
		if _sun:
			_sun.light_energy = _sun_energy
			_sun.shadow_enabled = true
		return

	var t := clampf(depth / dark_depth, 0.0, 1.0)
	if _sun:
		_sun.shadow_enabled = t < 0.3 # Shadows aren't visible in dark water.
	# Visibility shrinks fast at first, like light fading in real water.
	var visibility := shallow_visibility * pow(deep_visibility / shallow_visibility, t)
	_below.fog_light_color = shallow_color.lerp(deep_color, t)
	# Near the surface the haze starts right away, so it looks like water.
	_below.fog_depth_begin = visibility * 0.4 * t
	_below.fog_depth_end = visibility
	_below.ambient_light_energy = lerpf(1.0, deep_light, t)
	camera.environment = _below
	if _sun:
		_sun.light_energy = _sun_energy * lerpf(1.0, deep_light, t)


func _set_ocean_active(active: bool) -> void:
	for ocean: Node3D in get_tree().get_nodes_in_group("ocean"):
		if ocean.visible != active:
			ocean.visible = active
			ocean.process_mode = Node.PROCESS_MODE_INHERIT if active else Node.PROCESS_MODE_DISABLED
