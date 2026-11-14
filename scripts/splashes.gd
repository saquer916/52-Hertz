extends Node
## Makes a splash wherever the whale (this node's parent) breaks the surface,
## going up or coming back down. Faster through the surface = bigger splash.

const SPLASH := preload("res://scenes/splash.tscn")

## Slower than this (vertical m/s) and the whale just slips through quietly.
@export var min_speed := 1.5
## Seconds before another splash can happen (stops spam while skimming the surface).
@export var cooldown := 0.6

var _ready_at := 0.0

@onready var whale: Whale = get_parent()


func _ready() -> void:
	whale.surfaced.connect(_splash)
	whale.dived.connect(_splash)


func _splash() -> void:
	var now := Time.get_ticks_msec() / 1000.0
	var vertical_speed := absf(whale.velocity.y)
	if now < _ready_at or vertical_speed < min_speed:
		return
	_ready_at = now + cooldown
	var splash := SPLASH.instantiate()
	get_tree().current_scene.add_child(splash)
	splash.global_position = Vector3(whale.global_position.x, whale.water_level, whale.global_position.z)
	splash.play(vertical_speed / 10.0)
