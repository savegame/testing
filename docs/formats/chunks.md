# bChunk container format (`.BUN` / `.BIN`)

**Status:** implemented (`io/ChunkReader.h`, `io/ChunkIds.h`, `mw-chunkdump`).

## Sources

- `github.com/SpeedReflect/Binary` + `github.com/MaxHwoy/Nikki` (MIT) — the
  reference chunk reader/writer; our ID table is ported from Nikki's
  `BinBlockID.cs` (see THIRD_PARTY.md).
- `github.com/OpenNFS/OpenNFS` — loader architecture reference.
- `github.com/dbalatoni13/nfsmw` — bChunk naming/semantics (documentation only).

## Layout

A chunked file is a flat byte stream of consecutive chunks:

| offset | type | meaning |
|---|---|---|
| +0 | `uint32 LE` | chunk ID |
| +4 | `uint32 LE` | payload size in bytes (not counting this 8-byte header) |
| +8 | `size` bytes | payload |

- **Containers:** when `(id & 0x80000000) != 0` the payload is itself a
  sequence of child chunks. Recurse.
- **Padding:** `id == 0x00000000` is alignment filler (payload is garbage or
  zeros). Blackbox aligns many chunks to 0x10/0x40/0x80/0x800 boundaries —
  the per-ID alignment notes from Nikki are preserved as comments in
  `ChunkIds.h`.
- All observed PC MW files are little-endian.

## Known IDs

See `include/openmw05/io/ChunkIds.h` — 145 IDs ported from Nikki. Per
CLAUDE.md §9 the table only grows with IDs verified against community
sources; unknown chunks are parsed as opaque bytes and logged.

## Parser notes

- A chunk whose declared size exceeds the remaining buffer is corrupt input;
  `walkChunks` reports `CorruptData` (chunks before the corruption are still
  visited). No read ever leaves the buffer.
- Some payloads are compressed (JDLZ/RefPack) — see `compression.md`;
  notably `LZCompressed` (0x55441122) blocks and `.LZC` files.
