#include "game/player/playerladderinteraction.h"
#include "game/player/playerorientationupdate.h"
#include "game/actors/actorpathsampler.h"
#include "resources/glme01addresses.h"
#include "resources/dolresources.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    using ladders::Vector;
    enum Constant : unsigned { zero, one, epsilon, turnDistance, stepLength, lookAhead, bodyOffset,
        unused7, unused8, unused9, movementBase, selectionPadding, firstOffset, firstHeight, secondOffset, secondHeight };

    float scalar(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) |
            (unsigned(bytes[2]) << 8) | bytes[3];
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    float dot(const Vector& a, const Vector& b) { return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2]; }
    float magnitude(const Vector& value) { return std::sqrt(dot(value, value)); }
    Vector subtract(const Vector& a, const Vector& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }

    float normalize(Vector& value, float threshold) {
        const float result = magnitude(value);
        if (result >= threshold) {
            const float inverse = 1 / result;
            for (auto& axis : value) axis *= inverse;
        } else value.fill(0);
        return result;
    }

    std::uint32_t unsignedInteger(float value) {
        if (!(value >= 0)) return 0;
        if (value >= 4294967296.0) return 0xffffffff;
        return static_cast<std::uint32_t>(value);
    }

    float clamp(float value, float maximum) {
        if (value < 0) value = 0;
        if (value > maximum) value = maximum;
        return value;
    }

    actormodel::Matrix retained(const playerinteraction::LadderServices& services) {
        if (!services.retainedMatrix) throw std::runtime_error("Original failed ladder sample requires retained stack matrix.");
        return *services.retainedMatrix;
    }
}

playerinteraction::LadderInteraction::LadderInteraction(const std::vector<std::uint8_t>& executable) : fallback(executable, 0) {
    for (unsigned i = 0; i < constants.size(); ++i) constants[i] = scalar(executable, glme01::playerLadderConstants + i * 4);
}

bool playerinteraction::LadderInteraction::select(playercomponents::LadderState& state, const LadderInput& input,
    const ladders::Paths& paths) const {
    for (unsigned ladder = 0; ladder < paths.count(); ++ladder) {
        Vector end{}, endNormal{}, start{}, startNormal{};
        bool selected = false;
        if (paths.endpoint(ladder, true, end, endNormal)) {
            const auto delta = subtract(end, input.position);
            const float distance = magnitude(delta);
            if ((dot(delta, endNormal) < 0 && input.pressed && distance < input.radius + constants[selectionPadding]) ||
                (input.motion && input.motion->value > 0 && distance < input.radius && dot(input.motion->second, endNormal) < 0)) {
                if (!paths.endpoint(ladder, false, start, startNormal))
                    throw std::runtime_error("Selected ladder has no original start endpoint.");
                state.entrySide = delta[1] <= start[1] - input.position[1] ? 2 : 1;
                state.ladder = ladder;
                state.distance = paths.length(ladder);
                selected = true;
            }
        }
        if (!selected && input.pressed && paths.endpoint(ladder, false, start, startNormal)) {
            const auto delta = subtract(start, input.position);
            if (magnitude(delta) < input.radius + constants[selectionPadding] && dot(delta, startNormal) > 0) {
                if (!paths.endpoint(ladder, true, end, endNormal))
                    throw std::runtime_error("Selected ladder has no original end endpoint.");
                state.entrySide = end[1] - input.position[1] <= delta[1] ? 1 : 2;
                state.ladder = ladder;
                state.distance = 0;
                selected = true;
            }
        }
        if (selected) {
            state.step = unsignedInteger(state.distance / constants[stepLength]);
            return true;
        }
    }
    return false;
}

void playerinteraction::LadderInteraction::begin(playercomponents::LadderState& state, playeractions::Actions& actions,
    const LadderInput& input, LadderOutput& output, const LadderServices& services) const {
    actormodel::Matrix matrix;
    if (!services.paths.sample(state.ladder, state.distance, matrix)) matrix = retained(services);
    state.facing = {matrix[2], matrix[6], matrix[10]};
    state.approachPosition = {matrix[3], matrix[7], matrix[11]};
    if (state.entrySide == 1 || state.entrySide == 2) {
        const bool first = state.entrySide == 1;
        for (unsigned i = 0; i < 3; ++i)
            state.approachPosition[i] += state.facing[i] * constants[first ? firstOffset : secondOffset];
        state.approachPosition[1] += constants[first ? firstHeight : secondHeight];
    }
    auto direction = subtract(state.approachPosition, input.position);
    direction[1] = 0;
    if (normalize(direction, constants[epsilon]) == 0) direction = state.facing;
    output.displacement = {};
    output.rotation = playerorientation::heading(direction, false);
    std::array<std::uint32_t, 12> payload{};
    payload[6] = input.reaction;
    payload[10] = state.entrySide;
    actions.request(0x0d, &payload);
}

