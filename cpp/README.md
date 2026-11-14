# 52 Hertz (C++ / OpenGL)

The C++ version of 52 Hertz, made at Hack Club Haven Boston (theme: Bounce Back).
The Godot project at the repo root was the first prototype.

## Who wrote what

- **Gameplay** ([`src/gameplay.cpp`](src/gameplay.cpp)): echolocation, breathing and
  bounce-back, breaching, and winning. Written by Vihan.
- **Engine** (everything else in `src/`: rendering, ocean, whale model and swimming,
  maze, effects, HUD): adapted from an AI-generated (Claude) prototype.

## Build and run

Needs CMake, a C++20 compiler and OpenGL 4.5. GLFW and glm are used from the system
if installed (Arch: `pacman -S glfw glm`), otherwise CMake downloads them.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/52hertz
```

### macOS

macOS only has OpenGL 4.1, so the Mac build uses a 4.1 code path (`HERTZ_GL41`; build with
`-DGL41=ON` to try it on Linux or Windows). GitHub Actions builds it: see
`.github/workflows/build-macos.yml`. The app isn't signed, so the first time, **right-click the
`52hertz` file > Open**, or run `xattr -dr com.apple.quarantine` on the unzipped folder.

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

## Credits

- Humpback whale model: ["Humpback Whale (Idle)"](https://sketchfab.com/3d-models/humpback-whale-idle-bd64878acb0844b2be796ee91855c69e)
  by [Connlan_Immure](https://sketchfab.com/Connlan_Immure), [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/)
  (shared with the Godot version in `../assets/models/humpback_whale`).
- Libraries: [GLFW](https://www.glfw.org/) (zlib), [glm](https://github.com/g-truc/glm) (MIT),
  [cgltf](https://github.com/jkuhlmann/cgltf) (MIT), [stb](https://github.com/nothings/stb)
  (MIT / public domain), Khronos OpenGL headers (MIT/Apache-2.0).
