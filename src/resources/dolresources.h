#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dolresources {
    bool read(const std::vector<std::uint8_t>& executable, std::uint32_t address,
        std::size_t size, std::vector<std::uint8_t>& output, std::string& error);
    bool readStaticArchiveResource(const std::vector<std::uint8_t>& executable, const std::string& path,
        std::vector<std::uint8_t>& output, std::string& error);
}
