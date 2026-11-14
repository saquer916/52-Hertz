extends Node

@export var max_breath := 100.0;
@export var drain_per_second := 2.0;
@export var refill_per_second := 30.0;
@export var bar: ProgressBar;

var breath := 100.0;
var lastBreathPosition;

@onready var whale: Whale = get_parent();

func _ready() -> void:
	lastBreathPosition = whale.global_position;

func _process(delta: float) -> void:
	var at_surface := whale.global_position.y;
	if (at_surface > -3.0):
		breath += refill_per_second*delta;
		lastBreathPosition = whale.global_position;
	else:
		breath -= drain_per_second*delta;
	breath = clampf(breath, 0, max_breath);
	bar.value = breath;
	if (breath == 0):
		out_of_air();
		
func out_of_air() -> void:
	whale.position = lastBreathPosition;
	breath = max_breath;
	
