#include "openmw05/core/Stream.h"

#include <cstdio>

namespace omw05 {
namespace core {

Result<std::vector<std::uint8_t>> readFile(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return Error{ErrorCode::IoError, "cannot open " + path};
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    if (size < 0) {
        std::fclose(f);
        return Error{ErrorCode::IoError, "cannot stat " + path};
    }
    std::fseek(f, 0, SEEK_SET);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    const std::size_t got = size ? std::fread(data.data(), 1, data.size(), f) : 0;
    std::fclose(f);
    if (got != data.size()) {
        return Error{ErrorCode::IoError, "short read on " + path};
    }
    return data;
}

} // namespace core
} // namespace omw05
