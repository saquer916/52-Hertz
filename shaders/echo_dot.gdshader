shader_type spatial;
// Echo dots: dark gray far from the whale, lighter gray the closer it gets.
// Unshaded and fog-free so they still show up in the dark deep water.
// Fading in/out comes from the dot's `transparency` (set by echolocation.gd).
render_mode unshaded, fog_disabled, shadows_disabled;

uniform vec3 whale_position;
// At this distance or closer: lightest gray.
uniform float near_distance = 8.0;
// At this distance or farther: darkest gray.
uniform float far_distance = 90.0;
uniform float near_gray : hint_range(0.0, 1.0) = 0.92;
uniform float far_gray : hint_range(0.0, 1.0) = 0.2;

void fragment() {
	vec3 world_position = (INV_VIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;
	float t = smoothstep(near_distance, far_distance, distance(world_position, whale_position));
	ALBEDO = vec3(mix(near_gray, far_gray, t));
}
