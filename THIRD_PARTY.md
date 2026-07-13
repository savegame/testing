# THIRD_PARTY.md — provenance ledger

Every piece of vendored or ported third-party code is recorded here, per
CLAUDE.md §2 (license hygiene). Format documentation sources (no code taken)
are credited in `CREDITS.md` and cited inline in headers/docs.

| Component | Source | Commit/Version | License | What was taken |
|---|---|---|---|---|
| Dear ImGui | https://github.com/ocornut/imgui | v1.91.9b (f5befd2d29e66809cd1110a152e375a7f1981f06) | MIT | Core library + SDL2 platform backend + OpenGL3/GLES3 renderer backend, vendored in `extern/imgui/` |
| glad (generated loader) | https://github.com/Dav1dde/glad (glad2 generator v2.0.8) | generated for `gles2` API 3.0, core, no extensions beyond registry defaults | (WTFPL OR CC0-1.0) AND Apache-2.0 | Generated GLES 3.0 function loader, vendored in `extern/glad/` |
| doctest | https://github.com/doctest/doctest | v2.4.11 | MIT | Single-header test framework, vendored in `extern/doctest/` |
| VaultLib (ported code) | https://github.com/NFSTools/VaultLib | master (fetched 2026-07-13) | MIT ((c) 2019 NFS Tools & heyitsleo) | VLT 32/64-bit hash algorithms ported C#→C++ into `src/core/Hash.cpp` (VLT32Hasher.cs, VLT64Hasher.cs) |
