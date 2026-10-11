#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace gameassets {
    using Progress = std::function<bool(std::uint64_t, std::uint64_t)>;

    struct Entry {
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
        std::uint32_t checksum = 0;
    };

    class Package {
    public:
        bool open(const std::filesystem::path& path, std::string& error);
        bool read(const std::string& path, std::vector<std::uint8_t>& data, std::string& error);
        const std::map<std::string, Entry>& entries() const;

    private:
        std::ifstream file;
        std::map<std::string, Entry> index;
    };

    bool importDisc(const std::filesystem::path& disc, const std::filesystem::path& destination,
        std::string& error, const Progress& progress = {});
}
