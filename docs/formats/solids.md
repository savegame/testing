# Solid geometry (MW05 PC)

**Status:** parser implemented (`formats/Solids.h`), `mw-solid2gltf`
exporter. Sources: NFSTools/NFS-ModTools (`SolidReader.cs`,
`MostWantedSolidReader.cs`, `MostWantedSolidListReader.cs`) — no license
file, used as format documentation; code re-implemented (CREDITS.md).

## Chunk structure

```
GeometryPack 0x80134000 (container)
  0x80134001 header container
    0x00134002  list info: i64 pad, i32 marker, i32 numObjects,
                char[0x38] filename, char[0x20] group name
    0x00134003  object hash table (8 b/entry)
    0x00134004  unknown (24 b/entry)
  0x80134008  empty
  0x80134010 solid object container (one per object)
    0x00134011  object header (align 0x10, 0xA0 bytes + name, see below)
    0x00134012  texture table: {u32 binHash, u32 pad} per texture
    0x00134013  light materials {u32,u32}
    0x00134017/18/19  normal smoother / smooth vertices
    0x0013401A  position markers
    0x80134100  mesh ("plat") container
      0x00134900  descriptor (align 0x10): i64, i32, u32 flags, u32 numMats,
                  u32, u32 numVertexStreams, i64, i64, u32 numIndices
      0x00134b01  vertex buffer, payload aligned to 0x80 (may repeat —
                  one buffer per vertex stream)
      0x00134b02  shading groups (align 0x10, 0x68 bytes each, below)
      0x00134b03  index buffer (align 0x10): u16[], per material in order
      0x00134c02  material name (one chunk per named material)
```

## Object header (0x00134011, after 0x10 alignment)

| offset | type | field |
|---|---|---|
| 0x00 | 12 bytes | zero |
| 0x0C | u8 | version (0x16 in MW) |
| 0x0D | u8 | endian-swapped flag |
| 0x0E | u16 | flags |
| 0x10 | u32 | name binHash |
| 0x14 | u16,u16 | numPolys, numVerts |
| 0x18 | u8×4 | numBones, numTextureTableEntries, numLightMaterials, numPositionMarkers |
| 0x1C | u32 | pad |
| 0x20 | f32×3 + pad | bounds min |
| 0x30 | f32×3 + pad | bounds max |
| 0x40 | f32×16 | pivot transform (row-major) |
| 0x80 | 32 bytes | unknown |
| 0xA0 | cstring | object name |

## Shading group (0x68 bytes)

| offset | type | field |
|---|---|---|
| 0x00/0x0C | f32×3 ×2 | bounds min/max |
| 0x18..0x1C | u8×5 | diffuse/normal/height/specular/opacity texture index |
| 0x1D | u8 | light material number |
| 0x1E | u16 | unknown |
| 0x20 | 16 bytes | zero |
| 0x30 | u32 | effect ID (vertex format selector) |
| 0x34 | u32 | effect pointer (0 on disk) |
| 0x38 | u32 | flags |
| 0x3C | u32 | numVerts |
| 0x40 | u32 | numTris |
| 0x44 | 24 bytes | zero |
| 0x5C | u32 | numIndices (= numTris*3) |
| 0x60 | 8 bytes | zero |

## Vertex formats by effect ID

Vertex stream index increments whenever consecutive shading groups change
effect ID; stride = streamBytes / streamVertexCount.

| effect | id | layout | stride |
|---|---|---|---|
| WorldShader / CarShader / GlossyWindow / billboardshader | 0/4/5/6 | pos f32×3, normal f32×3, color u32, uv f32×2 | 36 |
| WorldReflectShader / WorldNormalMap | 1/3 | ... + 8 unknown, tangent f32×3, pad4 | 60 |
| WorldBoneShader | 2 | ... + blend weights f32×3, blend indices f32×3 | 60 |
| skyshader | 19 | ... + 8 unknown | 44 |

Unknown effect IDs: vertices zero-filled using the stream stride, logged
(never guessed).

## Coordinates

MW is Z-up. `mw-solid2gltf` swizzles to glTF Y-up: (x, y, z) → (x, z, −y).

## TODO

- Tangents/bone data are currently dropped.
- Vertex color is parsed but not exported to glTF (COLOR_0).
- Verify against real CARS/*/GEOMETRY.BIN (user-run).
