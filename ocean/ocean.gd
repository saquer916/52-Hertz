extends Node3D
## Drop-in ocean: keeps the water's clipmap mesh centred under the active camera
## so the sea looks endless. (Snapping logic adapted from main.gd in
## 2Retr0/GodotOceanWaves.)
##
## The waves are visual only. For gameplay, treat the water surface as y = 0.
## Call add_impact() to make the surface heave and ripple outward from a point.

## Snap step in metres. Whole metres keep the waves from jittering as you move.
const TILE_SIZE := 1.0
## Must match MAX_IMPACTS in water.gdshader.
const MAX_IMPACTS := 8

var _previous_tile := Vector3.INF
var _impacts := PackedVector4Array()
var _next_impact := 0
var _clock := 0.0

@onready var water: MeshInstance3D = $Water


func _ready() -> void:
	_impacts.resize(MAX_IMPACTS)
	_impacts.fill(Vector4.ZERO)
	water.material_override.set_shader_parameter("impacts", _impacts)


## Makes the water heave and send rings outward from `where` (only x and z are
## used). strength: about 0.3 for a gentle surfacing, 1 or more for a big breach.
func add_impact(where: Vector3, strength: float) -> void:
	_impacts[_next_impact] = Vector4(where.x, where.z, _clock, strength)
	_next_impact = (_next_impact + 1) % MAX_IMPACTS
	water.material_override.set_shader_parameter("impacts", _impacts)


func _process(delta: float) -> void:
	_clock += delta
	water.material_override.set_shader_parameter("impact_time", _clock)


func _physics_process(_delta: float) -> void:
	var camera := get_viewport().get_camera_3d()
	if camera == null:
		return
	var tile := (Vector3(camera.global_position.x, 0.0, camera.global_position.z) / TILE_SIZE).ceil()
	if not tile.is_equal_approx(_previous_tile):
		water.global_position = tile * TILE_SIZE
		_previous_tile = tile
