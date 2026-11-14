extends Node3D
## Procedural swimming for the whale model (put this on the whale's Model node).
## - A wave runs down the spine to the flukes, up and down like a real whale's
##   tail stroke, beating faster and harder the faster the whale swims.
## - In a turn the body curves into the turn (head leading, tail following).
## - The flippers flap gently with each stroke and steer in turns: the inside
##   flipper dips and the outside one lifts.
## The default bone names and axes are for the Humpback Whale (Idle) model; change
## them in the Inspector if you swap in a different rigged whale.

## Spine bones from mid-body to the flukes.
@export var spine_bones: PackedStringArray = ["Tail_J1_01", "Tail_J2_02", "Tail_J3_03", "Tail_J4_04", "Tail_J5_05"]
## How far each spine bone bends up/down at full power, in degrees (same order as spine_bones).
@export var spine_degrees: PackedFloat32Array = [3.0, 5.0, 9.0, 13.0, 16.0]
## How far each spine bone curves sideways in a hard turn, in degrees.
@export var turn_degrees: PackedFloat32Array = [5.0, 6.0, 7.0, 8.0, 8.0]
@export var head_bone := "Head_J1_06"
## Right flipper first, then left.
@export var flipper_bones: PackedStringArray = ["R_Root_Fin_08", "L_Root_Fin_011"]
## Gentle flipper flap with each stroke, in degrees.
@export var flipper_flap := 5.0
## How far the flippers tilt to steer in a hard turn, in degrees.
@export var flipper_steer := 20.0

@export_group("Skeleton directions")
## The model's axes in skeleton space: the body's length, its up, and its side.
## (Humpback: length X, up Y, side Z.)
@export var length_axis := Vector3(1, 0, 0)
@export var up_axis := Vector3(0, 1, 0)
@export var side_axis := Vector3(0, 0, 1)
## Flip these if the body curves out of turns instead of into them, or the
## flippers steer the wrong way.
@export var turn_sign := 1.0
@export var flipper_sign := 1.0

@export_group("Rhythm")
## How far each bone lags behind the one before it, in radians. This makes the wave travel toward the tail.
@export var wave_lag := 0.55
## Tail beats per second when still, plus this much per m/s of speed.
@export var base_beats := 0.3
@export var beats_per_speed := 0.07
## How far the whole body rises and falls with each stroke, in metres.
@export var heave := 0.06
## Turning this fast (radians per second) counts as a hard turn.
@export var hard_turn_rate := 0.9

var _skeleton: Skeleton3D
var _spine: Array[int] = []
var _head := -1
var _flippers: Array[int] = []
var _rest := {} # bone index -> neutral rotation
var _phase := 0.0
var _power := 0.5
var _turn := 0.0 # -1 (hard right) .. 1 (hard left), smoothed

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

	# The model's own clip is stiff, so we drive the bones ourselves. But its
	# average pose is a better neutral body than the rest pose.
	for player: AnimationPlayer in find_children("*", "AnimationPlayer", true, false):
		_use_average_pose_as_rest(player)
		player.stop()
		player.active = false


func _process(delta: float) -> void:
	# Stronger strokes at higher speed; a gentle glide when slow.
	var target_power := clampf(whale.speed / whale.boost_speed, 0.25, 1.0)
	_power = lerpf(_power, target_power, 1.0 - exp(-2.0 * delta))
	var target_turn := clampf(whale.turn_rate / hard_turn_rate, -1.0, 1.0) * turn_sign
	_turn = lerpf(_turn, target_turn, 1.0 - exp(-4.0 * delta))
	var beats := base_beats + beats_per_speed * whale.speed
	_phase = fmod(_phase + TAU * beats * delta, TAU)

	for i in _spine.size():
		var stroke := deg_to_rad(spine_degrees[i]) * _power * sin(_phase - wave_lag * i)
		# Spine bones point toward the tail, so curving the body into a turn means
		# rotating them the opposite way to the turn.
		var curve := -deg_to_rad(turn_degrees[i]) * _turn
		_pose(_spine[i], stroke, curve)
	# The head counters the stroke a little (so it stays steady) and leads into turns.
	_pose(_head, -deg_to_rad(2.0) * _power * sin(_phase + 0.6), deg_to_rad(10.0) * _turn)

	# Flippers: a mirrored flap with the stroke, plus opposite tilts to steer.
	var flap := deg_to_rad(flipper_flap) * _power * sin(_phase - 0.8)
	var steer := deg_to_rad(flipper_steer) * _turn * flipper_sign
	for i in _flippers.size():
		var mirror := 1.0 if i == 0 else -1.0
		_roll(_flippers[i], flap * mirror + steer)

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


## Turns a skeleton-space direction into this bone's own local direction.
func _local_axis(bone: int, skeleton_axis: Vector3) -> Vector3:
	return (_skeleton.get_bone_global_rest(bone).basis.inverse() * skeleton_axis).normalized()


## Bends a bone up/down (around the side axis) and sideways (around the up axis).
func _pose(bone: int, up_down: float, sideways: float) -> void:
	if bone < 0:
		return
	var q := _rest[bone] as Quaternion
	q *= Quaternion(_local_axis(bone, side_axis), up_down)
	q *= Quaternion(_local_axis(bone, up_axis), sideways)
	_skeleton.set_bone_pose_rotation(bone, q)


## Rolls a bone around the body's length axis (raises/lowers a flipper).
func _roll(bone: int, angle: float) -> void:
	if bone < 0:
		return
	var q := _rest[bone] as Quaternion
	_skeleton.set_bone_pose_rotation(bone, q * Quaternion(_local_axis(bone, length_axis), angle))