int playerinteraction::LadderInteraction::approach(const playercomponents::LadderState& state,
    const LadderInput& input, LadderOutput& output) const {
    auto direction = subtract(state.approachPosition, input.position);
    direction[1] = 0;
    const float distance = normalize(direction, constants[epsilon]);
    if (distance < input.speed) {
        output.displacement = subtract(state.approachPosition, input.position);
        output.rotation = playerorientation::heading(state.facing, false);
        return 1;
    }
    if (input.contact) return -1;
    for (unsigned i = 0; i < 3; ++i) output.displacement[i] = direction[i] * input.speed;
    const float blend = clamp(distance / (constants[turnDistance] * input.speed), 1);
    direction = playerorientation::direction(playerorientation::interpolate(blend,
        playerorientation::heading(state.facing, false), playerorientation::heading(direction, false)));
    output.rotation = playerorientation::heading(direction, false);
    return 0;
}

bool playerinteraction::LadderInteraction::climb(playercomponents::LadderState& state, playeractions::Actions& actions,
    const LadderInput& input, LadderOutput& output, const LadderServices& services) const {
    state.distance = constants[stepLength] * static_cast<float>(state.step);
    actormodel::Matrix matrix;
    const float ahead = clamp(state.distance + constants[lookAhead], services.paths.length(state.ladder));
    if (!services.paths.sampleSmoothed(state.ladder, ahead, constants[stepLength], matrix)) matrix = retained(services);
    output.rotation = playerorientation::fromMatrix(matrix);
    services.paths.sampleSmoothed(state.ladder, state.distance, constants[stepLength], matrix);
    Vector up{matrix[1], matrix[5], matrix[9]};
    for (unsigned i = 0; i < 3; ++i)
        output.displacement[i] = matrix[i * 4 + 3] + constants[bodyOffset] * matrix[i * 4 + 2] - input.position[i];
    if (input.cancel) actions.request(0x1f);
    else {
        if (!input.motion) throw std::runtime_error("Original ladder climb requires its bound motion input.");
        if (input.movementThreshold <= input.motion->value) {
            if (dot(up, input.motion->first) <= 0) {
                if (state.step < 2) { state.step = 0; return true; }
                --state.step;
                actions.request(0x1d);
            } else {
                const auto last = unsignedInteger(services.paths.length(state.ladder) / constants[stepLength]);
                if (state.step < last - 3u) {
                    ++state.step;
                    actions.request(0x1c);
                } else { state.step = last; return true; }
            }
        }
    }
    return false;
}

void playerinteraction::LadderInteraction::animate(playercomponents::LadderState& state, const playeractions::Actions& actions,
    const LadderInput& input, LadderOutput& output, const LadderServices& services) const {
    actormodel::Matrix matrix;
    if (!services.paths.sample(state.ladder, state.distance, matrix)) matrix = retained(services);
    matrix[3] = matrix[7] = matrix[11] = 0;
    Vector displacement;
    const auto sample = [&](unsigned type, unsigned animation) {
        const auto& info = services.catalog.type(type);
        const auto& selected = info.animations.at(animation);
        return actoranimation::pathDisplacement(services.models, info.model, selected[0], input.previousFrame,
            input.frame, (selected[5] & 2) != 0, displacement);
    };
    if (sample(input.actorType, input.animation) || sample(0, fallback.entries.at(actions.field60))) {
        for (unsigned i = 0; i < 3; ++i) output.displacement[i] =
            (matrix[i * 4] * displacement[0] + matrix[i * 4 + 1] * displacement[1]) + matrix[i * 4 + 2] * displacement[2];
    } else {
        if (!services.retainedDisplacement) throw std::runtime_error("Original missing ladder animation path requires retained displacement.");
        output.displacement = *services.retainedDisplacement;
    }
    const auto stage = actions.fields31C[1];
    if (stage > 3 && stage < 6) {
        state.distance += dot(output.displacement, {matrix[1], matrix[5], matrix[9]});
        if (!input.motion) throw std::runtime_error("Original animated ladder step requires motion input.");
        output.movementScale = constants[movementBase] + input.motion->value * constants[movementBase];
    }
    const float ahead = clamp(state.distance + constants[lookAhead], services.paths.length(state.ladder));
    services.paths.sampleSmoothed(state.ladder, ahead, constants[stepLength], matrix);
    output.rotation = playerorientation::fromMatrix(matrix);
}

