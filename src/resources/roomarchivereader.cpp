#include "resources/roomarchivereader.h"

#include <stdexcept>

namespace {
    bool contains(std::size_t size, std::size_t offset, std::size_t length) {
        return offset <= size && length <= size - offset;
    }

    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    std::uint32_t read32(const std::vector<std::uint8_t>& data, std::size_t offset) {
        require(contains(data.size(), offset, 4), "Truncated archive header.");
        return (static_cast<std::uint32_t>(data[offset]) << 24) |
            (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
            (static_cast<std::uint32_t>(data[offset + 2]) << 8) | data[offset + 3];
    }
}

bool roomarchive::decompress(const std::vector<std::uint8_t>& data, std::vector<std::uint8_t>& output, std::string& error) {
    output.clear();
    error.clear();
    try {
        require(read32(data, 0) == 0x59617930, "Unsupported archive compression.");
        const std::size_t size = read32(data, 4);
        std::size_t links = read32(data, 8);
        std::size_t chunks = read32(data, 12);
        require(size > 0 && size <= 64 * 1024 * 1024 && links >= 16 && links <= chunks && chunks <= data.size(), "Invalid Yay0 header.");
        const std::size_t maskEnd = links;
        const std::size_t linkEnd = chunks;
        std::size_t masks = 16;
        unsigned remaining = 0;
        std::uint32_t mask = 0;
        std::vector<std::uint8_t> result;
        result.reserve(size);
        while (result.size() < size) {
            if (remaining == 0) {
                require(contains(maskEnd, masks, 4), "Truncated Yay0 mask stream.");
                mask = read32(data, masks);
                masks += 4;
                remaining = 32;
            }
            if ((mask & 0x80000000U) != 0) {
                require(chunks < data.size(), "Truncated Yay0 literal stream.");
                result.push_back(data[chunks++]);
            } else {
                require(contains(linkEnd, links, 2), "Truncated Yay0 link stream.");
                const unsigned link = (data[links] << 8) | data[links + 1];
                links += 2;
                const std::size_t distance = (link & 0xfff) + 1;
                std::size_t count = link >> 12;
                if (count == 0) {
                    require(chunks < data.size(), "Truncated Yay0 length.");
                    count = data[chunks++] + 18;
                } else {
                    count += 2;
                }
                require(distance <= result.size() && count <= size - result.size(), "Invalid Yay0 back reference.");
                for (std::size_t index = 0; index < count; ++index) {
                    result.push_back(result[result.size() - distance]);
                }
            }
            mask <<= 1;
            --remaining;
        }
        output = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool roomarchive::read(const std::vector<std::uint8_t>& archive, const std::string& path,
    std::vector<std::uint8_t>& output, std::string& error, bool* missing) {
    output.clear();
    error.clear();
    if (missing) *missing = false;
    try {
        require(archive.size() >= 0x40 && read32(archive, 0) == 0x52415243 &&
            read32(archive, 4) == archive.size() && read32(archive, 8) == 0x20, "Unsupported or damaged RARC archive.");
        const std::size_t nodeCount = read32(archive, 0x20);
        const std::size_t nodes = static_cast<std::size_t>(read32(archive, 0x24)) + 0x20;
        const std::size_t count = read32(archive, 0x28);
        const std::size_t entries = static_cast<std::size_t>(read32(archive, 0x2c)) + 0x20;
        const std::size_t strings = static_cast<std::size_t>(read32(archive, 0x34)) + 0x20;
        const std::size_t stringSize = read32(archive, 0x30);
        const std::size_t payload = static_cast<std::size_t>(read32(archive, 0x0c)) + 0x20;
        const std::size_t payloadSize = read32(archive, 0x10);
        require(nodeCount > 0 && nodeCount <= archive.size() / 16 && count <= archive.size() / 20 &&
            contains(archive.size(), nodes, nodeCount * 16) && contains(archive.size(), entries, count * 20) &&
            contains(archive.size(), strings, stringSize) && contains(archive.size(), payload, payloadSize), "Invalid RARC tables.");
        std::size_t node = 0;
        std::size_t start = 0;
        while (start < path.size()) {
            const auto separator = path.find('/', start);
            const auto component = path.substr(start, separator == std::string::npos ? separator : separator - start);
            require(!component.empty() && component != "." && component != "..", "Invalid archive path.");
            const auto first = read32(archive, nodes + node * 16 + 12);
            const auto length = read32(archive, nodes + node * 16 + 8) & 0xffff;
            require(contains(count, first, length), "Invalid RARC directory entries.");
            bool found = false;
            for (std::size_t index = first; index < first + length; ++index) {
                const auto entry = entries + index * 20;
                const auto flags = read32(archive, entry + 4);
                const auto nameOffset = flags & 0xffffff;
                require(nameOffset < stringSize, "Invalid RARC filename offset.");
                std::size_t end = nameOffset;
                while (end < stringSize && archive[strings + end] != 0) ++end;
                require(end < stringSize && end - nameOffset <= 1024, "Invalid RARC filename.");
                const std::string name(archive.begin() + strings + nameOffset, archive.begin() + strings + end);
                if (name != component) continue;
                const auto offset = read32(archive, entry + 8);
                if (separator != std::string::npos) {
                    require((flags >> 24 & 2) != 0 && offset < nodeCount, "Invalid RARC subdirectory.");
                    node = offset;
                    found = true;
                    break;
                }
                require((flags >> 24 & 1) != 0 && (flags >> 24 & 2) == 0, "Archive path is not a file.");
                const auto size = read32(archive, entry + 12);
                require(contains(payloadSize, offset, size), "File exceeds archive payload.");
                output.assign(archive.begin() + payload + offset, archive.begin() + payload + offset + size);
                return true;
            }
            if (!found) {
                if (missing) *missing = true;
                throw std::runtime_error("Archive file not found: " + path);
            }
            start = separator + 1;
        }
        throw std::runtime_error("Invalid archive path.");
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
