# CLAUDE.md — OpenMW05 Engine Client

Agent instructions for building an open-source reimplementation client for
**Need for Speed: Most Wanted (2005, PC)** that loads and renders original
game assets. Read this file fully before doing anything.

---

## 1. Project Goal

Build, inside this repository, a from-scratch **C++17** client using
**SDL2 + OpenGL ES 3.0** that:

1. Opens an existing user-owned installation of NFS:MW 2005 (PC) and parses
   its resource files (chunked `.BUN`/`.BIN`/`.LZC` bundles, texture packs,
   solid geometry, track streaming sections, VLT database, string/language
   packs, FNG frontend packages).
2. Provides clean, reusable **header-declared classes** for every format:
   loaders, parsers, in-memory representations. Headers are the public API
   of the project — design them first, carefully.
3. Renders loaded content: start with a texture/model viewer, grow into a
   free-camera streaming world viewer of Rockport.
4. Consolidates existing community reverse-engineering knowledge. Do not
   re-reverse what is already documented — port, adapt, and credit.

This is an engine reimplementation, **not** a mod tool and **not** a decompilation.

## 2. Hard Constraints (never violate)

- **Language: C++17. Strictly. No C++20/23 features.** No concepts, no
  ranges, no `std::span`, no designated initializers, no `consteval`,
  no `char8_t`, no coroutines. CMake must set `CMAKE_CXX_STANDARD 17`,
  `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`.
  If a vendored library requires newer C++, isolate it or reject it.
