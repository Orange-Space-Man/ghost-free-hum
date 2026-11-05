#include "playerscannerinteraction.h"

#include <cmath>

namespace {
    using playerscanner::Vector;

    float magnitude(const Vector& value) {
        return std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
    }

    Vector transform(const actormodel::Matrix& matrix, const Vector& value) {
        Vector result;
        for (unsigned i = 0; i < 3; ++i) result[i] =
            ((matrix[i * 4] * value[0] + matrix[i * 4 + 1] * value[1]) + matrix[i * 4 + 2] * value[2]) + matrix[i * 4 + 3];
        return result;
    }
}

bool playerscanner::intersect(const FurnitureTarget& target, const Vector& position, const Vector& direction, float& distance) {
    auto point = transform(target.inverse, position);
    auto ray = transform(target.inverse, direction);
    for (unsigned i = 0; i < 3; ++i) ray[i] -= target.inverse[i * 4 + 3];
    const auto& extent = target.extent;
    bool inside = true;
    for (unsigned i = 0; i < 3; ++i)
        if (!(-extent[i] <= point[i] && point[i] <= extent[i])) inside = false;
    if (!inside) {
        for (unsigned i = 0; i < 3; ++i) {
            if ((point[i] <= -extent[i] || 0 <= ray[i]) && (extent[i] <= point[i] || ray[i] <= 0)) return false;
        }
        bool hit = false;
        for (unsigned axis = 0; axis < 3 && !hit; ++axis) {
            const unsigned first = axis == 0 ? 1 : 0;
            const unsigned second = axis == 2 ? 1 : 2;
            const float slopeFirst = ray[first] * (1 / ray[axis]);
            const float slopeSecond = ray[second] * (1 / ray[axis]);
            for (unsigned side = 0; side < 2 && !hit; ++side) {
                const float offset = (side ? -extent[axis] : extent[axis]) - point[axis];
                const float a = point[first] + slopeFirst * offset;
                const float b = point[second] + slopeSecond * offset;
                if (axis == 2 && side == 1) {
                    point[0] = a;
                    point[1] = b;
                    hit = -extent[2] <= a && a <= extent[2] && -extent[1] <= b && b <= extent[1];
                } else {
                    hit = !(a < -extent[first] || extent[first] < a || b < -extent[second] || extent[second] < b);
                }
            }
        }
        if (!hit) return false;
    }
    distance = magnitude(point);
    return true;
}

playerscanner::Hit playerscanner::furnitureTarget(const std::vector<FurnitureTarget>& targets,
    const Vector& position, const Vector& direction, float maximum) {
    Hit result;
    for (unsigned i = 0; i < targets.size(); ++i) {
        float distance;
        if (targets[i].enabled && intersect(targets[i], position, direction, distance) && distance < maximum) {
            result = {static_cast<int>(i), distance};
            maximum = distance;
        }
    }
    return result;
}

playerscanner::Hit playerscanner::actorTarget(const std::vector<ActorTarget>& targets,
    const Vector& position, const Vector& direction, float maximum) {
    Hit result;
    for (const auto& target : targets) {
        if (!target.active || !target.scannerType || !target.hasOwner || !target.eligible || !target.visible) continue;
        Vector delta;
        for (unsigned i = 0; i < 3; ++i) delta[i] = target.position[i] - position[i];
        const float along = (direction[0] * delta[0] + direction[1] * delta[1]) + direction[2] * delta[2];
        if (!(0 < along)) continue;
        Vector perpendicular;
        for (unsigned i = 0; i < 3; ++i) perpendicular[i] = delta[i] - direction[i] * along;
        if (!(magnitude(perpendicular) < target.radius)) continue;
        const float distance = magnitude(delta);
        if (distance < maximum) {
            result = {target.owner, distance};
            maximum = distance;
        }
    }
    return result;
}

bool playerscanner::update(playermodels::ScannerModels& state, const Input& input, bool& warp, Services& services) {
    state.camera = &input.camera;
    bool requestedWarp = false;
    if (!state.state[0]) {
        if (input.pressed) {
            state.state[0] = 1;
            state.state[1] = 0;
            state.field10 = static_cast<float>(services.delay());
            services.playScanSound(0x3813);
        }
    } else {
        state.field10 -= 1.2f;
        if (!state.state[1] && state.field10 <= 0) {
            Vector direction;
            for (unsigned i = 0; i < 3; ++i) direction[i] = input.camera.target[i] - input.camera.position[i];
            const float length = magnitude(direction);
            if (length >= 0.0001f) for (auto& axis : direction) axis *= 1 / length;
            else direction.fill(0);
            auto furniture = services.furniture(input.camera.position, direction);
            auto actor = services.actor(input.camera.position, direction);
            if (furniture.index >= 0 && actor.index >= 0) {
                if (furniture.distance <= actor.distance) actor.index = -1;
                else furniture.index = -1;
            }
            state.field8 = static_cast<std::uint32_t>(furniture.index);
            state.fieldC = static_cast<std::uint32_t>(actor.index);
            if (furniture.index >= 0) {
                state.state[1] = 1;
                services.markFurniture(furniture.index);
                if (services.mirror(furniture.index)) requestedWarp = true;
                else if (services.furnitureMessage(furniture.index)) services.openFurnitureMessage();
            } else if (actor.index >= 0) {
                state.state[1] = 1;
                const auto type = services.actorType(actor.index);
                const auto message = type == 0x31 ? std::optional<unsigned>(0x1a) :
                    type == 0x32 ? std::optional<unsigned>(0x1b) : services.actorMessage(actor.index);
                if (message) {
                    services.setDiscoveryFlag(0xad);
                    services.selectMessage(0, *message & 255);
                    services.openMessage();
                }
            }
        }
        if (state.field10 <= 0 || input.cancel) {
            state.field10 = 0;
            state.state[0] = 0;
        }
    }
    const bool active = state.state[0] || requestedWarp;
    if (active) warp = requestedWarp;
    state.state[2] = 0;
    return active;
}
