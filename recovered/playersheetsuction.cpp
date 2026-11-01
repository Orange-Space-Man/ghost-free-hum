#include "playersheetsuction.h"
#include "resources/dolresources.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    using playerscanner::Vector;

    float scalar(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
    float square(const Vector& value) { return (value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]; }
    Vector subtract(const Vector& a, const Vector& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
    bool within(unsigned value, std::int16_t first, std::int16_t last) {
        return unsigned(int(first)) <= value && value <= unsigned(int(last));
    }
}

playersheets::Suction::Suction(Grid& grid, Motion& motion, const std::vector<std::uint8_t>& executable) :
    previousSpeed(scalar(executable, 0x804dae18)), soundDelay(scalar(executable, 0x804dae1c)), grid(grid), motion(motion),
    queryRadius(scalar(executable, 0x804dae40)), attachedRadiusSquared(scalar(executable, 0x804dae48)),
    detachedRadiusSquared(scalar(executable, 0x804dae44)), attachedForce(scalar(executable, 0x804dae4c)),
    detachedForce(scalar(executable, 0x804dae40)), impactSpeed(scalar(executable, 0x804dae1c)),
    impactRadius(scalar(executable, 0x804dae5c)), soundSpeed(scalar(executable, 0x804dae78)),
    soundScale(scalar(executable, 0x804dae7c)), pullRange(scalar(executable, 0x804dae6c)) {}

void playersheets::Suction::attract(const SuctionOwner* owner, SheetServices& services) {
    auto& points = grid.points();
    if ((!owner || !owner->volume) && !motion.detached) {
        for (auto& point : points) point.attracted = point.touched = false;
        return;
    }
    if (!owner) throw std::runtime_error("Detached cloth suction requires the original player owner.");
    const auto& layout = grid.layout();
    const auto& border = layout.border;
    unsigned previousCount = 0, count = 0;
    for (auto& point : points) {
        point.attracted = false;
        if (point.touched || !point.active) ++previousCount;
    }
    for (unsigned i = 0; i < points.size(); ++i) {
        auto& point = points[i];
        if (!point.active) { ++count; continue; }
        Vector queryDirection{};
        if (!motion.detached && (point.edge || (layout.splitRows && point.weight <= 0.5f) ||
            !owner->volume->intersects(queryRadius, queryDirection, point.position))) continue;
        point.attracted = true;
        const float distance = square(subtract(point.position, owner->position));
        if (!(distance < (motion.detached ? detachedRadiusSquared : attachedRadiusSquared))) continue;
        const unsigned x = i % layout.width, y = i / layout.width;
        const bool insideX = within(x, border[0], border[2]), insideY = within(y, border[1], border[3]);
        if (insideY) {
            if (x < unsigned(int(border[0]))) {
                for (unsigned column = x; column < unsigned(int(border[0])); ++column)
                    points.at(y * layout.width + column).attracted = true;
            } else if (unsigned(int(border[2])) < x) {
                for (unsigned column = unsigned(int(border[2])) + 1; column <= x; ++column)
                    points.at(y * layout.width + column).attracted = true;
            }
        } else if (insideX) {
            if (y < unsigned(int(border[1]))) {
                for (unsigned row = y; row < unsigned(int(border[1])); ++row)
                    points.at(row * layout.width + x).attracted = true;
            } else if (unsigned(int(border[3])) < y) {
                for (unsigned row = unsigned(int(border[3])) + 1; row <= y; ++row)
                    points.at(row * layout.width + x).attracted = true;
            }
        }
        if (motion.detached || (insideX != insideY) || layout.splitRows) { point.touched = true; ++count; }
    }
    for (auto& point : points) {
        if (!point.active || !point.attracted) continue;
        auto direction = subtract(owner->position, point.position);
        const float distance = std::sqrt(square(direction));
        if (distance >= 0.0001f) for (auto& value : direction) value *= 1 / distance;
        else direction.fill(0);
        const float force = motion.detached ? detachedForce : attachedForce;
        if (!motion.detached || distance >= 2 * force) {
            for (unsigned axis = 0; axis < 3; ++axis) point.velocity[axis] += direction[axis] * force;
        } else {
            for (unsigned axis = 0; axis < 3; ++axis) point.velocity[axis] = direction[axis] * distance;
        }
    }
    if (!previousCount && count) services.sound(0x41, owner->position);
    if (motion.detached && count >= (points.size() >> 2) * 3) {
        playerevents::Payload payload;
        payload.word(0x60, 7);
        services.completion(payload);
        services.completionEffect(0, owner->position, owner->direction);
        services.completionEffect(1, owner->position, owner->direction);
        for (auto& point : points) point.active = false;
        completed = true;
    }
}

void playersheets::Suction::requestRetraction(bool hasOwner) {
    if (!hasOwner || motion.detached || motion.pulling || motion.retracting) return;
    const auto& layout = grid.layout();
    const auto& border = layout.border;
    float xSum = 0, ySum = 0;
    unsigned count = 0;
    for (unsigned i = 0; i < grid.points().size(); ++i) if (grid.points()[i].touched) {
        xSum += (float(i % layout.width) - border[0]) / float(border[2] - border[0]);
        ySum += (float(i / layout.width) - border[1]) / float(border[3] - border[1]);
        ++count;
    }
    if (!count) return;
    const float inverse = 1 / float(count);
    const float x = xSum * inverse, y = ySum * inverse;
    const bool insideX = (0 <= x && x <= 1) || (border[0] == 0 && int(border[2]) == int(layout.width) - 1);
    const bool insideY = (0 <= y && y <= 1) || (border[1] == 0 && int(border[3]) == int(layout.height) - 1);
    if (insideX && insideY) motion.retracting = true;
    else {
        motion.pulling = releaseStarted = true;
        const auto sign = [](float value) { return value > 0 ? 1.0f : value < 0 ? -1.0f : 0.0f; };
        motion.pullDirection = {insideX ? 0 : sign(x - 0.5f), insideY ? 0 : sign(y - 0.5f)};
    }
}

void playersheets::Suction::updateFeedback(bool hasPlayer, bool hasVolume, bool playerHolding, bool playerPulling,
    SheetServices& services) {
    updateFeedback(hasPlayer, hasVolume, [playerHolding]() { return playerHolding; },
        [playerPulling]() { return playerPulling; }, services);
}

void playersheets::Suction::updateFeedback(bool hasPlayer, bool hasVolume, const std::function<bool()>& playerHolding,
    const std::function<bool()>& playerPulling, SheetServices& services) {
    if (!hasPlayer) return;
    const auto& layout = grid.layout();
    const auto& border = layout.border;
    Vector center{};
    unsigned count = 0;
    bool release = false;
    if (!motion.detached && !completed) {
        for (unsigned y = 0; y < layout.height; ++y) for (unsigned x = 0; x < layout.width; ++x) {
            const auto& point = grid.points()[y * layout.width + x];
            if (!point.touched) continue;
            const Vector* source = &point.anchor;
            if (!layout.splitRows) {
                const unsigned column = x < unsigned(border[0]) ? unsigned(border[0]) :
                    x > unsigned(int(border[2])) ? unsigned(int(border[2])) : x;
                const unsigned row = y < unsigned(border[1]) ? unsigned(border[1]) :
                    y > unsigned(int(border[3])) ? unsigned(int(border[3])) : y;
                source = &grid.points().at(row * layout.width + column).position;
            }
            for (unsigned axis = 0; axis < 3; ++axis) center[axis] += (*source)[axis];
            ++count;
        }
    }
    if (!count) {
        release = playerHolding();
        pullFrames = 0;
        releaseStarted = false;
    } else {
        const float scale = 1 / float(count);
        for (unsigned axis = 0; axis < 3; ++axis) captureCenter[axis] = center[axis] * scale;
        if (!services.requestPull(grid, captureCenter, pullRange + layout.range, layout.elasticity)) {
            for (auto& point : grid.points()) point.touched = false;
        } else {
            if (!playerPulling() && !releaseStarted && !motion.halfway) pullFrames = 0;
            else {
                pullFrames += 1;
                if (float(static_cast<std::int32_t>(layout.lifetime)) < pullFrames || releaseStarted || motion.halfway) {
                    if (layout.elasticity == 0) {
                        requestRetraction(hasVolume);
                        release = true;
                    } else release = services.elasticReaction(captureCenter, 5, 6);
                }
            }
            if (layout.elasticity > 0) impactReady = true;
        }
    }
    if (release) services.releasePull(grid);
}

void playersheets::Suction::collide(SheetServices& services, bool hasSecondaryTexture) {
    if (motion.detached || completed) return;
    bool impact = false;
    for (unsigned i = 0; i < grid.points().size(); ++i) {
        const auto* plane = motion.collisionPlane(i);
        if (!plane) continue;
        auto& point = grid.points()[i];
        const float response = motion.collide(point, *plane, grid.layout().elasticity > 0);
        if (response > 0) {
            point.touched = false;
            if (impactReady && response > impactSpeed) impact = true;
        }
    }
    if (impact) {
        services.impact(impactRadius, captureCenter, 0x80);
        if (hasSecondaryTexture) services.secondaryTexture();
        impactReady = false;
    }
}

void playersheets::Suction::updateSound(SheetServices& services) {
    float speed = 0, count = 0;
    if (!motion.detached && !completed) {
        for (const auto& point : grid.points()) {
            const float value = std::sqrt(square(point.velocity));
            if (value > soundSpeed) {
                speed += value;
                if (point.attracted) speed += value;
                count += 1;
            }
        }
        if (count > 0) speed /= count;
    }
    const float change = speed - previousSpeed;
    if (change > 0 && soundDelay == 0) {
        const auto& layout = grid.layout();
        const auto& border = layout.border;
        const bool full = border[0] == 0 && border[1] == 0 &&
            int(border[2]) == int(layout.width) - 1 && int(border[3]) == int(layout.height) - 1;
        const auto& matrix = grid.matrix();
        const float volume = change * soundScale;
        services.rustle(full ? 0x18ba : 0x1812, {matrix[3], matrix[7], matrix[11]}, volume < 1 ? volume : 1);
    }
    previousSpeed = speed;
    if (soundDelay > 0) { soundDelay -= 1; if (soundDelay < 0) soundDelay = 0; }
}
