#include "resources/dolresources.h"
#include "game/player/playerinputdecoder.h"
#include "game/startup/startupresources.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    float constant(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    std::array<float, 3> normalize(const std::array<float, 3>& value, float epsilon) {
        const float length = std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
        if (length < epsilon) return {};
        const float inverse = 1 / length;
        return {value[0] * inverse, value[1] * inverse, value[2] * inverse};
    }
}

playerinput::Decoder::Decoder(const std::vector<std::uint8_t>& executable) {
    threshold = constant(executable, 0x804daa84);
    epsilon = constant(executable, 0x804daa88);
    buttonMaximum = constant(executable, 0x804daa94);
    triggerMaximum = constant(executable, 0x804daa90);
    triggerThreshold = constant(executable, 0x804daaa0);
    diagonalCorrection = constant(executable, 0x804daab0);
}

float playerinput::Decoder::magnitude(float x, float y) const {
    const double ax = x <= 0 ? -double(x) : x, ay = y <= 0 ? -double(y) : y;
    float ratio = 0;
    if (ay > ax && ay > 0) ratio = static_cast<float>(ax / ay);
    else if (ay < ax && ax > 0) ratio = static_cast<float>(ay / ax);
    const float length = std::sqrt(x * x + y * y);
    return std::clamp(static_cast<float>(double(length) * (1 + static_cast<float>(double(ratio) * diagonalCorrection))), 0.0f, 1.0f);
}

unsigned playerinput::Decoder::direction(const playercontrols::Motion& motion) const {
    const float z = motion.second[2] * motion.value, x = motion.second[0] * motion.value;
    if (threshold <= z || threshold <= -z) {
        if (x < threshold && -x < threshold) return z < threshold ? 4 : 2;
    } else if (x < threshold) {
        if (threshold <= -x) return 3;
    } else return 1;
    return 0;
}

playercontrols::Motion playerinput::Decoder::transform(float x, float y, const Context& context, unsigned mode) const {
    playercontrols::Motion result;
    result.value = magnitude(x, y);
    if (mode == 0) {
        const float length = std::sqrt(x * x + y * y);
        if (length > 0) {
            const float inverse = 1 / length;
            result.first = {x * inverse, y * inverse, 0};
            result.second = {x * inverse, 0, y * inverse};
        }
        return result;
    }
    if (mode > 2) throw std::runtime_error("Invalid original control transform mode.");
    auto matrix = context.view;
    if (mode == 2) {
        const auto rotation = playerorientation::toMatrix(context.orientation);
        for (unsigned row = 0; row < 3; ++row) for (unsigned column = 0; column < 3; ++column)
            matrix[row * 4 + column] = rotation[row * 4] * context.view[column] +
                rotation[row * 4 + 1] * context.view[4 + column] + rotation[row * 4 + 2] * context.view[8 + column];
    }
    result.first = normalize({matrix[0] * x + matrix[4] * y, matrix[1] * x + matrix[5] * y,
        matrix[2] * x + matrix[6] * y}, epsilon);
    result.second = normalize({matrix[0] * x - matrix[8] * y, 0, matrix[2] * x - matrix[10] * y}, epsilon);
    return result;
}

float playerinput::Decoder::analog(const playercontrols::State& state, const Pad& pad, unsigned selector) const {
    switch (selector) {
    case 1: return pad.stick[0];
    case 2: return pad.stick[1];
    case 3: return pad.substick[0];
    case 4: return pad.substick[1];
    case 5: return std::min(float(pad.triggerLeft), triggerMaximum) / triggerMaximum;
    case 6: return std::min(float(pad.triggerRight), triggerMaximum) / triggerMaximum;
    case 7: return std::min(float(pad.analogA), buttonMaximum) / buttonMaximum;
    case 8: return std::min(float(pad.analogB), buttonMaximum) / buttonMaximum;
    case 9: return pad.stickMagnitude;
    case 10: return pad.substickMagnitude;
    case 11: return state.field4;
    case 12: return state.field8;
    default: return 0;
    }
}

bool playerinput::Decoder::button(const playercontrols::State& state, const Context& context, unsigned selector, bool edge) const {
    if (edge && !context.edges) return false;
    if (selector == 0x8000005) return state.flags0[edge ? 0 : 1] != 0;
    if (selector == 0x8000006) return state.flags0[edge ? 2 : 3] != 0;
    if (!selector) return false;
    if (selector >= 0x8000001 && selector <= 0x8000004) {
        const auto index = selector <= 0x8000002 ? 0 : 2;
        const auto& current = state.history[index];
        const auto& previous = state.history[index + 1];
        if (selector & 1) return threshold <= current.value && (!edge || previous.value < threshold);
        const auto value = direction(current), old = direction(previous);
        return value != 0 && (!edge || old == 0 || value != old);
    }
    return ((edge ? context.pad->pressed : context.pad->held) & selector) != 0;
}

