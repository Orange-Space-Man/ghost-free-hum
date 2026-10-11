#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace roommodel {
    struct Section {
        std::uint32_t headerOffset = 0;
        std::uint32_t fileOffset = 0;
        bool optional = false;
    };

    struct Node {
        std::uint16_t index = 0;
        std::int32_t link2 = -1;
        std::int32_t link4 = -1;
        std::array<float, 3> scale{};
        std::array<float, 3> rotation{};
        std::array<float, 3> translation{};
    };

    struct Model {
        std::size_t fileSize = 0;
        std::uint8_t firstByte = 0;
        std::vector<Section> sections;
        std::vector<Node> nodes;
    };

    bool inspect(const std::vector<std::uint8_t>& data, Model& model, std::string& error);
}
