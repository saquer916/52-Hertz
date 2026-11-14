# Ocean (third-party)

FFT ocean waves from [2Retr0/GodotOceanWaves](https://github.com/2Retr0/GodotOceanWaves)
at commit `a171446` (2024-09-27), MIT licensed. See `LICENSE` in this folder.
Its FFT stage is adapted from [OTFFT](http://wwwa.pikara.ne.jp/okojisan/otfft-en/stockham3.html)
by Takuya Okahisa (MIT).

Changes for 52 Hertz:
- Files moved from `assets/water/` and `assets/shaders/` into `ocean/`, with paths updated.
- The ImGui debug panel and its addon are not included.
- `ocean.gd` / `ocean.tscn` are new: a drop-in scene that keeps the water mesh
  under the active camera (logic taken from upstream `main.gd`).
- Fixed for Godot 4.7: exact-size push constants (`render_context.gd`) and a
  non-`readonly` spectrum image (`spectrum_modulate.glsl`).
- `water.gdshader` has impact ripples added: `ocean.gd`'s `add_impact()` makes
  the surface heave and send rings outward (used when the whale breaks the surface).

Requirements: the Forward+ renderer (it uses compute shaders) and the
`[shader_globals]` entries in `project.godot`.