playercontrols::Motion playerinput::Decoder::motion(const Context& context, unsigned source, unsigned mode, unsigned inversion) const {
    const auto& pad = *context.pad;
    float x = 0, y = 0;
    const auto digital = [&](unsigned left, unsigned right) { return float(int((pad.held & right) != 0) - int((pad.held & left) != 0)); };
    switch (source) {
    case 0: break;
    case 1: x = pad.stick[0]; y = pad.stick[1]; break;
    case 2: x = pad.substick[0]; y = pad.substick[1]; break;
    case 3: x = digital(0x1000000, 0x2000000); y = digital(0x4000000, 0x8000000); break;
    case 4: x = digital(0x10000, 0x20000); y = digital(0x40000, 0x80000); break;
    case 5: x = std::min(float(pad.triggerRight), triggerMaximum) / triggerMaximum -
        std::min(float(pad.triggerLeft), triggerMaximum) / triggerMaximum; break;
    case 6: x = float(int(std::min(float(pad.triggerRight), triggerMaximum) / triggerMaximum > triggerThreshold) -
        int(std::min(float(pad.triggerLeft), triggerMaximum) / triggerMaximum > triggerThreshold)); break;
    case 7: x = digital(1, 2); y = digital(4, 8); break;
    case 8: x = digital(0x40, 0x20); break;
    default: break;
    }
    auto result = transform(x, y, context, mode);
    if (inversion == 2 || inversion == 3) { result.first[0] = -result.first[0]; result.second[0] = -result.second[0]; }
    if (inversion == 1 || inversion == 3) { result.first[1] = -result.first[1]; result.second[2] = -result.second[2]; }
    return result;
}

void playerinput::Decoder::update(playercontrols::State& state, const Context& context) const {
    if (!context.pad) { state.reset(); return; }
    state.history[1] = state.history[0];
    state.history[3] = state.history[2];
    state.history[0] = transform(context.pad->stick[0], context.pad->stick[1], context, 1);
    state.history[2] = transform(context.pad->substick[0], context.pad->substick[1], context, 1);
    const auto& fields = state.settings.fields;
    const auto alias = [](unsigned selector) { return selector == 0x8000005 || selector == 0x8000006; };
    const auto edge = [&](unsigned index) { return button(state, context, fields.at(index), true); };
    const auto held = [&](unsigned index) { return button(state, context, fields.at(index), false); };
    if (alias(fields[0]) || alias(fields[1])) {
        state.flags0[0] = button(state, context, 0x100, true);
        state.flags0[1] = button(state, context, 0x100, false);
    } else { state.flags0[0] = edge(0) || edge(1); state.flags0[1] = held(0) || held(1); }
    state.field4 = analog(state, *context.pad, fields[3] == 11 || fields[3] == 12 ? 7 : fields[3]);
    state.flags0[2] = alias(fields[2]) ? button(state, context, 0x200, true) : edge(2);
    state.flags0[3] = alias(fields[2]) ? button(state, context, 0x200, false) : held(2);
    state.field8 = analog(state, *context.pad, fields[4] == 11 || fields[4] == 12 ? 8 : fields[4]);
    const unsigned flagSelectors[]{5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 18, 19, 20, 22};
    for (unsigned i = 0; i < state.flagsC.size(); ++i)
        state.flagsC[i] = i == 3 || i == 7 || i == 12 || i == 14 ? held(flagSelectors[i]) : edge(flagSelectors[i]);
    state.flagsC[16] = edge(20) || edge(21);
    state.field20 = analog(state, *context.pad, fields[23]);
    for (unsigned i = 0; i < state.flags24.size(); ++i) state.flags24[i] = i == 0 || i == 3 ? held(24 + i) : edge(24 + i);
    state.field2C = analog(state, *context.pad, fields[29]);
    for (unsigned i = 0; i < state.flags30.size(); ++i) state.flags30[i] = i == 3 ? held(30 + i) : edge(30 + i);
    const unsigned motionOffsets[]{37, 40, 46, 43, 49};
    for (unsigned i = 0; i < state.motions.size(); ++i) {
        const auto offset = motionOffsets[i];
        state.motions[i] = motion(context, fields[offset], fields[offset + 1], fields[offset + 2]);
    }
    state.fieldC4 = held(52);
    state.fieldC5 = held(53);
    state.fieldC8 = state.field2C;
    state.fieldCC = state.flags30[0];
    if (context.suppress) {
        const auto retained = state.flagsC[16];
        state.reset();
        state.flagsC[16] = retained;
    }
}
