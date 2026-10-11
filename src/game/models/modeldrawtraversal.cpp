#include "game/models/modeldrawtraversal.h"

#include <algorithm>
#include <stdexcept>

void modeldraw::traverse(const Resource& resource, unsigned pass, std::uint8_t opacity,
    TraversalState& state, const DrawSubmitter& submit, std::int16_t root, bool skipRootSiblings,
    const NodeSubmitter& prepareNode) {
    if (!submit) throw std::runtime_error("Model traversal requires a draw consumer.");
    struct Visit { std::int16_t index; std::uint8_t opacity; bool enter, skipSiblings; };
    std::vector<Visit> pending{{root, opacity, true, skipRootSiblings}};
    while (!pending.empty()) {
        auto visit = pending.back();
        pending.pop_back();
        if (visit.index == -1) continue;
        const auto& node = resource.node(visit.index);
        if (visit.enter && !visit.skipSiblings) {
            pending.push_back({visit.index, visit.opacity, false, false});
            pending.push_back({node.sibling, visit.opacity, true, false});
            continue;
        }
        if (!state.overrideOpacity && (node.flags & 4)) {
            visit.opacity = state.flaggedOpacity;
            if (!visit.opacity) continue;
        }
        if (!state.overrideOpacity && (node.flags & 8)) {
            if ((pass & 255) != 2) continue;
            visit.opacity = std::min<std::uint8_t>(visit.opacity, 100);
        }
        state.lightingFlags = static_cast<std::uint8_t>((state.lightingFlags & ~2U) |
            (state.nodeLightingOverride && (node.flags & 0x40) ? 2 : 0));
        if (!state.overrideOpacity && (node.flags & 0x80)) {
            if (!state.limitedOpacity) continue;
            visit.opacity = std::min(visit.opacity, state.limitedOpacity);
        }
        const bool suppress = (state.mode == 1 && !(node.flags & 2)) || (!state.mode && (node.flags & 0x10));
        if (!suppress && node.batchCount && prepareNode) prepareNode(visit.index);
        if (!suppress) for (const auto& binding : node.bindings) {
            const auto materialPass = resource.materialPass(binding.material);
            if (visit.opacity == 255 ? (materialPass >= 128 || materialPass != (pass & 255)) : (pass & 255) != 2)
                continue;
            submit({visit.index, binding.material, binding.batch, visit.opacity, state.lightingFlags,
                visit.opacity == 255 && state.mode != 0});
        }
        pending.push_back({node.child, visit.opacity, true, false});
    }
}

bool modeldraw::cullMode(std::uint8_t batchCull, std::uint8_t mode, std::uint8_t& output) {
    if (!mode) output = batchCull;
    else if (batchCull == 0) output = 0;
    else if (batchCull == 1) output = 2;
    else if (batchCull == 2) output = 1;
    else return false;
    return true;
}
