# CREDITS

This project consolidates format knowledge produced over many years by the
NFS reverse-engineering community. Code is written fresh for this repo (or
ported under license, see `THIRD_PARTY.md`), but the *knowledge* — chunk IDs,
structure layouts, compression schemes — comes from the people and projects
below.

- **Arushan** — original VLTEdit and MW Geometry Compiler (redistributed with
  permission via the NFSTools org); foundational VLT + solid format work.
- **MaxHwoy / SpeedReflect** — `Binary`, the de-facto reference chunk
  (`.BIN`/`.BUN`/`.LZC`) reader/writer.
- **heyitsleo & nfs-tools contributors** — NFS-ModTools and related tooling;
  geometry/texture/track chunk parsing and streaming-section documentation.
  The MW solid geometry parser (`formats/Solids`) follows the layouts
  documented by NFS-ModTools' SolidReader/MostWantedSolidReader.
- **berkayylmao** — NFSPluginSDK, reverse-engineered Blackbox-era struct
  layouts.
- **dbalatoni13 & contributors** — nfsmw matching decompilation (GameCube),
  used strictly as naming/semantics documentation.
- **Ekey** — NFS.BIN.Tool (console ZZDATA archives).
- **OpenNFS contributors** — architecture reference for an
  original-asset-loading NFS engine.
- The **Classic Need for Speed** community (Discord) — the hub where much of
  this knowledge lives.
