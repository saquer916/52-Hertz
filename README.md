# 52 Hertz

You are a whale whose calls no other whale answers. Alone in a dark sea maze,
you can only see what your echoes bounce back from. Surface to breathe, call
into the dark, and find your way to a pod that will finally call back.

Made in one day at **Hack Club Haven Boston** (November 14, 2026). Theme: **Bounce Back**.

**Play it on itch.io: [saquer916.itch.io/hertz-52](https://saquer916.itch.io/hertz-52)**
(in the browser, or download for Windows, macOS and Linux).

The name comes from a real whale that sings at 52 Hz, a pitch no other whale uses.

## How to play

- **Echolocate.** Tap Space for a short click in every direction. Hold it for a long call:
  a focused beam that reaches much farther. Your sound bounces back as dots on the cliffs.
- **Breathe.** A breath lasts about 50 seconds, and boosting uses it up faster. Every call
  costs air too, and longer calls cost more. Surface or breach to refill.
- **Bounce back.** Run out of air and you wake up back where you last breathed.
- **Stay deep.** Rough waves slow you down at the surface, so you can't just swim over the maze.
- **Find the pod** past the maze's exit, deep below. Call out when you're close and they answer.

## Controls

| Input | Action |
| --- | --- |
| Mouse / trackpad | Steer |
| W / Shift | Swim faster |
| S | Slow down |
| Space (tap) | Short click in every direction |
| Space (hold) | Long call: a focused beam ahead |
| R | New maze |
| Esc | Free the mouse (click to grab it again) |
| F11 | Fullscreen |

## Build from source

The game is in [`cpp/`](cpp/): C++20 and OpenGL, with a WebGL 2 build for the browser.

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build
./cpp/build/52hertz
```

See [`cpp/README.md`](cpp/README.md) for macOS, the browser build and packaging for itch.io.

## Credits

- Humpback whale model: based on ["Humpback Whale (Idle)"](https://sketchfab.com/3d-models/humpback-whale-idle-bd64878acb0844b2be796ee91855c69e) by [Connlan_Immure](https://sketchfab.com/Connlan_Immure), licensed under [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/).
- Libraries (C++ version): [GLFW](https://www.glfw.org/) (zlib), [glm](https://github.com/g-truc/glm) (MIT), [cgltf](https://github.com/jkuhlmann/cgltf) (MIT), [stb](https://github.com/nothings/stb) (MIT / public domain).
- Used by the Godot version in this repo:
  - Ocean simulation: [GodotOceanWaves](https://github.com/2Retr0/GodotOceanWaves) by 2Retr0 (MIT). See [`ocean/README.md`](ocean/README.md).
  - Sky: [Evening Road 01 (Pure Sky)](https://polyhaven.com/a/evening_road_01_puresky) by Jarod Guest, Poly Haven (CC0).
  - Orca model: based on ["Killer Whale"](https://sketchfab.com/3d-models/killer-whale-63b680d7e58f463a9868ed7bf163094a) by [Abner Wu](https://sketchfab.com/dashdu), licensed under [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/).

## License

Code is MIT (see [`LICENSE`](LICENSE)). Third-party code and assets keep their own licenses, listed above.
