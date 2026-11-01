#include "game/player/playerbreathframe.h"
#include "game/actors/actortimingsampler.h"
#include "game/world/dooranimationpose.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

playeractor::BreathFrame::BreathFrame(const std::vector<std::uint8_t>& executable) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, glme01::playerBreathJointParameters, 16, bytes, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < parameters.size(); ++i) {
        const auto offset = i * 4;
        const std::uint32_t bits = (unsigned(bytes[offset]) << 24) | (unsigned(bytes[offset + 1]) << 16) |
            (unsigned(bytes[offset + 2]) << 8) | bytes[offset + 3];
        std::memcpy(&parameters[i], &bits, 4);
    }
}

void playeractor::BreathFrame::run(Player& player, const actormodel::Catalog& catalog,
    const actormodel::Resources& resources, bool roomCleared) const {
    auto& state = player.state;
    const auto matrix = dooranimation::concatenate(player.matrix, player.model->matrix(0, 0x33));
    for (unsigned i = 0; i < 3; ++i) {
        state.fieldFC4[i] = matrix[i * 4 + 3] +
            (matrix[i * 4] * parameters[0] + matrix[i * 4 + 2] * parameters[1]);
        state.fieldFD0[i] = (matrix[i * 4] * parameters[2] + matrix[i * 4 + 1] * 0) + matrix[i * 4 + 2] * parameters[3];
    }
    const auto& direction = state.fieldFD0;
    const float length = std::sqrt((direction[0] * direction[0] + direction[1] * direction[1]) + direction[2] * direction[2]);
    if (length >= 0.0001f) for (auto& value : state.fieldFD0) value *= 1 / length;
    else state.fieldFD0.fill(0);
    const auto& type = catalog.type(player.actorType);
    const auto timing = resources.timing(type.model, type.animations.at(player.animationState)[0]);
    const float previous = static_cast<float>(double(player.animationFrame) - double(player.animationFrameStep));
    state.fieldFDC = actoranimation::timingCrossing(timing.get(), "Lbreath_timing", previous, player.animationFrame);
    if (state.fieldFDC < 0) state.fieldFDC = -1;
    state.fieldFE0 = !(player.flags & 0xc000) && !roomCleared && !state.flags1140[2] ?
        actoranimation::sampleTiming(timing.get(), "Lbreath_lv", player.animationFrame) : 0;
    if (state.fieldFE0 < 0) state.fieldFE0 = 2;
}
