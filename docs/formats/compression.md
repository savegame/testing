# Compression: EA RefPack and JDLZ

**Status:** decompression implemented (`io/Compression.h`); compression not
needed (the engine never writes game files).

## RefPack (a.k.a. QFS)

**Sources:** Niotso wiki "RefPack" page (public format documentation);
cross-checked with `SpeedReflect/Binary`'s implementation.

Big-endian 16-bit header; signature `(header & 0x3EFF) == 0x10FB`.

- bit `0x8000`: size fields are 4 bytes (else 3), big-endian
- bit `0x0100`: a compressed-size field precedes the uncompressed-size field

Followed by the uncompressed size, then a command stream. Each command
copies `proceed` literal bytes then (except literal/stop commands) a
back-reference of `refLen` bytes from `refDist` bytes behind the output
cursor (overlap allowed):

| first byte | total bytes | fields |
|---|---|---|
| `0x00–0x7F` | 2 | `proceed = b0 & 3`; `len = ((b0 & 0x1C) >> 2) + 3`; `dist = ((b0 & 0x60) << 3) + b1 + 1` |
| `0x80–0xBF` | 3 | `proceed = b1 >> 6`; `len = (b0 & 0x3F) + 4`; `dist = ((b1 & 0x3F) << 8) + b2 + 1` |
| `0xC0–0xDF` | 4 | `proceed = b0 & 3`; `len = ((b0 & 0x0C) << 6) + b3 + 5`; `dist = ((b0 & 0x10) << 12) + (b1 << 8) + b2 + 1` |
| `0xE0–0xFB` | 1 | literal run: `proceed = ((b0 & 0x1F) + 1) * 4` |
| `0xFC–0xFF` | 1 | stop: `proceed = b0 & 3`, then end |

## JDLZ

**Sources:** community `JDLZ.cs` implementations (algorithm long documented
in NFS modding tools; also present in `MaxHwoy/Nikki`). Re-implemented from
the format behavior, credited in CREDITS.md.

16-byte header:

| offset | type | meaning |
|---|---|---|
| 0 | `char[4]` | `"JDLZ"` |
| 4 | `uint8` | version, observed `0x02` |
| 5 | `uint8` | observed `0x10` |
| 6 | `uint16 LE` | flags, observed 0 |
| 8 | `uint32 LE` | uncompressed size |
| 12 | `uint32 LE` | total stream size incl. header |

Body: two interleaved LSB-first flag streams. `flags1` selects
literal (0) vs back-reference (1) per step; on a back-reference, `flags2`
selects the encoding of the two-byte reference:

- `flags2` bit = 1 (near): `dist = (b0 & 0x0F) + 1`, `len = (b1 | ((b0 & 0xF0) << 4)) + 3`
- `flags2` bit = 0 (far): `dist = (b1 | ((b0 & 0xE0) << 3)) + 17`, `len = (b0 & 0x1F) + 3`

Each flag byte provides 8 steps (`flag = byte | 0x100`, shift right per use;
when the register hits 1, fetch the next byte). `flags2` is consumed/shifted
only by back-reference steps.

## TODO

- `.LZC` container files and `LZCompressed` (0x55441122) chunk wrapping:
  verify header layout against real files in M2 (they embed one of the two
  schemes above).
- Verify decompressors against real game files (M2 texture packs).