bool playerinteraction::LadderInteraction::run(playercomponents::LadderState& state, playeractions::Actions& actions,
    const LadderInput& input, LadderOutput& output, const LadderServices& services) const {
    LadderOutput next;
    next.rotation = playerorientation::heading(state.facing, false);
    if (actions.field310 != 2) {
        if (actions.field310 != 1 || !input.eligible || !select(state, input, services.paths)) return false;
        begin(state, actions, input, next, services);
    } else {
        if (actions.field314 != 0x21) return false;
        switch (actions.fields31C[1]) {
        case 1: {
            const int result = approach(state, input, next);
            if (result < 0) {
                if (!services.retainedDisplacement)
                    throw std::runtime_error("Original blocked ladder approach requires retained displacement.");
                next.displacement = *services.retainedDisplacement;
                actions.request(0x1a);
            }
            else if (result > 0) {
                if (state.entrySide == 2) ++state.step;
                else if (state.entrySide == 1) state.step -= 3;
                actions.request(0x1b);
            }
            break;
        }
        case 3:
            if (climb(state, actions, input, next, services)) {
                std::array<std::uint32_t, 12> payload{};
                payload[10] = state.step == 0 ? 2 : 1;
                actions.request(0x1e, &payload);
            }
            break;
        default: animate(state, actions, input, next, services); break;
        }
    }
    output = next;
    return true;
}

playerinteraction::LadderFrame::LadderFrame(const std::vector<std::uint8_t>& executable) : interaction(executable), actions(executable),
    approachSpeed(scalar(executable, glme01::playerDoorApproachSpeed)),
    movementThreshold(scalar(executable, glme01::playerLadderMovementThreshold)),
    turnBlend(scalar(executable, glme01::playerDoorTurnBlend)) {}

playerinteraction::LadderInput playerinteraction::LadderFrame::input(const playeractor::Player& player) const {
    const auto& state = player.state;
    const auto& controls = *player.components->controls;
    LadderInput result;
    result.position = player.position;
    result.motion = state.fieldFB8;
    result.speed = approachSpeed * actions.maximumSpeed(player);
    result.actorType = player.actorType;
    result.radius = state.fieldF54;
    result.reaction = player.components->reactions->state;
    result.animation = player.animationState;
    result.previousFrame = player.animationFrame - player.animationFrameStep;
    result.frame = player.animationFrame;
    result.movementThreshold = movementThreshold;
    result.eligible = (state.flags792 & 2) && state.field1130 == 0 && state.field1338 == 1 &&
        state.field1180 != 1 && state.field1180 != 6 && (state.field1184 == 1 || state.field1180 != state.field1184) &&
        !state.flags1140[2] && !state.field1060 && state.field1170 == 0 && (result.reaction == 1 || result.reaction == 2);
    result.pressed = controls.flagsC[10] != 0;
    result.cancel = controls.flagsC[11] || static_cast<std::int16_t>(player.life) <= 0;
    result.contact = player.field9C >= 0;
    return result;
}

bool playerinteraction::LadderFrame::run(playeractor::Player& player, const LadderServices& services) const {
    LadderOutput output;
    if (!interaction.run(*player.components->ladder, *player.components->actions, input(player), output, services)) return false;
    auto& state = player.state;
    state.fieldF70 = 1;
    state.fieldF74 = output.displacement;
    state.field1070 = 4;
    state.field1080 = {output.rotation[0], output.rotation[1], output.rotation[2]};
    state.field108C = output.rotation[3];
    state.field109C = turnBlend;
    state.fieldF68 *= output.movementScale;
    return true;
}