- **Graphics: OpenGL ES 3.0 API surface only.** Even when running on
  desktop GL, restrict yourself to the GLES3 subset (use a loader such as
  glad generated for `gles2` API version 3.0, or ANGLE). No geometry
  shaders, no compute (that's ES 3.1), no desktop-only enums.
- **Windowing/input/audio: SDL2** (not SDL3). Request an ES 3.0 context via
  `SDL_GL_CONTEXT_PROFILE_ES`, major 3, minor 0.
- **No EA assets, no EA code, ever, in this repo.** No game files, no
  extracted textures/models committed as fixtures, no bytes of the original
  executable, no decompiled/disassembled listings pasted into sources or
  comments. The engine only *reads* files from a user-supplied game
  directory at runtime.
- **Clean-room discipline.** Community format documentation and open-source
  parsers are fine to study and port (respecting their licenses). Machine
  decompiler output of the EA binary is NOT an acceptable source to copy
  from. Structure layouts (offsets, sizes, field meanings) are facts and
  are fine; verbatim reproduced implementation code is not.
- **License hygiene.** Before vendoring or porting code from any repo,
  read its LICENSE. Record every borrowed piece in `THIRD_PARTY.md`
  (source URL, commit, license, what was taken). If a repo has NO license
  file, treat it as all-rights-reserved: use it only as *format
  documentation* (field names, offsets), re-implement code yourself, and
  credit the author in `CREDITS.md`.

## 3. Reference Material (consult in this order)

Study these before writing parsers. Prefer porting proven parsing logic
over guessing.

| Repo | What to take from it |
|---|---|
| `github.com/OpenNFS/OpenNFS` | Architecture reference for an original-asset-loading engine (older NFS titles). Loader layering, resource abstraction ideas. MIT-family — check current LICENSE. |
| `github.com/berkayylmao/NFSPluginSDK` | Reverse-engineered C++ struct layouts of Blackbox-era games incl. MW05. Use as authoritative field-layout documentation. Check license before including headers directly; otherwise re-declare needed structs. |
| `github.com/SpeedReflect/Binary` (a.k.a. MaxHwoy Binary) | The de-facto reference chunk (`.BIN/.BUN/.LZC`) reader/writer, C#. Port chunk IDs, container walking, and compression handling to C++. |
| `github.com/NFSTools/*` (org) | Arushan's original VLTEdit and MW Geometry Compiler sources (distributed with permission). VLT database format, MW solid/geometry format. |
| `github.com/heyitsleo` / `nfs-tools` orgs (NFS-ModTools, L5RA, etc.) | Geometry/texture/track chunk parsing for MW-era games, chunk ID tables, streaming section naming (`STREAML2RA_*`). |
| `github.com/dbalatoni13/nfsmw` | WIP matching decompilation (GC focus). Use ONLY as documentation of names/структур/algorithms semantics (e.g. bChunk IDs in their tables); do not copy matching-decomp source into this repo — provenance is the original binary. |
| `github.com/Ekey/NFS.BIN.Tool` | Console ZZDATA archives (secondary; PC MW mostly doesn't need it). |
| FFmpeg | VP6 video, EA ADPCM audio decoders exist there — later milestones. |

When information conflicts, trust (1) the GC/PS2 symbol-derived
documentation, (2) Binary's chunk tables, (3) blog/wiki writeups, in that
order. Community hub: "Classic Need for Speed" Discord — mention it in the
README for human contributors, you obviously can't join it.

## 4. Repository Layout

```
/CMakeLists.txt
/CLAUDE.md                  # this file
/README.md                  # user-facing: what works, how to build/run
/THIRD_PARTY.md             # provenance ledger (mandatory, keep current)
/CREDITS.md                 # RE community credits
/cmake/                     # toolchain helpers, FindXXX
/extern/                    # vendored deps ONLY (SDL2 via find_package preferred;
                            #   glad, stb, glm, catch2/doctest may be vendored)
/include/openmw05/          # PUBLIC headers — the API. One subdir per module:
    core/                   #   Span17.h (own span), Stream.h, Endian.h, Hash.h (bin/vlt hashes),
                            #   Log.h, Result.h
    io/                     #   ChunkReader.h, ChunkIds.h, Compression.h (JDLZ/RefPack/LZC), Vfs.h
    formats/                #   TexturePack.h, Solids.h, TrackStreamer.h, Vlt.h,
                            #   LangPack.h, Fng.h, Collision.h, Nis.h (stubs ok)
    gfx/                    #   GlContext.h, Texture.h, Mesh.h, Shader.h, Camera.h,
                            #   DxtDecode.h (CPU DXT1/3/5 -> RGBA8 fallback),
                            #   RenderTarget.h (FBO wrapper), Compositor.h (see §5a),
                            #   ImGuiLayer.h (Dear ImGui integration)
    app/                    #   Viewer.h, DebugUi.h
/src/                       # mirrors include/, .cpp implementations
/tools/                     # CLI utilities built from the same libs:
                            #   mw-chunkdump, mw-texdump, mw-solid2gltf, mw-vltdump
/tests/                     # unit tests on SYNTHETIC data only (hand-built chunk
                            #   buffers in code). Never commit real game data.
/docs/formats/              # your consolidated format notes in Markdown, one file
                            #   per format, with sources cited. This is a deliverable.
```

Rules:
- Every class is declared in a header under `include/openmw05/`, implemented
  in `src/`. Header-only is fine for small utilities.
- Namespace: `omw05::core`, `omw05::io`, `omw05::formats`, `omw05::gfx`.
- Parsers must not touch GL. `formats/` produces plain CPU-side structs;
  `gfx/` uploads them. Keep this boundary absolute so tools/tests build
  headless.

## 5. Technical Notes You Must Get Right

- **Chunk container:** files are sequences of `{ uint32 id; uint32 size; }`
  headers (little-endian) with nested child chunks when `id & 0x80000000`.
  Alignment padding chunks (id `0x00000000`) are common. Build one generic
  recursive `ChunkReader` with a visitor/callback API; every format parser
  is a set of chunk handlers on top of it. Maintain `ChunkIds.h` as a single
  exhaustive enum with source comments.
- **Compression:** `.LZC`/embedded blocks use EA RefPack and JDLZ. Implement
  both decompressors from open documentation/ports; add synthetic-buffer
  round-trip tests where possible (decompress known hand-crafted samples).
- **Hashes:** the engine keys resources by bin-hash (case-shifted 32-bit)
  and VLT hash (64-bit in MW). Implement both in `core/Hash.h`; they're
  required to resolve names everywhere.
- **Textures:** MW PC texture packs contain DXT1/3/5 and palettized/RGBA
  surfaces. On GLES3: probe `GL_EXT_texture_compression_s3tc`; if absent
  (most mobile GPUs), decode DXT on CPU (`DxtDecode.h`) to RGBA8, or
  optionally transcode. Never assume S3TC exists.
- **Geometry:** solid chunks reference materials by hash into texture packs;
  vertex formats vary per chunk version — table them in `docs/formats/solids.md`.
  First deliverable is `mw-solid2gltf` so correctness is visually verifiable
  in any glTF viewer before the in-engine renderer exists.
- **World streaming:** `TRACKS/` contains a master bundle + numbered
  streaming section bundles. Implement: parse section table → load sections
  by camera position → LRU-unload. Design `TrackStreamer` around async-ready
  interfaces but a synchronous first implementation is fine.
- **GLES3 renderer:** GLSL ES 3.00 shaders (`#version 300 es`,
  `precision highp float;`). VAOs, UBO for camera matrices. sRGB awareness.
  No immediate mode anywhere.
- **Endianness/packing:** never `reinterpret_cast` file bytes into structs.
  Read through an explicit `Stream`/`Span` reader with typed getters. The
  formats are little-endian PC; keep readers explicit anyway so console
  variants can come later.

## 5a. Render Architecture: Offscreen FBOs + Compositor (mandatory from day one)

Nothing except the final composite pass ever draws to the default
framebuffer. This is a hard architectural rule, designed for ARM embedded
targets where the display compositor does not rotate/scale for you, and for
weak GPUs that can't render 3D at native resolution.

- **`RenderTarget` (gfx/RenderTarget.h):** RAII wrapper over an FBO with a
  color texture (RGBA8 or sRGB) and optional depth-stencil renderbuffer.
  Resizable; handles GLES3 completeness checks.
- **Two independent targets:**
  - `sceneRT` — the 3D world. Its resolution is decoupled from the window:
    a runtime **render-scale factor** (e.g. 1.0 / 0.75 / 0.5, config +
    hotkey + CLI `--render-scale`) so weak devices render 3D at reduced
    resolution and upscale in the composite.
  - `uiRT` — all 2D/UI (ImGui, debug HUD, later the game frontend). Always
    at native (post-rotation) display resolution so text stays crisp,
    regardless of the 3D render scale. Cleared to transparent, alpha-blended
    over the scene in the composite.
- **`Compositor` (gfx/Compositor.h):** final fullscreen pass that samples
  `sceneRT` then `uiRT` onto the default framebuffer, applying a
  **presentation rotation of 0/90/180/270°** (config `--rotate <deg>` +
  env `OMW05_ROTATE`). Rotation is a UV/matrix transform in the composite
  shader — no per-object hacks. When rotated 90/270 the logical width/height
  swap: input coordinates (mouse/touch) must be transformed inversely in one
  central place (app layer), and both RTs are allocated in *logical*
  orientation. Linear filtering when upscaling the scene; UI composited 1:1.
- Keep the composite pass trivially cheap (one shader, two texture fetches);
  it must run comfortably on Mali/VideoCore-class GPUs. Structure it so a
  post-processing hook (color grading, MW's "yellow filter") can slot in
  later between scene and UI, but do not implement post effects now.
- M0's definition changes accordingly: even the very first "clear screen"
  build must already clear into `sceneRT` and present through the
  `Compositor`, with `--rotate` and `--render-scale` functional.

## 5b. Dear ImGui

Dear ImGui (MIT) **is an approved dependency** — vendor it in `extern/imgui/`
with the SDL2 platform backend and the OpenGL ES 3.0 renderer backend
(`imgui_impl_sdl2` + `imgui_impl_opengl3` with `#version 300 es`).

- Use it freely for developer/debug UI: chunk tree inspector, texture
  browser, streaming-section debug view, camera/render-scale controls,
  log console, stats overlay.
- ImGui renders **into `uiRT` only**, via `ImGuiLayer` — never directly to
  the default framebuffer, so it participates in rotation automatically.
  Feed it the rotation-corrected (logical) input coordinates.
- ImGui is a debug/dev tool. The eventual reimplementation of the game's
  own frontend (FNG) must not depend on ImGui — keep `ImGuiLayer`
  compile-time removable (CMake option `OMW05_WITH_IMGUI`, default ON).

## 6. Milestones (work strictly in order, keep `main` always building)

- **M0 — Skeleton.** CMake superstructure, SDL2 window with ES 3.0 context,
  `RenderTarget` + `Compositor` (scene FBO cleared to a color, UI FBO with an
  ImGui demo/stats window, composite with working `--rotate 0|90|180|270`
  and `--render-scale`), logging, CI script (`scripts/build.sh`), empty
  module targets, README with build instructions (Linux + Windows/MSYS2 or
  MSVC; note ARM/embedded Linux as a first-class target).
- **M1 — Chunk layer.** `ChunkReader`, `ChunkIds`, RefPack+JDLZ, hashes,
  `mw-chunkdump` CLI printing a recursive chunk tree of any game file.
  Unit tests with synthetic buffers.
- **M2 — Textures.** TexturePack parser, CPU DXT decode, `mw-texdump`
  exporting PNGs (stb_image_write), and an SDL2 in-engine texture browser.
- **M3 — Solids.** Solid geometry parser, `mw-solid2gltf`, in-engine model
  viewer with orbit camera rendering a car model with its textures.
- **M4 — World.** Track streaming sections, scenery placement, free-fly
  camera over Rockport with LOD/streaming by position. Frustum culling.
- **M5 — Data layer.** VLT database parser (`mw-vltdump`), language packs,
  groundwork docs for FNG (frontend), collision, NIS — headers + stub
  parsers with TODOs are acceptable here.

After each milestone: update README status table, update `docs/formats/`,
commit with a clear message. Do not start M(n+1) until M(n) builds and its
tool runs end-to-end.

## 7. Runtime Asset Access

- Game path comes from `--gamedir <path>` or env `OMW05_GAMEDIR`. Validate
  it (expect `TRACKS/`, `GLOBAL/`, `CARS/`, `FRONTEND/` subdirs) and fail
  with a helpful message.
- `.gitignore` must exclude any conceivable asset spill: `*.BUN *.BIN *.LZC
  *.VIV *.big /gamedata/ /assets/`.
- Tests and CI must pass **without** a game directory present; anything
  needing real files goes behind a manual `mw-*` tool invocation.

## 8. Coding Conventions

- Warnings as errors where feasible (`-Wall -Wextra`; keep MSVC clean too).
- No exceptions across module boundaries for parse failures — return
  `Result<T>`/error codes; parsers must survive corrupt input (fuzz-friendly).
- No global mutable state except the logger.
- `clang-format` file at repo root; format everything.
- Every format struct/field gets a comment citing where the knowledge came
  from (repo/file or doc), so `docs/formats/` and code stay auditable.
- Dependencies: SDL2, glad (GLES3), glm, stb, Dear ImGui (SDL2 + GLES3
  backends, behind `OMW05_WITH_IMGUI`), a C++17 test framework
  (doctest/catch2), CLI11 or hand-rolled arg parsing. Nothing heavier
  without strong justification. No Boost.

## 9. What NOT To Do

- Don't implement gameplay (physics/AI/police/UI logic) yet — this repo
  phase is loaders + viewer. Leave clearly marked extension points.
- Don't copy code from unlicensed repos or matching-decomp repos; port
  knowledge, rewrite code, credit authors.
- Don't silently downgrade the GLES3 requirement to "whatever desktop GL".
- Don't commit binaries, generated files, or anything from a game disc.
- Don't invent chunk IDs or field layouts. If undocumented and unverifiable,
  parse it as opaque bytes, name it `UnknownXX`, log it, and file a TODO in
  `docs/formats/`.

## 10. Definition of Done (per task)

1. Builds clean on C++17 with warnings-as-errors.
2. Headers document ownership/lifetime and cite format sources.
3. Tool or viewer demonstrates the feature against a real game dir (user
   runs it; you provide the exact command in the PR/commit message).
4. `THIRD_PARTY.md`, `CREDITS.md`, `docs/formats/`, README status updated.
