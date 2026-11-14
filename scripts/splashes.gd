extends Node
## When the whale (this node's parent) breaks the surface, going up or coming
## back down, a splash of spray bursts out at that spot. Faster through the
## surface = bigger splash.

const SPLASH := preload("res://scenes/splash.tscn")

## Slower than this (vertical m/s) and the whale slips through without a splash.
@export var min_speed := 1.5
## Seconds before another splash can happen (stops spam while skimming the surface).
@export var cooldown := 0.6

var _ready_at := 0.0

@onready var whale: Whale = get_parent()


func _ready() -> void:
	whale.surfaced.connect(_on_cross_surface)
	whale.dived.connect(_on_cross_surface)


func _on_cross_surface() -> void:
	var now := Time.get_ticks_msec() / 1000.0
	var vertical_speed := absf(whale.velocity.y)
	if now < _ready_at or vertical_speed < min_speed:
		return
	_ready_at = now + cooldown
	var strength := clampf(vertical_speed / 10.0, 0.3, 1.5)
	var where := Vector3(whale.global_position.x, whale.water_level, whale.global_position.z)
	var splash := SPLASH.instantiate()
	get_tree().current_scene.add_child(splash)
	splash.global_position = where
	splash.play(strength)
