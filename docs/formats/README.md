# Format documentation

Consolidated notes on NFS:MW (2005, PC) file formats, one file per format,
with sources cited. These documents are a project deliverable in their own
right and must stay in sync with the parsers in `include/openmw05/formats/`.

Planned documents:

- `chunks.md` — the generic chunk container (`.BIN`/`.BUN`), IDs, alignment.
- `compression.md` — EA RefPack and JDLZ.
- `hashes.md` — bin-hash (32-bit) and VLT hash (64-bit).
- `texturepacks.md` — texture pack chunks, surface formats (DXT1/3/5, P8, RGBA).
- `solids.md` — solid geometry chunks, vertex format table per chunk version.
- `trackstreamer.md` — master bundle + `STREAML2RA_*` streaming sections.
- `vlt.md` — VLT database.
- `langpacks.md` — string/language packs.
- `fng.md`, `collision.md`, `nis.md` — later milestones (stubs).
