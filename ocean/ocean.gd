extends Node3D
## Drop-in ocean: keeps the water's clipmap mesh centred under the active camera
## so the sea looks endless. (Snapping logic adapted from main.gd in
## 2Retr0/GodotOceanWaves.)
##
## The waves are visual only. For gameplay, treat the water surface as y = 0.

## Snap step in metres. Whole metres keep the waves from jittering as you move.
const TILE_SIZE := 1.0

var _previous_tile := Vector3.INF

@onready var water: MeshInstance3D = $Water


func _physics_process(_delta: float) -> void:
	var camera := get_viewport().get_camera_3d()
	if camera == null:
		return
	var tile := (Vector3(camera.global_position.x, 0.0, camera.global_position.z) / TILE_SIZE).ceil()
	if not tile.is_equal_approx(_previous_tile):
		water.global_position = tile * TILE_SIZE
		_previous_tile = tile
