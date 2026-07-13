// mw-chunkdump: prints the recursive chunk tree of any chunked game file
// (.BUN/.BIN and friends). M1 deliverable (CLAUDE.md §6).
//
// Usage: mw-chunkdump <file> [--max-depth N] [--decompress]
//   --decompress  if the file is a JDLZ/RefPack stream, decompress first.

#include "openmw05/core/Stream.h"
#include "openmw05/io/ChunkIds.h"
#include "openmw05/io/ChunkReader.h"
#include "openmw05/io/Compression.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
    const char* path = nullptr;
    int maxDepth = 64;
    bool tryDecompress = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--max-depth") && i + 1 < argc) {
            maxDepth = std::atoi(argv[++i]);
        } else if (!std::strcmp(argv[i], "--decompress")) {
            tryDecompress = true;
        } else if (!path) {
            path = argv[i];
        } else {
            std::fprintf(stderr, "usage: mw-chunkdump <file> [--max-depth N] [--decompress]\n");
            return 2;
        }
    }
    if (!path) {
        std::fprintf(stderr, "usage: mw-chunkdump <file> [--max-depth N] [--decompress]\n");
        return 2;
    }

    auto file = omw05::core::readFile(path);
    if (!file) {
        std::fprintf(stderr, "error: %s\n", file.error().message.c_str());
        return 1;
    }
    std::vector<std::uint8_t> bytes = file.take();
    omw05::core::ByteSpan span(bytes.data(), bytes.size());

    std::vector<std::uint8_t> decompressed;
    if (tryDecompress) {
        auto d = omw05::io::decompressAuto(span);
        if (d.ok()) {
            decompressed = d.take();
            span = omw05::core::ByteSpan(decompressed.data(), decompressed.size());
            std::printf("(decompressed %zu -> %zu bytes)\n", bytes.size(), decompressed.size());
        } else {
            std::fprintf(stderr, "note: not decompressed: %s\n", d.error().message.c_str());
        }
    }

    std::size_t count = 0;
    auto result = omw05::io::walkChunks(
        span,
        [&count](const omw05::io::Chunk& c, int depth) {
            ++count;
            for (int i = 0; i < depth; ++i) {
                std::printf("  ");
            }
            const char* name = omw05::io::chunkIdName(c.id);
            std::printf("0x%08X %-28s size=%-10zu @0x%zX%s\n", c.id, name ? name : "?",
                        c.data.size(), c.fileOffset, c.isContainer() ? " [container]" : "");
            return true;
        },
        maxDepth);

    std::printf("%zu chunks\n", count);
    if (!result) {
        std::fprintf(stderr, "warning: %s\n", result.error().message.c_str());
        return 1;
    }
    return 0;
}
