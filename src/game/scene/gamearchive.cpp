#include "game/scene/gamearchive.h"

#include <algorithm>
#include <stdexcept>

namespace {
    bool contains(std::size_t size, std::size_t offset, std::size_t length) {
        return offset <= size && length <= size - offset;
    }
    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        if (!contains(data.size(), offset, 4)) throw std::runtime_error("Truncated archive table.");
        return (unsigned(data[offset]) << 24) | (unsigned(data[offset + 1]) << 16) | (unsigned(data[offset + 2]) << 8) | data[offset + 3];
    }
    struct Name { std::string text; std::uint16_t hash = 0; };
    Name name(std::string_view source) {
        Name result;
        for (unsigned char character : source) {
            if (!character) break;
            if (character >= 128) throw std::runtime_error("Unsupported non-ASCII archive lookup.");
            if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
            result.hash = static_cast<std::uint16_t>(result.hash * 3 + character);
            if (result.text.size() < 32) result.text.push_back(static_cast<char>(character));
        }
        return result;
    }
}

bool gamearchive::Archive::read(const std::vector<std::uint8_t>& source, std::string& error) {
    error.clear();
    try {
        if (source.size() < 64 || word(source, 0) != 0x52415243 || word(source, 4) != source.size() || word(source, 8) != 32)
            throw std::runtime_error("Unsupported archive header.");
        const auto nodeCount = word(source, 32), fileCount = word(source, 40), stringSize = word(source, 48);
        const std::size_t nodeOffset = std::size_t(word(source, 36)) + 32, fileOffset = std::size_t(word(source, 44)) + 32;
        const std::size_t strings = std::size_t(word(source, 52)) + 32, payloadOffset = std::size_t(word(source, 12)) + 32;
        if (!nodeCount || !contains(source.size(), nodeOffset, std::size_t(nodeCount) * 16) ||
            !contains(source.size(), fileOffset, std::size_t(fileCount) * 20) || !contains(source.size(), strings, stringSize) ||
            !contains(source.size(), payloadOffset, word(source, 16))) throw std::runtime_error("Invalid archive ranges.");
        Archive result;
        result.bytes = source;
        result.payload = static_cast<unsigned>(payloadOffset);
        result.payloadSize = word(source, 16);
        for (unsigned i = 0; i < nodeCount; ++i) {
            const auto offset = nodeOffset + i * 16;
            Node node{word(source, offset), word(source, offset + 12), word(source, offset + 8) & 65535};
            if (!contains(fileCount, node.first, node.count)) throw std::runtime_error("Invalid archive directory range.");
            result.nodes.push_back(node);
        }
        for (unsigned i = 0; i < fileCount; ++i) {
            const auto offset = fileOffset + i * 20;
            const auto packed = word(source, offset + 4), start = packed & 0xffffff;
            if (start >= stringSize) throw std::runtime_error("Invalid archive name offset.");
            auto end = start;
            while (end < stringSize && source[strings + end]) ++end;
            if (end == stringSize) throw std::runtime_error("Unterminated archive name.");
            Entry entry{std::string(source.begin() + strings + start, source.begin() + strings + end),
                static_cast<std::uint16_t>(word(source, offset)), packed >> 24, word(source, offset + 8), word(source, offset + 12)};
            if (!(entry.flags & 2) && !contains(result.payloadSize, entry.offset, entry.size))
                throw std::runtime_error("Archive resource outside payload.");
            result.files.push_back(std::move(entry));
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}

int gamearchive::Archive::findTypeResource(std::uint32_t type, std::string_view requested) const {
    if (!type) return -1;
    const auto key = name(requested);
    for (const auto& node : nodes) {
        if (node.type != type) continue;
        for (unsigned i = node.first; i < node.first + node.count; ++i)
            if (files[i].hash == key.hash && files[i].name == key.text) return static_cast<int>(i);
        return -1;
    }
    return -1;
}

int gamearchive::Archive::findNameResource(std::string_view requested) const {
    const auto key = name(requested);
    for (unsigned i = 0; i < files.size(); ++i) {
        if (files[i].hash == key.hash && files[i].name == key.text) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int gamearchive::Archive::findPathResource(std::string_view path) const {
    path = path.substr(0, path.find('\0'));
    if (nodes.empty() || path.empty() || path.front() != '/') {
        return -1;
    }
    path.remove_prefix(1);
    unsigned nodeIndex = 0;
    while (!path.empty()) {
        const auto slash = path.find('/');
        const auto key = name(path.substr(0, slash));
        const auto& node = nodes.at(nodeIndex);
        bool found = false;
        for (unsigned i = node.first; i < node.first + node.count; ++i) {
            const auto& file = files[i];
            if (file.hash != key.hash || file.name != key.text) {
                continue;
            }
            if (!(file.flags & 2)) {
                return slash == std::string_view::npos ? static_cast<int>(i) : -1;
            }
            if (slash == std::string_view::npos) {
                return -1;
            }
            nodeIndex = file.offset;
            if (nodeIndex >= nodes.size()) {
                throw std::runtime_error("Invalid archive directory target.");
            }
            path.remove_prefix(slash + 1);
            found = true;
            break;
        }
        if (!found) {
            return -1;
        }
    }
    return -1;
}

std::vector<unsigned> gamearchive::Archive::directory(std::string_view path) const {
    if (nodes.empty() || path.empty() || path.front() != '/') throw std::runtime_error("Archive directory requires an absolute path.");
    path.remove_prefix(1);
    unsigned index = 0;
    while (!path.empty()) {
        const auto slash = path.find('/');
        const auto key = name(path.substr(0, slash));
        const auto& node = nodes.at(index);
        bool found = false;
        for (unsigned i = node.first; i < node.first + node.count; ++i) {
            if (files[i].hash != key.hash || files[i].name != key.text) continue;
            if (!(files[i].flags & 2)) return {};
            index = files[i].offset;
            if (index >= nodes.size()) throw std::runtime_error("Invalid archive directory target.");
            found = true;
            break;
        }
        if (!found) return {};
        if (slash == std::string_view::npos) break;
        path.remove_prefix(slash + 1);
    }
    std::vector<unsigned> result;
    const auto& node = nodes.at(index);
    for (unsigned i = node.first; i < node.first + node.count; ++i) result.push_back(i);
    return result;
}

const gamearchive::Entry& gamearchive::Archive::entry(unsigned index) const { return files.at(index); }

std::vector<std::uint8_t> gamearchive::Archive::resource(unsigned index) const {
    const auto& file = files.at(index);
    if (!(file.flags & 1) || (file.flags & 6)) throw std::runtime_error("Unsupported archive resource encoding.");
    return {bytes.begin() + payload + file.offset, bytes.begin() + payload + file.offset + file.size};
}
