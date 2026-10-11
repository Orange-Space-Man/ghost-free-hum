#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace roomarchive {
    bool decompress(const std::vector<std::uint8_t>& data, std::vector<std::uint8_t>& output, std::string& error);
    bool read(const std::vector<std::uint8_t>& archive, const std::string& path,
        std::vector<std::uint8_t>& output, std::string& error, bool* missing = nullptr);
}
