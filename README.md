# 52 Hertz

You are a whale whose calls no other whale answers. Alone in a dark sea maze,
you can only see what your echoes bounce back from. Surface to breathe, call
into the dark, and find your way to a pod that will finally call back.

Made in one day at **Hack Club Haven Boston** (November 14, 2026). Theme: **Bounce Back**.

## Controls

| Input | Action |
| --- | --- |
| Mouse / trackpad | Steer |
| W / Shift | Swim faster |
| S | Slow down |
| Space (tap) | Short echolocation click |
| Space (hold) | Long echolocation call |
| Esc | Free the mouse (click to grab it again) |
| F11 | Fullscreen |

## Run from source

1. Install [Godot 4.7](https://godotengine.org/) (the standard build, not .NET).
2. Open `project.godot` and press F5.

The game uses the Forward+ renderer, so it needs a GPU with Vulkan support.

## Build for itch.io

```bash
tools/build.sh
```

This writes `build/52-Hertz-linux.zip` and `build/52-Hertz-windows.zip`.
It needs the Godot 4.7.2 export templates.

## Credits

- Ocean simulation: [GodotOceanWaves](https://github.com/2Retr0/GodotOceanWaves) by 2Retr0 (MIT). See [`ocean/README.md`](ocean/README.md).
- Sky: [Evening Road 01 (Pure Sky)](https://polyhaven.com/a/evening_road_01_puresky) by Jarod Guest, Poly Haven (CC0).

## License

Code is MIT (see [`LICENSE`](LICENSE)). Third-party code and assets keep their own licenses, listed above.
