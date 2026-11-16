#include "game/player/playerresourcepools.h"
#include "game/world/roomlightpool.h"
#include "game/world/mapcollisionstate.h"
#include "game/actors/actorinitialpose.h"
#include "resources/dolresources.h"

#include <algorithm>
#include <stdexcept>
#include <cstring>

playerresources::PointSettings playerresources::PointSettings::read(const std::vector<std::uint8_t>& executable) {
    constexpr unsigned pointSettingsAddress = 0x804d20f0;
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, pointSettingsAddress, 20, bytes, error)) throw std::runtime_error(error);
    std::array<std::uint32_t, 5> words{};
    for (unsigned i = 0; i < words.size(); ++i) {
        words[i] = (unsigned(bytes[i * 4]) << 24) | (unsigned(bytes[i * 4 + 1]) << 16) |
            (unsigned(bytes[i * 4 + 2]) << 8) | bytes[i * 4 + 3];
    }
    PointSettings result;
    std::memcpy(&result.gravity, &words[0], sizeof(float));
    std::memcpy(&result.damping, &words[1], sizeof(float));
    std::memcpy(&result.bounce, &words[2], sizeof(float));
    std::memcpy(&result.lifetime, &words[3], sizeof(std::int32_t));
    result.collision = words[4] != 0;
    return result;
}

playerresources::PointPool::PointPool(unsigned count) : count(count), records(new PointRecord[count]), last(nullptr) {
    if (!count) throw std::runtime_error("Original point pool requires at least one record.");
    last = records.get() + count - 1;
}

void playerresources::PointPool::emit(unsigned requested, float speed, const std::array<float, 3>& position,
    const std::array<float, 3>* velocity, const PointSettings& settings,
    const actormodel::InitialPose& pose, const std::function<std::uint32_t()>& random) {
    for (unsigned i = 0; i < count && requested; ++i) {
        auto& point = records[i];
        if (!(point.lifetime <= 0)) continue;
        if (!random) throw std::runtime_error("Point emission requires the original random stream.");
        point.position = position;
        const auto first = pose.horizontalDirection(static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(float(random()) * (1.0f / 65536.0f))));
        const auto second = pose.horizontalDirection(static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(float(random()) * (1.0f / 65536.0f))));
        point.velocity = {first[2] * second[2], first[0], first[2] * second[0]};
        for (unsigned axis = 0; axis < 3; ++axis) {
            point.velocity[axis] *= speed;
            if (velocity) point.velocity[axis] += (*velocity)[axis];
        }
        const float range = (1.0f / 4294967296.0f) * float(settings.lifetime);
        point.lifetime = float(static_cast<int>(1.0f + range * float(random())));
        --requested;
    }
}

void playerresources::PointPool::update(const PointSettings& settings,
    const std::function<mapcollision::SphereContact(const std::array<float, 3>&,
        const std::array<float, 3>&)>& collision) {
    const float damping = float(1.0 - double(float(1.0 - double(settings.damping))));
    for (unsigned i = 0; i < count; ++i) {
        auto& point = records[i];
        if (!(point.lifetime > 0)) continue;
        if (settings.collision && !collision) throw std::runtime_error("Point update requires room collision.");
        for (auto& value : point.velocity) value *= damping;
        point.velocity[1] += settings.gravity;
        auto next = point.position;
        for (unsigned axis = 0; axis < 3; ++axis) next[axis] += point.velocity[axis];
        if (settings.collision) {
            const auto contact = collision(next, point.position);
            if (contact.floor >= 0 || contact.wall >= 0) {
                const auto& normal = contact.floor >= 0 ? contact.floorNormal : contact.wallNormal;
                const float dot = point.velocity[0] * normal[0] + point.velocity[1] * normal[1] + point.velocity[2] * normal[2];
                const float response = float(double(settings.bounce) * double(dot));
                for (unsigned axis = 0; axis < 3; ++axis) {
                    next[axis] += contact.correction[axis];
                    point.velocity[axis] += normal[axis] * response;
                }
            }
        } else if (next[1] <= 0) {
            next[1] = 0;
            point.velocity[1] += float(double(point.velocity[1]) * double(settings.bounce));
        }
        point.position = next;
        point.lifetime -= 1;
    }
}

void playerresources::PointPool::draw(const PointSettings& settings,
    const std::function<void(const PointLine&)>& line) const {
    const double scale = 1.0f / float(settings.lifetime);
    for (unsigned i = 0; i < count; ++i) {
        const auto& point = records[i];
        if (!(point.lifetime > 0 && point.lifetime < float(settings.lifetime))) continue;
        if (!line) throw std::runtime_error("Point drawing requires a line renderer.");
        PointLine output;
        output.end = point.position;
        for (unsigned axis = 0; axis < 3; ++axis) output.start[axis] = point.position[axis] - point.velocity[axis];
        output.color = {255, static_cast<std::uint8_t>(static_cast<int>(127.0f + float(double(127.0f * point.lifetime) * scale))),
            127, static_cast<std::uint8_t>(static_cast<int>(double(255.0f * point.lifetime) * scale))};
        line(output);
    }
}

playerresources::EffectRecord80::EffectRecord80() : field7C(0) {}

playerresources::EffectRecord20::EffectRecord20() : field18(0) {}

playerresources::EffectPool80::EffectPool80(unsigned count) : count(count), records(new EffectRecord80[count]) {}

playerresources::EffectPool20::EffectPool20(unsigned count) : count(count), records(new EffectRecord20[count]) {}

playerresources::RecordPool44::RecordPool44(unsigned count) : count(count), records(new Record[count]) {}

playerresources::CurvePoints::CurvePoints(unsigned count) {
    if (!(count & 1)) ++count;
    count = std::max(count, 3u);
    points.resize(count);
}

playerresources::CurveOwner::CurveOwner() {
    primary = std::make_unique<CurvePoints>(32);
    for (auto& curve : secondary) curve = std::make_unique<CurvePoints>(32);
}

playerresources::CurveOwner::~CurveOwner() {
    for (auto& curve : secondary) curve.reset();
    primary.reset();
}

void playerresources::LightPulse::start(const std::array<float, 3>& origin, std::uint32_t rgba,
    std::uint16_t range, std::uint16_t intensity, std::uint32_t frames) {
    position = origin;
    color = rgba;
    distance = range;
    brightness = intensity;
    remaining = duration = static_cast<float>(frames);
}

void playerresources::LightPulse::update(roomlighting::Pool& lights) {
    if (!(remaining > 0)) return;
    const auto range = static_cast<std::uint16_t>(static_cast<std::uint32_t>(float(distance) * (remaining / duration)));
    if (range) {
        modellighting::RoomLight light;
        light.position = position;
        for (unsigned i = 0; i < 4; ++i) light.color[i] = static_cast<std::uint8_t>(color >> (24 - i * 8));
        light.referenceDistance = range;
        light.brightnessPercent = brightness;
        lights.add(1, light);
    }
    remaining -= 1;
}
