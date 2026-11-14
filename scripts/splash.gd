extends Node3D
## A one-shot splash (spray + a ring of foam). Add it to the scene, call play(),
## and it removes itself when finished.

## strength: about 0.3 for a gentle surfacing, 1+ for a big breach or belly flop.
func play(strength: float) -> void:
	strength = clampf(strength, 0.25, 1.5)
	scale = Vector3.ONE * lerpf(0.6, 1.4, strength / 1.5)
	for particles: GPUParticles3D in [$Spray, $Foam]:
		particles.amount_ratio = clampf(strength, 0.3, 1.0)
		particles.restart()
	await get_tree().create_timer(4.0).timeout
	queue_free()
