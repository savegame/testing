// mw-trackdump: lists the streaming section table of a track master bundle
// and (optionally) the scenery content of one section. M4 tool.
//
// Usage:
//   mw-trackdump --gamedir <dir> [--track L2RA]            # section table
//   mw-trackdump --gamedir <dir> --section A5              # inspect section

#include "openmw05/core/Stream.h"
#include "openmw05/formats/Solids.h"
#include "openmw05/formats/TexturePack.h"
#include "openmw05/formats/TrackStreamer.h"

#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    const char* gamedir = std::getenv("OMW05_GAMEDIR");
    const char* track = "L2RA";
    const char* sectionName = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--gamedir") && i + 1 < argc) {
            gamedir = argv[++i];
        } else if (!std::strcmp(argv[i], "--track") && i + 1 < argc) {
            track = argv[++i];
        } else if (!std::strcmp(argv[i], "--section") && i + 1 < argc) {
            sectionName = argv[++i];
        }
    }
    if (!gamedir) {
        std::fprintf(stderr,
                     "usage: mw-trackdump --gamedir <dir> [--track L2RA] [--section A5]\n");
        return 2;
    }

    auto tsResult = omw05::formats::TrackStreamer::open(gamedir, track);
    if (!tsResult) {
        std::fprintf(stderr, "error: %s\n", tsResult.error().message.c_str());
        return 1;
    }
    omw05::formats::TrackStreamer ts = tsResult.take();

    if (!sectionName) {
        std::printf("%-8s %6s %10s %10s %10s %12s %8s\n", "name", "num", "offset", "size",
                    "permSize", "centre", "radius");
        for (const auto& s : ts.sections()) {
            std::printf("%-8s %6d 0x%08X %10u %10u (%6.0f,%6.0f) %8.0f\n", s.name.c_str(),
                        s.number, s.fileOffset, s.size, s.permSize,
                        static_cast<double>(s.centre[0]), static_cast<double>(s.centre[1]),
                        static_cast<double>(s.radius));
        }
        std::printf("%zu sections\n", ts.sections().size());
        return 0;
    }

    for (const auto& s : ts.sections()) {
        if (s.name != sectionName) {
            continue;
        }
        auto data = ts.loadSection(s);
        if (!data) {
            std::fprintf(stderr, "error: %s\n", data.error().message.c_str());
            return 1;
        }
        omw05::core::ByteSpan span(data.value().data(), data.value().size());
        std::printf("section %s: %zu bytes\n", s.name.c_str(), data.value().size());

        auto solids = omw05::formats::parseSolidLists(span);
        if (solids) {
            for (const auto& list : solids.value()) {
                std::printf("  solids '%s': %zu objects\n", list.filename.c_str(),
                            list.objects.size());
            }
        }
        auto tpks = omw05::formats::parseTexturePacks(span);
        if (tpks) {
            for (const auto& pack : tpks.value()) {
                std::printf("  textures '%s': %zu textures\n", pack.name.c_str(),
                            pack.textures.size());
            }
        }
        auto scenery = omw05::formats::parseScenerySections(span);
        if (scenery) {
            for (const auto& sc : scenery.value()) {
                std::printf("  scenery section %d: %zu infos, %zu instances\n",
                            sc.sectionNumber, sc.infos.size(), sc.instances.size());
            }
        }
        return 0;
    }
    std::fprintf(stderr, "section '%s' not found\n", sectionName);
    return 1;
}
