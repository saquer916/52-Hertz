extends Node3D
## Procedural swimming for the whale model (put this on the whale's Model node).
## A wave runs down the spine to the flukes, up and down like a real whale's
## tail stroke, and beats faster and harder the faster the whale swims.
## The default bone names are for the Killer Whale model; change them in the
## Inspector if you swap in a different rigged whale.

## Spine bones from mid-body to the flukes.
@export var spine_bones: PackedStringArray = ["joint8_03", "joint3_04", "joint14_05", "joint4_06", "joint5_07"]
## How far each spine bone bends at full power, in degrees (same order as spine_bones).
@export var spine_degrees: PackedFloat32Array = [3.0, 5.0, 9.0, 13.0, 16.0]
@export var head_bone := "joint1_010"
@export var flipper_bones: PackedStringArray = ["joint6_01", "joint9_08"]
## How far each bone lags behind the one before it, in radians. This makes the wave travel toward the tail.
@export var wave_lag := 0.55
## The bone-local axis that bends the body up and down.
@export var bend_axis := Vector3.RIGHT
## Tail beats per second when still, plus this much per m/s of speed.
@export var base_beats := 0.3
@export var beats_per_speed := 0.07
## How far the whole body rises and falls with each stroke, in metres.
@export var heave := 0.06

var _skeleton: Skeleton3D
var _spine: Array[int] = []
var _head := -1
var _flippers: Array[int] = []
var _rest := {} # bone index -> rest rotation
var _phase := 0.0
var _power := 0.5

@onready var whale: Whale = get_parent()


func _ready() -> void:
	var skeletons := find_children("*", "Skeleton3D", true, false)
	if skeletons.is_empty():
		set_process(false)
		return
	_skeleton = skeletons[0]
	for bone_name in spine_bones:
		_spine.append(_find_bone(bone_name))
	_head = _find_bone(head_bone)
	for bone_name in flipper_bones:
		_flippers.append(_find_bone(bone_name))

	# The model's own swim clip is stiff, so we drive the bones ourselves. But its
	# average pose is a better "straight body" than the rest pose (whose tail droops).
	for player: AnimationPlayer in find_children("*", "AnimationPlayer", true, false):
		_use_average_pose_as_rest(player)
		player.stop()
		player.active = false


func _process(delta: float) -> void:
	# Stronger strokes at higher speed; a gentle glide when slow.
	var target_power := clampf(whale.speed / whale.boost_speed, 0.25, 1.0)
	_power = lerpf(_power, target_power, 1.0 - exp(-2.0 * delta))
	var beats := base_beats + beats_per_speed * whale.speed
	_phase = fmod(_phase + TAU * beats * delta, TAU)

	for i in _spine.size():
		_bend(_spine[i], deg_to_rad(spine_degrees[i]) * _power * sin(_phase - wave_lag * i))
	# The head counters the stroke a little so it stays steady.
	_bend(_head, -deg_to_rad(2.0) * _power * sin(_phase + 0.6))
	for flipper in _flippers:
		_bend(flipper, deg_to_rad(4.0) * _power * sin(_phase - 0.8))

	position.y = heave * _power * sin(_phase + PI / 2.0)


func _find_bone(bone_name: String) -> int:
	var bone := _skeleton.find_bone(bone_name)
	if bone >= 0:
		_rest[bone] = _skeleton.get_bone_rest(bone).basis.get_rotation_quaternion()
	else:
		push_warning("swim_motion: no bone named '%s'" % bone_name)
	return bone


func _use_average_pose_as_rest(player: AnimationPlayer) -> void:
	var clips := Array(player.get_animation_list()).filter(func(n): return n != "RESET")
	if clips.is_empty():
		return
	const SAMPLES := 16
	var length := player.get_animation(clips[0]).length
	var sums := {}
	player.play(clips[0])
	for s in SAMPLES:
		player.seek(length * s / SAMPLES, true)
		for bone in _rest:
			var q := _skeleton.get_bone_pose_rotation(bone)
			var sum: Vector4 = sums.get(bone, Vector4.ZERO)
			# Keep every sample on the same side so they don't cancel out.
			if sum.dot(Vector4(q.x, q.y, q.z, q.w)) < 0.0:
				q = -q
			sums[bone] = sum + Vector4(q.x, q.y, q.z, q.w)
	for bone in sums:
		var v: Vector4 = sums[bone].normalized()
		_rest[bone] = Quaternion(v.x, v.y, v.z, v.w)


func _bend(bone: int, angle: float) -> void:
	if bone >= 0:
		_skeleton.set_bone_pose_rotation(bone, _rest[bone] * Quaternion(bend_axis, angle))
