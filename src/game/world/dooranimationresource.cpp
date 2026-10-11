#include "game/world/dooranimationresource.h"

#include <stdexcept>
#include <utility>

namespace {
    std::uint32_t word(const std::vector<std::uint8_t>& data, unsigned offset) {
        return (std::uint32_t(data[offset]) << 24) | (std::uint32_t(data[offset + 1]) << 16) |
            (std::uint32_t(data[offset + 2]) << 8) | data[offset + 3];
    }
}

bool dooranimation::Resource::read(const std::vector<std::uint8_t>& data, std::string& error) {
    error.clear();
    try {
        if (data.size() < 24) throw std::runtime_error("Truncated door animation header.");
        Resource result;
        result.attributes = {data[0], data[1]};
        result.value = word(data, 4);
        for (unsigned i = 0; i < 4; ++i) {
            result.offsets[i] = word(data, 8 + i * 4);
            if (result.offsets[i] > data.size()) throw std::runtime_error("Door animation section is outside its resource.");
        }
        result.storage = data;
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool dooranimation::Resource::loaded() const { return !storage.empty(); }
const std::vector<std::uint8_t>& dooranimation::Resource::bytes() const { return storage; }
std::uint8_t dooranimation::Resource::headerByte(unsigned index) const { return attributes.at(index); }
std::uint32_t dooranimation::Resource::headerValue() const { return value; }
std::uint32_t dooranimation::Resource::sectionOffset(unsigned index) const { return offsets.at(index); }
