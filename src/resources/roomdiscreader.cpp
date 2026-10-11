#include "resources/roomdiscreader.h"
#include "resources/roomarchivereader.h"

#include <fstream>
#include <algorithm>
#include <stdexcept>

namespace {
    bool contains(std::size_t size, std::size_t offset, std::size_t length) {
        return offset <= size && length <= size - offset;
    }

    std::uint32_t read32(const std::vector<std::uint8_t>& data, std::size_t offset) {
        if (!contains(data.size(), offset, 4)) {
            throw std::runtime_error("Truncated file table.");
        }
        return (static_cast<std::uint32_t>(data[offset]) << 24) |
            (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
            (static_cast<std::uint32_t>(data[offset + 2]) << 8) | data[offset + 3];
    }

    std::string readName(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t end) {
        std::string name;
        while (offset < end && offset < data.size()) {
            const char value = static_cast<char>(data[offset++]);
            if (value == 0) {
                return name;
            }
            if (name.size() >= 1024) {
                break;
            }
            name += value;
        }
        throw std::runtime_error("Invalid filename in file table.");
    }

    std::vector<std::uint8_t> readRange(std::ifstream& file, std::uint64_t size, std::uint64_t offset, std::uint64_t length) {
        if (offset > size || length > size - offset || length > 64 * 1024 * 1024) {
            throw std::runtime_error("Disc data range is invalid or exceeds 64 MiB.");
        }
        std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
        file.seekg(static_cast<std::streamoff>(offset));
        if (!file || !file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(length))) {
            throw std::runtime_error("Could not read disc data.");
        }
        return data;
    }


}

bool roomdisc::listFiles(const std::filesystem::path& path, std::vector<File>& files, std::string& error) {
    files.clear();
    error.clear();
    try {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file || file.tellg() < 0) {
            throw std::runtime_error("Cannot open the ISO.");
        }
        const std::uint64_t size = static_cast<std::uint64_t>(file.tellg());
        const auto header = readRange(file, size, 0, 0x440);
        if (read32(header, 0x1c) != 0xc2339f3d) {
            throw std::runtime_error("This is not an uncompressed GameCube ISO.");
        }
        if (std::string(header.begin(), header.begin() + 6) != "GLME01" || header[6] != 0 || header[7] != 0) {
            throw std::runtime_error("This build supports Luigi's Mansion USA revision 0 (GLME01).");
        }
        const auto table = readRange(file, size, read32(header, 0x424), read32(header, 0x428));
        const std::size_t count = read32(table, 8);
        if (read32(table, 0) >> 24 != 1 || count == 0 || count > table.size() / 12) {
            throw std::runtime_error("Invalid disc file table.");
        }
        const std::size_t strings = count * 12;
        struct Directory {
            std::size_t end = 0;
            std::string path;
        };
        std::vector<Directory> parents{{count, ""}};
        std::vector<File> result;
        for (std::size_t index = 1; index < count; ++index) {
            while (!parents.empty() && index >= parents.back().end) {
                parents.pop_back();
            }
            if (parents.empty()) {
                throw std::runtime_error("Invalid disc directory nesting.");
            }
            const std::uint32_t typeName = read32(table, index * 12);
            const std::size_t nameOffset = typeName & 0xffffff;
            if (nameOffset >= table.size() - strings) {
                throw std::runtime_error("Invalid disc filename offset.");
            }
            const std::string name = readName(table, strings + nameOffset, table.size());
            if (name.empty() || name == "." || name == ".." || name.find_first_of("/\\:") != std::string::npos) {
                throw std::runtime_error("Invalid disc path component.");
            }
            const std::string fullPath = parents.back().path + name;
            if (typeName >> 24 == 1) {
                const std::size_t end = read32(table, index * 12 + 8);
                if (end <= index || end > parents.back().end) {
                    throw std::runtime_error("Invalid disc directory range.");
                }
                parents.push_back({end, fullPath + "/"});
            } else if (typeName >> 24 != 0) {
                throw std::runtime_error("Invalid disc entry type.");
            } else {
                const std::uint64_t offset = read32(table, index * 12 + 4);
                const std::uint64_t length = read32(table, index * 12 + 8);
                if (offset > size || length > size - offset) throw std::runtime_error("Truncated disc file: " + fullPath);
                result.push_back({fullPath, offset, length});
            }
        }
        files = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool roomdisc::readFile(const std::filesystem::path& path, const std::string& internalPath, std::vector<std::uint8_t>& model, std::string& error) {
    model.clear();
    std::vector<File> files;
    if (!listFiles(path, files, error)) return false;
    try {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file || file.tellg() < 0) throw std::runtime_error("Cannot open the ISO.");
        const auto size = static_cast<std::uint64_t>(file.tellg());
        for (const auto& entry : files) {
            if (entry.path == internalPath) {
                model = readRange(file, size, entry.offset, entry.size);
                return true;
            }
        }
        throw std::runtime_error("Disc file not found: " + internalPath);
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool roomdisc::read(const std::filesystem::path& path, std::vector<std::uint8_t>& model, std::string& error) {
    model.clear();
    std::vector<std::uint8_t> archive;
    return readFile(path, "Iwamoto/map2/room_00.arc", archive, error) && roomarchive::read(archive, "room.bin", model, error);
}

bool roomdisc::readExecutable(const std::filesystem::path& path, std::vector<std::uint8_t>& data, std::string& error) {
    data.clear();
    error.clear();
    try {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file || file.tellg() < 0) throw std::runtime_error("Cannot open the ISO.");
        const std::uint64_t size = static_cast<std::uint64_t>(file.tellg());
        const auto header = readRange(file, size, 0, 0x440);
        if (read32(header, 0x1c) != 0xc2339f3d || std::string(header.begin(), header.begin() + 6) != "GLME01" || header[6] != 0 || header[7] != 0) {
            throw std::runtime_error("Unsupported disc for executable data.");
        }
        const auto start = read32(header, 0x420);
        const auto dol = readRange(file, size, start, 0x100);
        std::uint64_t length = 0x100;
        for (unsigned index = 0; index < 18; ++index) {
            const auto sectionSize = read32(dol, 0x90 + index * 4);
            if (sectionSize == 0) continue;
            length = std::max(length, static_cast<std::uint64_t>(read32(dol, index * 4)) + sectionSize);
        }
        data = readRange(file, size, start, length);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
