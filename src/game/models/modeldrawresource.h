#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace modeldraw {
    struct Binding {
        std::int16_t material = 0, batch = 0;
    };

    struct Node {
        std::int16_t child = -1, sibling = -1;
        std::uint16_t flags = 0;
        std::int16_t batchCount = 0;
        std::vector<Binding> bindings;
    };

    struct Batch {
        std::uint16_t polygonCount = 0;
        std::uint32_t attributes = 0;
        std::uint8_t lighting = 0, cull = 0, textureCount = 0, normalMode = 0;
        std::vector<std::uint8_t> displayList;
    };

    class Resource {
    public:
        bool read(const std::vector<std::uint8_t>& bytes, std::string& error);
        const Node& node(std::int16_t index) const;
        const Batch& batch(std::int16_t index) const;
        std::uint8_t materialPass(std::int16_t index) const;
        const std::map<std::int16_t, Node>& nodes() const;

    private:
        std::map<std::int16_t, Node> hierarchy;
        std::map<std::int16_t, Batch> batches;
        std::map<std::int16_t, std::uint8_t> passes;
    };
}
