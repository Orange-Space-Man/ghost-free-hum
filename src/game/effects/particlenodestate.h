#pragma once

#include <array>
#include <cstdint>

namespace particles {
    struct Emitter;
    struct Node;

    class NodeCallback {
    public:
        virtual ~NodeCallback() = default;
        virtual bool advance(Emitter& emitter, Node& node) = 0;
    };

    struct Node {
        Node();
        void reset();
        std::array<float, 9> fields0;
        std::array<std::uint32_t, 3> fields24;
        std::array<std::uint16_t, 2> fields30;
        std::uint8_t field34;
        std::array<std::uint8_t, 3> fields35;
        std::array<float, 3> fields38;
        std::array<std::uint16_t, 2> fields44;
        float field48;
        std::array<float, 2> fields4C;
        std::array<std::uint8_t, 5> fields54;
        std::array<std::uint8_t, 3> fields59;
        Node* next;
        Node* previous;
    };

    struct ShortNode {
        ShortNode();
        std::array<float, 6> fields0{};
        std::array<std::uint8_t, 2> fields18{};
        std::array<std::uint8_t, 6> fields1A;
        std::array<std::uint8_t, 4> fields20{};
        ShortNode* next;
        ShortNode* previous;
    };
}
