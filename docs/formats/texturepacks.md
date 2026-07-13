# Texture packs (TPK)

**Status:** parser implemented (`formats/TexturePack.h`), DXT1/3/5 + RGBA32
decode (`gfx/DxtDecode.h`), `mw-texdump` exporter. Palettized (P4/P8) and
16-bit formats: TODO (parsed, not decoded).

## Sources

- `MaxHwoy/Nikki` (MIT): `Support.MostWanted/Class/TPKBlock.cs`,
  `Texture.cs`, `Reflection/Enum/TextureCompressionType.cs` — authoritative
  MW field layouts (this file documents what the parser ports).
- Chunk IDs: `ChunkIds.h` (TPK_* entries, from Nikki's BinBlockID).

## Chunk structure

```
TPK_InfoBlock  0xB3310000 (container)
  TPK_InfoPart1 0x33310001   pack header (0x7C bytes)
  TPK_InfoPart2 0x33310002   {u32 hash, u32 0} per texture (hash index)
  TPK_InfoPart3 0x33310003   compressed-entry offsets (LZC packs; TODO)
  TPK_InfoPart4 0x33310004   texture entries, 0x7C bytes each
  TPK_InfoPart5 0x33310005   per-texture compression slots
TPK_DataBlock  0xB3320000 (container)
  TPK_DataPart1 0x33320001   {u32 1, u32 size} header
  TPK_DataPart2 0x33320002   texture bytes; entry offsets relative to
                             payload start + 0x7C
```

## InfoPart1 (pack header, 0x7C bytes)

| offset | type | field |
|---|---|---|
| 0x00 | u32 | header size (0x7C) |
| 0x04 | u32 | version |
| 0x08 | char[0x1C] | collection name |
| 0x24 | char[0x40] | source .tpk path |
| 0x64 | u32 | pack key (binHash) |
| 0x68 | 0x14 bytes | unknown |

## InfoPart4 texture entry (0x7C bytes, MW)

| offset | type | field |
|---|---|---|
| 0x00 | 12 bytes | unknown/zero |
| 0x0C | char[0x18] | texture name (may be truncated; binkey is canonical) |
| 0x24 | u32 | name binHash |
| 0x28 | u32 | class key |
| 0x2C | u32 | unknown |
| 0x30 | u32 | data offset (relative to DataPart2 payload + 0x7C) |
| 0x34 | u32 | palette offset (ditto) |
| 0x38 | u32 | data size |
| 0x3C | u32 | palette size |
| 0x40 | u32 | area |
| 0x44 | u16 | width |
| 0x46 | u16 | height |
| 0x48 | u8 | log2 width |
| 0x49 | u8 | log2 height |
| 0x4A | u8 | compression type (see below) |
| 0x4B | u8 | palette compression |
| 0x4C | u16 | palette count |
| 0x4E | u8 | mipmap count |
| 0x4F.. | bytes | tileable/bias/render-order/scroll/alpha fields + 5×u32 unknown |

## Compression types (TextureCompressionType)

`4`=P4, `8`=P8, `16..19`=16-bit variants, `24`=RGB24, `32`=RGBA32 (stored
BGRA on PC), `34`=DXT1, `36`=DXT3, `38`=DXT5, `40`=L8. Full list in
`formats/TexturePack.h`.

## TODO

- P4/P8 palette decode; 16-bit variants; L8.
- Compressed packs (InfoPart3 / LZC): decompress per-texture entries.
- Mipmap chain extraction (data currently exposed as one blob).
- Verify against real GLOBAL/TRACKS packs (user-run, M2 definition of done).
