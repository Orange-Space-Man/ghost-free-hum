#include "jlib/j3d/j3dnametable.h"

#include <stdexcept>

bool j3dmodel::NameTable::read(const std::vector<std::uint8_t>& bytes, std::size_t offset, std::size_t end,
    std::string& error) {
    error.clear();
    try {
        if (end > bytes.size() || offset > end || end - offset < 4) {
            throw std::runtime_error("J3D name table exceeds its resource.");
        }
        const auto half = [&](std::size_t position) {
            if (position > end || end - position < 2) {
                throw std::runtime_error("J3D name entry exceeds its resource.");
            }
            return static_cast<std::uint16_t>((unsigned(bytes[position]) << 8) | bytes[position + 1]);
        };
        const auto count = half(offset);
        if (count > (end - offset - 4) / 4) {
            throw std::runtime_error("J3D name count exceeds its table.");
        }
        std::vector<Entry> result;
        result.reserve(count);
        for (unsigned i = 0; i < count; ++i) {
            const auto record = offset + 4 + i * 4;
            const auto relative = half(record + 2);
            if (relative >= end - offset) {
                throw std::runtime_error("J3D name offset exceeds its table.");
            }
            auto cursor = offset + relative;
            Entry entry{half(record), {}};
            while (cursor < end && bytes[cursor]) {
                entry.name.push_back(static_cast<char>(bytes[cursor++]));
            }
            if (cursor == end) {
                throw std::runtime_error("J3D name has no terminator.");
            }
            result.push_back(std::move(entry));
        }
        entries = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

std::uint16_t j3dmodel::NameTable::key(std::string_view name) {
    std::uint16_t result = 0;
    for (const auto character : name) {
        if (!character) {
            break;
        }
        const auto byte = static_cast<unsigned char>(character);
        result = static_cast<std::uint16_t>(result * 3 + (byte < 128 ? int(byte) : int(byte) - 256));
    }
    return result;
}

int j3dmodel::NameTable::index(std::string_view value) const {
    value = value.substr(0, value.find('\0'));
    const auto hash = key(value);
    for (unsigned i = 0; i < entries.size(); ++i) {
        if (entries[i].key == hash && entries[i].name == value) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const std::string* j3dmodel::NameTable::name(std::uint16_t index) const {
    return index < entries.size() ? &entries[index].name : nullptr;
}

unsigned j3dmodel::NameTable::size() const {
    return static_cast<unsigned>(entries.size());
}
