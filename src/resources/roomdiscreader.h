#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace roomdisc {
    struct File {
        std::string path;
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
    };

    bool listFiles(const std::filesystem::path& path, std::vector<File>& files, std::string& error);
    bool readExecutable(const std::filesystem::path& path, std::vector<std::uint8_t>& data, std::string& error);
    bool readFile(const std::filesystem::path& path, const std::string& internalPath, std::vector<std::uint8_t>& data, std::string& error);
    bool read(const std::filesystem::path& path, std::vector<std::uint8_t>& model, std::string& error);
}
