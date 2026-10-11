#include "game/actors/actorcapturemotion.h"
#include "game/actors/moveactor.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    unsigned half(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (unsigned(data.at(offset)) << 8) | data.at(offset + 1);
    }

    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (half(data, offset) << 16) | half(data, offset + 2);
    }

    float number(const std::vector<std::uint8_t>& data, std::size_t offset) {
        const auto bits = word(data, offset);
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
}

void actorcapture::initializeActor(Motion& motion, State& state, actors::MoveActor& actor, const Vector& target, std::uint16_t type) {
    if (actor.flags & 0x80000) return;
    actor.field100 = target;
    if (!actor.model->model) return;
    actor.flags |= 0x80000;
    state.number(0x54, actor.field160);
    motion.initialize(state, actor.field100, actor.position, type);
    actor.field160 = state.number(0x54);
}

void actorcapture::updateActor(Motion& motion, State& state, actors::MoveActor& actor, const playermodels::VacuumPath* path,
    const Vector* suctionDirection, actormodel::Matrix& retainedTransform,
    const Vector* originalScratch, const float* originalRegister, const bool* emptyCompletion) {
    if (!(actor.flags & 0x80000) || !actor.model->model) return;
    state.number(0x54, actor.field160);
    const auto& data = actor.model->model->bytes;
    const unsigned count = half(data, 8) + half(data, 12);
    motion.advance(state, actor.field100, actor.position, actor.scale, actor.matrix, path, suctionDirection, originalScratch);
    const std::size_t positions = std::size_t(word(data, 0x48)) + std::size_t(half(data, 0x10)) * 12;
    bool complete = false;
    if (!count) {
        if (!emptyCompletion) throw std::runtime_error("Original empty capture model requires the retained completion register.");
        complete = *emptyCompletion;
    }
    for (unsigned i = 0; i < count; ++i) {
        const auto offset = positions + std::size_t(i) * 12;
        const Vector position{number(data, offset), number(data, offset + 4), number(data, offset + 8)};
        auto& matrix = actor.model->matrix(0, i);
        Vector transformed;
        for (unsigned row = 0; row < 3; ++row) {
            const unsigned r = row * 4;
            transformed[row] = std::fma(matrix[r + 2], position[2], matrix[r] * position[0]) +
                (matrix[r + 3] + matrix[r + 1] * position[1]);
        }
        complete = motion.deform(state, 1, transformed, matrix, retainedTransform, originalRegister);
    }
    if (complete) actor.flags &= ~0x80000u;
    actor.field160 = state.number(0x54);
}
