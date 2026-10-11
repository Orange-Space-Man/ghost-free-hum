#include "game/world/roommodelpose.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    unsigned read16(const std::vector<std::uint8_t>& data, std::size_t offset) {
        require(offset <= data.size() && data.size() - offset >= 2, "Truncated ANM data.");
        return (data[offset] << 8) | data[offset + 1];
    }

    std::uint32_t read32(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (static_cast<std::uint32_t>(read16(data, offset)) << 16) | read16(data, offset + 2);
    }

    float number(const std::vector<std::uint8_t>& data, std::size_t offset) {
        const auto bits = read32(data, offset);
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        require(std::isfinite(value), "Non-finite ANM value.");
        return value;
    }
}

bool roompose::readInitial(const std::vector<std::uint8_t>& animation, std::size_t nodes,
    std::vector<roommodel::Node>& pose, std::string& error) {
    pose.clear();
    error.clear();
    try {
        require(animation.size() >= 24 && animation[0] == 1 && read32(animation, 4) > 0, "Unsupported ANM header.");
        const std::size_t groups = read32(animation, 20);
        const std::size_t tables[]{read32(animation, 8), read32(animation, 12), read32(animation, 16), animation.size()};
        require(nodes > 0 && nodes <= 0x8000 && groups >= 24 && groups <= tables[0] && nodes <= (tables[0] - groups) / 54 &&
            tables[0] <= tables[1] && tables[1] <= tables[2] && tables[2] <= tables[3], "Invalid ANM sections.");
        std::vector<roommodel::Node> result(nodes);
        for (std::size_t node = 0; node < nodes; ++node) {
            result[node].index = static_cast<std::uint16_t>(node);
            for (unsigned channel = 0; channel < 9; ++channel) {
                const auto group = groups + node * 54 + channel * 6;
                const auto count = read16(animation, group);
                const auto first = read16(animation, group + 2);
                const auto flags = read16(animation, group + 4);
                const auto table = channel / 3;
                const auto key = tables[table] + static_cast<std::size_t>(first) * 4;
                const std::size_t stride = flags == 0 ? 12 : 16;
                const std::size_t length = count == 1 ? 4 : count * stride;
                require(count > 0 && (flags == 0 || flags == 0x80) && key <= tables[table + 1] && length <= tables[table + 1] - key,
                    "Invalid ANM channel range.");
                if (count > 1) require(number(animation, key) == 0, "Initial ANM key does not start at frame zero.");
                const float value = number(animation, key + (count == 1 ? 0 : 4));
                auto& component = channel < 3 ? result[node].scale : channel < 6 ? result[node].rotation : result[node].translation;
                component[channel % 3] = value;
            }
        }
        pose = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
