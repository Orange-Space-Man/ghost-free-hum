#include "roomfurnitureswing.h"
#include "resources/dolresources.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    template<class T> T constant(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, sizeof(T), bytes, error)) throw std::runtime_error(error);
        std::reverse(bytes.begin(), bytes.end());
        T value;
        std::memcpy(&value, bytes.data(), sizeof(value));
        return value;
    }
}

roomfurniture::Swing::Swing(const std::vector<std::uint8_t>& executable) : pose(executable) {
    constexpr unsigned springTable = 0x804da1a8, soundTable = 0x8038eb8c;
    for (unsigned i = 0; i < spring.size(); ++i) spring[i] = constant<float>(executable, springTable + i * 4);
    for (unsigned i = 0; i < sounds.size(); ++i) sounds[i] = constant<unsigned>(executable, soundTable + i * 4);
    quantization = constant<float>(executable, 0x804d9a7c);
    inverseQuantization = constant<float>(executable, 0x804d9a78);
    strengthScale = constant<float>(executable, 0x804da150);
    directionScale = constant<float>(executable, 0x804da160);
    radius = constant<float>(executable, 0x804da170);
    zero = constant<float>(executable, 0x804da130);
    one = constant<float>(executable, 0x804da158);
    rootThreshold = constant<float>(executable, 0x804d9a88);
    half = constant<double>(executable, 0x804d9a90);
    three = constant<double>(executable, 0x804d9a98);
    soundMaximum = constant<float>(executable, 0x804dd12c);
    soundMinimum = constant<float>(executable, 0x804dd130);
    pitchScale = constant<float>(executable, 0x804dd134);
    pitchThreshold = constant<float>(executable, 0x804dd138);
    volumeScale = constant<float>(executable, 0x804dd13c);
    activeMode = constant<float>(executable, 0x804da1fc);
    delayLimit = constant<float>(executable, 0x804da154);
    reactionFrame = constant<float>(executable, 0x804da1c8);
    cameraReleaseFrame = constant<float>(executable, 0x804da22c);
    effectOffset = constant<float>(executable, 0x804da224);
    farDistance = constant<float>(executable, 0x804da228);
    nearDistance = constant<float>(executable, 0x804da230);
    randomThreshold = constant<double>(executable, 0x804da238);
    for (unsigned axis = 0; axis < 3; ++axis) {
        jointOffset[axis] = constant<float>(executable, 0x8020c0d4 + axis * 4);
        reactionDirection[axis] = constant<float>(executable, 0x8020c0e0 + axis * 4);
    }
    reactionParameters = {constant<float>(executable, 0x804da1e4), strengthScale};
}

roomfurniture::SwingSound roomfurniture::Swing::sound(const std::array<float, 2>& velocity, float currentPitch) const {
    const float speed = static_cast<float>(double(std::min(std::max(std::abs(velocity[0]), std::abs(velocity[1])), soundMaximum)) /
        double(soundMaximum));
    SwingSound result;
    result.stop = speed <= soundMinimum;
    if (result.stop) return result;
    result.pitch = static_cast<float>(double(pitchScale) + double(static_cast<float>(double(pitchScale) * speed)));
    result.changePitch = pitchThreshold < std::abs(static_cast<float>(double(currentPitch) - result.pitch));
    result.volume = volumeScale * static_cast<float>(double(speed) - soundMinimum);
    return result;
}

std::array<float, 2> roomfurniture::Swing::angles(const Vector& direction) const {
    const auto roll = static_cast<std::int16_t>(pose.angle(direction[1], -double(direction[0])));
    double squared = direction[0] * direction[0] + direction[1] * direction[1];
    if (rootThreshold < squared) {
        double root = 1.0 / std::sqrt(squared);
        root = half * root * (three - squared * root * root);
        root = half * root * (three - squared * root * root);
        squared = static_cast<float>(squared * half * root * (three - squared * root * root));
    }
    return {static_cast<float>(static_cast<std::int16_t>(pose.angle(squared, direction[2]))), static_cast<float>(roll)};
}

void roomfurniture::Swing::approach(float target, float stiffness, float damping, float& position, float& velocity) const {
    target = inverseQuantization * static_cast<float>(static_cast<int>(double(quantization) * target));
    position = inverseQuantization * static_cast<float>(static_cast<int>(double(quantization) * position));
    velocity = static_cast<float>(double(damping) * (velocity + static_cast<float>(-double(stiffness) * (position - target))));
    position += velocity;
}

