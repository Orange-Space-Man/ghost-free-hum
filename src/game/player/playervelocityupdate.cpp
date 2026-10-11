#include "game/player/playervelocityupdate.h"

#include <cmath>
#include <stdexcept>

namespace {
    using Vector = playermotion::Vector;

    float dot(const Vector& first, const Vector& second) {
        return std::fma(first[0], second[0], first[1] * second[1]) + first[2] * second[2];
    }

    Vector cross(const Vector& first, const Vector& second) {
        return {first[1] * second[2] - first[2] * second[1], first[2] * second[0] - first[0] * second[2],
            first[0] * second[1] - first[1] * second[0]};
    }

    Vector normalize(Vector value) {
        const float length = std::sqrt(dot(value, value));
        if (length >= 0.0001f) {
            const float inverse = 1 / length;
            for (auto& component : value) component *= inverse;
        } else value.fill(0);
        return value;
    }

    bool normal(const mapcollision::State& collision, std::uint16_t polygon, Vector& value) {
        if (polygon >= 0xfffe) {
            value = {0, 1, 0};
            return polygon != 0xffff;
        }
        return collision.polygonNormal(polygon, value);
    }

    bool ladderWall(const mapcollision::State& collision, std::uint16_t polygon) {
        if (polygon == 0xffff) return false;
        const auto* table = collision.polygonInfo();
        if (!table) throw std::runtime_error("Player wall response requires PolygonInfo.");
        std::uint8_t ladder = 0;
        Vector value;
        return table->getValue(polygon, "hashigo", ladder) && ladder &&
            collision.polygonNormal(polygon, value) && value[1] == 0;
    }

    float reflect(Vector& velocity, const mapcollision::State& collision, std::uint16_t polygon, float factor) {
        Vector value;
        if (!normal(collision, polygon, value)) return 0;
        const float amount = factor * dot(value, velocity);
        if (!(amount > 0)) return 0;
        for (unsigned i = 0; i < 3; ++i) velocity[i] += value[i] * amount;
        return amount;
    }
}

playermotion::Vector playermotion::updateVelocity(const Vector& velocity, const VelocityInput& input,
    const mapcollision::State& collision, const std::function<void(std::uint32_t, const Vector&)>& sound) {
    auto result = velocity;
    const auto emit = [&](std::uint32_t id) {
        if (!sound) throw std::runtime_error("Player contact response requires positional sound playback.");
        sound(id, input.position);
    };
    if (input.applyReflection) {
        if (reflect(result, collision, input.floor, input.reflection) > 4 && input.emitSound) emit(0x845);
        if (!ladderWall(collision, input.wall) &&
            reflect(result, collision, input.wall, input.reflection) > 10 && input.emitSound) emit(0x844);
    }
    if (input.applyDrag) {
        const float factor = 1 - (1 - input.drag);
        for (auto& value : result) value *= factor;
    }
    if (input.applyFriction) {
        const float friction = input.motion && input.motion->value >= input.threshold ? input.movingFriction : input.idleFriction;
        Vector value;
        if (normal(collision, input.floor, value)) {
            const float projection = dot(value, result);
            const float factor = (1 - (1 - friction)) - 1;
            for (unsigned i = 0; i < 3; ++i) result[i] += (result[i] - value[i] * projection) * factor;
        }
    }
    if (input.applyGravity) result[1] += input.inverted ? -input.gravity : input.gravity;
    if (input.applyLadderGravity && input.floor == 0xffff && collision.ladderContact(input.probe, input.position))
        result[1] += input.inverted ? -input.ladderGravity : input.ladderGravity;
    if (input.applyMotion && input.motion && input.motion->value >= input.threshold) {
        Vector value;
        normal(collision, input.floor, value);
        auto direction = normalize(cross(cross(value, {input.motion->second[0], 0, input.motion->second[2]}), value));
        float amount = input.motion->value * input.acceleration;
        if (dot(result, direction) < 0) amount *= input.reversal;
        amount *= std::abs(value[1]);
        for (unsigned i = 0; i < 3; ++i) result[i] += direction[i] * amount;
    }
    if (input.applyDirection && input.direction && input.directionalForce != 0) {
        Vector value;
        normal(collision, input.floor, value);
        const auto direction = normalize(cross(cross(value, {(*input.direction)[0], 0, (*input.direction)[2]}), value));
        for (unsigned i = 0; i < 3; ++i) result[i] += direction[i] * input.directionalForce;
    }
    const float length = std::sqrt(dot(result, result));
    if (length > input.maximumSpeed) {
        const float factor = input.maximumSpeed / length;
        result[0] *= factor;
        result[2] *= factor;
    }
    return result;
}
