# OpenMW05

An open-source, from-scratch engine client for **Need for Speed: Most Wanted
(2005, PC)** that loads and renders assets from an existing, user-owned
installation of the game.

This repository contains **no EA assets and no EA code**. You must own the
game; the engine reads its files at runtime from a directory you point it at
(`--gamedir <path>` or env `OMW05_GAMEDIR`).

- Language: **C++17** (strictly — no C++20/23)
- Graphics: **OpenGL ES 3.0** (works on desktop GL drivers exposing GLES3;
  ARM/embedded Linux is a first-class target)
- Windowing/input: **SDL2**
- Debug UI: **Dear ImGui** (optional, `OMW05_WITH_IMGUI`)

## Status

| Milestone | Description | Status |
|---|---|---|
| M0 | Skeleton: window, ES 3.0 context, offscreen RTs + compositor (`--rotate`, `--render-scale`), ImGui, logging | ✅ |
| M1 | Chunk layer: ChunkReader, chunk IDs, RefPack + JDLZ, hashes, `mw-chunkdump` | ✅ |
| M2 | Textures: texture pack parser, CPU DXT decode, `mw-texdump`, texture browser | ✅ |
| M3 | Solids: geometry parser, `mw-solid2gltf`, in-engine model viewer | ✅ |
| M4 | World: track streaming, free camera over Rockport | ✅ (culling: TODO) |
| M5 | Data layer: VLT (`mw-vltdump`), language packs, FNG/collision/NIS stubs | ⬜ next |

## Building

### Linux (Debian/Ubuntu)

```sh
sudo apt install cmake g++ libsdl2-dev libgles-dev
./scripts/build.sh            # configures + builds into build/
./build/bin/omw05-viewer --help
```

### Windows

- **MSYS2 (MinGW64):** `pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-SDL2`, then the usual CMake configure/build. An EGL/GLES3 implementation is required at runtime (ANGLE ships `libEGL.dll`/`libGLESv2.dll`).
- **MSVC:** use CMake presets/GUI with SDL2 from vcpkg (`vcpkg install sdl2 angle`).

### ARM / embedded Linux

The renderer is written against the GLES 3.0 API surface only and the
compositor supports presentation rotation (`--rotate 0|90|180|270`) and a 3D
render-scale factor (`--render-scale 0.5..1.0`) specifically for
Mali/VideoCore-class devices. Build as on any Linux; SDL2 with KMS/DRM or
Wayland works.

## Running

```sh
omw05-viewer --gamedir /path/to/NFS-MostWanted [--rotate 90] [--render-scale 0.75]
```

The game directory is validated (expects `TRACKS/`, `GLOBAL/`, `CARS/`,
`FRONTEND/`).

## Contributing / community

Format knowledge consolidates decades of community reverse engineering —
see `CREDITS.md` and `THIRD_PARTY.md`, and the format notes under
`docs/formats/`. Human contributors can find the community at the
"Classic Need for Speed" Discord.

## License

Engine code: see `LICENSE` (to be added). Borrowed/ported code is tracked in
`THIRD_PARTY.md`.
