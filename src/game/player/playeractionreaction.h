#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace playeractions {
    struct ReactionPayload {
        ReactionPayload();
        void reset();
        std::array<std::optional<std::uint32_t>, 16> fields;
    };
}