void roomfurniture::Swing::update(Object& object, RowState& row, SwingState& state, bool visible, bool vacuumActive,
    const Response& response, const ResponseServices& responses, const SwingServices& services) const {
    object.flags = visible ? object.flags | 1 : object.flags & ~1u;
    if (!visible) return;
    auto point = object.position;
    point[1] -= static_cast<float>(object.itemOffset);
    Vector direction{};
    const int strength = vacuumActive ? static_cast<int>(strengthScale * services.suction(radius, point, direction)) : 0;
    if (!strength) {
        state.target = {zero, zero};
        object.suctionTicks = 0;
    } else {
        for (unsigned axis = 0; axis < 3; ++axis)
            direction[axis] = directionScale * ((point[axis] + static_cast<float>(strength) * direction[axis]) - object.position[axis]);
        state.target = angles(direction);
        ++object.suctionTicks;
    }
    if (object.arguments[0] == 0 || object.arguments[0] == 1) {
        const auto index = object.arguments[0] * 2;
        for (unsigned axis = 0; axis < 2; ++axis)
            approach(state.target[axis], spring[index], spring[index + 1], object.swing[axis], object.swingVelocity[axis]);
    }
    if (one < std::abs(object.swingVelocity[0]) || one < std::abs(object.swingVelocity[1])) {
        const auto sound = object.sound & 255;
        services.sound(sounds[sound > 25 ? 0 : sound], object.position, object.swingVelocity);
    }
    object.angles[0] = static_cast<int>(object.swing[0]);
    object.angles[2] = static_cast<int>(object.swing[1]);
    state.previousVelocity = object.swingVelocity;
    const int threshold = object.arguments[0] == 0 ? 2500 : 1800;
    if ((threshold < std::abs(object.angles[0]) || threshold < std::abs(object.angles[2])) && state.collected == zero) {
        response.collect(object, row, true, responses);
        response.occupant(object, row, true, responses);
        state.collected = one;
    }
    if (std::abs(object.angles[0]) < 100 && std::abs(object.angles[2]) < 100 &&
        std::abs(object.swingVelocity[0]) < strengthScale && std::abs(object.swingVelocity[1]) < strengthScale)
        state.collected = zero;
    if (row.flags & 0x20) {
        responses.scanMessage(object.counter & 255);
        row.flags &= ~0x20u;
    }
}

void roomfurniture::Swing::animate(Object& object, AnimatedSwingState& state, const ResponseServices& responses,
    const AnimatedSwingServices& services) const {
    const auto point = services.jointPoint(object, jointOffset);
    if (reactionFrame <= object.frame) services.actorReaction(point, reactionParameters, reactionDirection);
    else {
        const float x = services.playerPosition[0] - point[0], y = services.playerPosition[1] - point[1];
        const float z = services.playerPosition[2] - point[2];
        if (z * z + x * x + y * y < farDistance) {
            services.playerDamage(point, 10, 6);
            state.hit = one;
        }
    }
    if (object.frame == cameraReleaseFrame) services.camera(false);
    const auto frame = static_cast<int>(object.frame);
    if (frame == 75) {
        auto effectPoint = point;
        effectPoint[1] -= effectOffset;
        responses.effect(0x66, effectPoint);
    }
    if (frame == 110) {
        services.globalSound(state.hit == one ? 0x4820 : 0x4822);
        state.hit = zero;
    }
}

void roomfurniture::Swing::updateAnimated(Object& object, RowState& row, AnimatedSwingState& state, bool visible, bool vacuumActive,
    const Response& response, const ResponseServices& responses, const SwingServices& services,
    const AnimatedSwingServices& animation) const {
    if (state.mode == activeMode) {
        animate(object, state, responses, animation);
        object.frame += one;
        if (static_cast<int>(object.frame) == static_cast<int>(animation.animationEnd(object))) {
            state.mode = zero;
            responses.animation(object, 0);
        }
    } else {
        update(object, row, state.swing, visible, vacuumActive, response, responses, services);
        if (state.mode == zero && state.proximity == one) {
            state.mode = one;
            state.proximity = activeMode;
            animation.camera(true);
            animation.playerAction(0x55);
        }
        if (state.mode == one) {
            state.delay += one;
            if (delayLimit < state.delay) {
                state.mode = activeMode;
                responses.animation(object, 1);
                object.angles[0] = object.angles[2] = 0;
                state.delay = zero;
            }
        }
    }
    const float x = object.position[0] - animation.playerPosition[0], z = object.position[2] - animation.playerPosition[2];
    const double distance = x * x + z * z;
    if (animation.enabled && !animation.messageBusy() && state.proximity == zero && state.mode == zero && !animation.scanner) {
        if (distance < nearDistance) state.proximity = randomThreshold <= animation.random() ? activeMode : one;
    } else if (farDistance < distance && state.proximity == activeMode) state.proximity = zero;
    if (row.flags & 0x20) {
        responses.scanMessage(object.counter & 255);
        row.flags &= ~0x20u;
    }
}
