#pragma once

#include "game/models/modeldrawresource.h"

#include <functional>

namespace modeldraw {
    struct TraversalState {
        bool overrideOpacity = false, nodeLightingOverride = false;
        std::uint8_t mode = 0, flaggedOpacity = 255, limitedOpacity = 255, lightingFlags = 0;
    };

    struct DrawCommand {
        std::int16_t node = 0, material = 0, batch = 0;
        std::uint8_t opacity = 255, lightingFlags = 0;
        bool alternateMaterial = false;
    };

    using DrawSubmitter = std::function<void(const DrawCommand&)>;
    using NodeSubmitter = std::function<void(std::int16_t)>;
    void traverse(const Resource& resource, unsigned pass, std::uint8_t opacity,
        TraversalState& state, const DrawSubmitter& submit, std::int16_t root = 0, bool skipRootSiblings = false,
        const NodeSubmitter& prepareNode = {});
    bool cullMode(std::uint8_t batchCull, std::uint8_t mode, std::uint8_t& output);
}
