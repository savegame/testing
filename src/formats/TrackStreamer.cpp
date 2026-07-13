#include "openmw05/formats/TrackStreamer.h"

#include "openmw05/core/Log.h"
#include "openmw05/core/Stream.h"
#include "openmw05/io/ChunkIds.h"
#include "openmw05/io/ChunkReader.h"

#include <cstdio>

namespace omw05 {
namespace formats {

namespace {

constexpr std::size_t kSectionRecordSize = 0x5C;   // TrackStreamingSection (decomp)
constexpr std::size_t kSceneryInfoSize = 0x48;     // SceneryInfo (decomp)
constexpr std::size_t kSceneryInstanceSize = 0x40; // SceneryInstance (decomp)

std::size_t alignPad(std::size_t absOffset, std::size_t alignment) {
    const std::size_t rem = absOffset % alignment;
    return rem == 0 ? 0 : alignment - rem;
}

std::string readFixedString(core::Stream& s, std::size_t len) {
    core::ByteSpan raw = s.bytes(len);
    std::string out;
    for (std::size_t i = 0; i < raw.size() && raw[i] != 0; ++i) {
        out.push_back(static_cast<char>(raw[i]));
    }
    return out;
}

// 0x5C on-disk record; trailing 0x24 bytes are runtime state (zero on disk).
StreamingSection parseSectionRecord(core::Stream& s) {
    StreamingSection rec;
    rec.name = readFixedString(s, 8);
    rec.number = s.i16();
    s.skip(2);  // WasRendered, CurrentlyVisible
    s.skip(4);  // Status
    rec.fileType = s.u32();
    rec.fileOffset = s.u32();
    rec.size = s.u32();
    rec.compressedSize = s.u32();
    rec.permSize = s.u32();
    s.skip(4);  // SectionPriority
    rec.centre[0] = s.f32();
    rec.centre[1] = s.f32();
    rec.radius = s.f32();
    rec.checksum = s.u32();
    s.skip(0x5C - 0x38);  // runtime fields (timestamps, pointers)
    return rec;
}

void parseSceneryInfos(const io::Chunk& c, ScenerySection* out) {
    core::Stream s(c.data);
    const std::size_t count = c.data.size() / kSceneryInfoSize;
    for (std::size_t i = 0; i < count; ++i) {
        SceneryInfo info;
        info.debugName = readFixedString(s, 24);
        for (int j = 0; j < 4; ++j) {
            info.modelHash[j] = s.u32();
        }
        s.skip(16);  // eModel* pointers (zero on disk)
        info.radius = s.f32();
        s.skip(4);  // mesh checksum
        info.hierarchyHash = s.u32();
        s.skip(4);  // ModelHeirarchy* pointer
        out->infos.push_back(std::move(info));
    }
}

void parseSceneryInstances(const io::Chunk& c, ScenerySection* out) {
    // Payload is aligned to 16 within the file (chunk->GetAlignedData(16)).
    const std::size_t pad = alignPad(c.fileOffset + 8, 16);
    core::Stream s(c.data.subspan(pad));
    const std::size_t count = s.remaining() / kSceneryInstanceSize;
    for (std::size_t i = 0; i < count; ++i) {
        SceneryInstance inst;
        for (int j = 0; j < 3; ++j) {
            inst.bboxMin[j] = s.f32();
        }
        for (int j = 0; j < 3; ++j) {
            inst.bboxMax[j] = s.f32();
        }
        inst.excludeFlags = s.u32();
        s.skip(2 + 2);  // preculler info index, lighting context
        for (int j = 0; j < 3; ++j) {
            inst.position[j] = s.f32();
        }
        for (int j = 0; j < 9; ++j) {  // i16/8192 fixed point (decomp GetRotation)
            inst.rotation[j] = static_cast<float>(s.i16()) / 8192.0f;
        }
        inst.sceneryInfoNumber = s.i16();
        out->instances.push_back(inst);
    }
}

} // namespace

core::Result<std::vector<StreamingSection>> parseStreamingSections(core::ByteSpan file) {
    std::vector<StreamingSection> sections;
    core::Result<void> walk = io::walkChunks(file, [&](const io::Chunk& c, int) {
        if (c.id == static_cast<std::uint32_t>(io::ChunkId::TrackStreamingSections)) {
            core::Stream s(c.data);
            const std::size_t count = c.data.size() / kSectionRecordSize;
            for (std::size_t i = 0; i < count; ++i) {
                sections.push_back(parseSectionRecord(s));
            }
            return false;
        }
        return true;
    });
    if (!walk) {
        return walk.error();
    }
    return sections;
}

core::Result<std::vector<ScenerySection>> parseScenerySections(core::ByteSpan file) {
    std::vector<ScenerySection> result;
    core::Result<void> walk = io::walkChunks(file, [&](const io::Chunk& c, int) {
        if (c.id != static_cast<std::uint32_t>(io::ChunkId::ScenerySection)) {
            return true;
        }
        ScenerySection section;
        (void)io::forEachChunk(c.data, c.fileOffset + 8, [&](const io::Chunk& child) {
            switch (child.id) {
            case 0x00034101: {  // ScenerySectionHeader: SectionNumber at +0xC
                core::Stream s(child.data);
                s.skip(0xC);
                section.sectionNumber = s.i32();
                break;
            }
            case 0x00034102:
                parseSceneryInfos(child, &section);
                break;
            case 0x00034103:
                parseSceneryInstances(child, &section);
                break;
            default:
                break;  // tree/preculler/overrides: culling data, not needed yet
            }
        });
        result.push_back(std::move(section));
        return false;
    });
    if (!walk) {
        return walk.error();
    }
    return result;
}

core::Result<TrackStreamer> TrackStreamer::open(const std::string& gamedir,
                                                const std::string& trackId) {
    TrackStreamer ts;
    const std::string masterPath = gamedir + "/TRACKS/" + trackId + ".BUN";
    ts.streamPath_ = gamedir + "/TRACKS/STREAM" + trackId + ".BUN";

    auto master = core::readFile(masterPath);
    if (!master) {
        return master.error();
    }
    ts.master_ = master.take();

    auto sections =
        parseStreamingSections(core::ByteSpan(ts.master_.data(), ts.master_.size()));
    if (!sections) {
        return sections.error();
    }
    ts.sections_ = sections.take();
    if (ts.sections_.empty()) {
        return core::Error{core::ErrorCode::CorruptData,
                           "no streaming section table in " + masterPath};
    }
    OMW05_LOG_INFO("formats", "track %s: %zu streaming sections", trackId.c_str(),
                   ts.sections_.size());
    return core::Result<TrackStreamer>(std::move(ts));
}

core::Result<std::vector<std::uint8_t>> TrackStreamer::loadSection(
    const StreamingSection& section) const {
    std::FILE* f = std::fopen(streamPath_.c_str(), "rb");
    if (!f) {
        return core::Error{core::ErrorCode::IoError, "cannot open " + streamPath_};
    }
    if (std::fseek(f, static_cast<long>(section.fileOffset), SEEK_SET) != 0) {
        std::fclose(f);
        return core::Error{core::ErrorCode::IoError,
                           "seek failed for section " + section.name};
    }
    // compressedSize == size for uncompressed sections; PC MW free roam
    // streams uncompressed. If they differ, read the compressed image
    // (decompression of section images: TODO, log for now).
    const std::size_t readSize = section.size;
    std::vector<std::uint8_t> data(readSize);
    const std::size_t got = std::fread(data.data(), 1, data.size(), f);
    std::fclose(f);
    if (got != data.size()) {
        return core::Error{core::ErrorCode::IoError,
                           "short read for section " + section.name};
    }
    return data;
}

} // namespace formats
} // namespace omw05
