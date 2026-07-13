#pragma once
// NFS:MW world streaming (M4) — CPU-side only (CLAUDE.md §4).
//
// Format sources: dbalatoni13/nfsmw matching decompilation used strictly as
// struct/layout documentation (TrackStreamer.hpp: TrackStreamingSection
// 0x5C; Scenery.hpp: SceneryInfo 0x48, SceneryInstance 0x40), chunk IDs
// cross-checked with NFS-ModTools ChunkDef. Code written fresh for this
// repo. See docs/formats/trackstreamer.md.
//
// Data flow: TRACKS/<track>.BUN (master bundle) carries the streaming
// section table (0x00034110) plus track-wide texture packs; the section
// payloads live in TRACKS/STREAM<track>.BUN at [fileOffset, size). Each
// loaded section is a normal chunked buffer with GeometryPack, TPK and
// ScenerySection (0x80034100) chunks.
//
// TrackStreamer below is the synchronous first implementation with an
// async-ready interface (request/isLoaded/release), per CLAUDE.md §5.

#include "openmw05/core/Result.h"
#include "openmw05/core/Span17.h"

#include <cstdint>
#include <string>
#include <vector>

namespace omw05 {
namespace formats {

// One record of BCHUNK_TRACKSTREAMER_SECTIONS (0x00034110), 0x5C bytes.
struct StreamingSection {
    std::string name;              // up to 8 chars ("A0", "B23", ...)
    std::int16_t number = 0;
    std::uint32_t fileType = 0;
    std::uint32_t fileOffset = 0;  // into STREAM<track>.BUN
    std::uint32_t size = 0;        // uncompressed/loaded size
    std::uint32_t compressedSize = 0;
    std::uint32_t permSize = 0;
    float centre[2] = {0, 0};      // world XY
    float radius = 0;
    std::uint32_t checksum = 0;
};

// BCHUNK_SCENERY_INFOS entry (0x00034102), 0x48 bytes: one placeable model
// with up to 4 LOD name hashes (A..D, binHash of solid names).
struct SceneryInfo {
    std::string debugName;         // char[24], often empty on PC
    std::uint32_t modelHash[4] = {0, 0, 0, 0};
    float radius = 0;
    std::uint32_t hierarchyHash = 0;
};

// BCHUNK_SCENERY_INSTANCES entry (0x00034103), 0x40 bytes, 16-aligned.
struct SceneryInstance {
    float bboxMin[3] = {0, 0, 0};
    float bboxMax[3] = {0, 0, 0};
    std::uint32_t excludeFlags = 0;
    float position[3] = {0, 0, 0};
    float rotation[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};  // row-major 3x3 (i16/8192 on disk)
    std::int16_t sceneryInfoNumber = 0;
};

struct ScenerySection {
    std::int32_t sectionNumber = 0;
    std::vector<SceneryInfo> infos;
    std::vector<SceneryInstance> instances;
};

// Parses every 0x00034110 chunk found in a (master) bundle.
core::Result<std::vector<StreamingSection>> parseStreamingSections(core::ByteSpan file);

// Parses every ScenerySection (0x80034100) container found in a buffer
// (master bundle or a loaded streaming section).
core::Result<std::vector<ScenerySection>> parseScenerySections(core::ByteSpan file);

// Synchronous streaming-section loader over the master + stream bundle pair.
class TrackStreamer {
public:
    // gamedir: validated game root; trackId e.g. "L2RA" (MW free roam).
    // Reads TRACKS/<trackId>.BUN into memory and opens STREAM<trackId>.BUN.
    static core::Result<TrackStreamer> open(const std::string& gamedir,
                                            const std::string& trackId);

    const std::vector<StreamingSection>& sections() const { return sections_; }
    core::ByteSpan masterBundle() const {
        return core::ByteSpan(master_.data(), master_.size());
    }

    // Loads one section's bytes from the stream file (synchronous).
    core::Result<std::vector<std::uint8_t>> loadSection(const StreamingSection& section) const;

private:
    TrackStreamer() = default;
    std::string streamPath_;
    std::vector<std::uint8_t> master_;
    std::vector<StreamingSection> sections_;
};

} // namespace formats
} // namespace omw05
