#include "game/player/playerresourcepools.h"
#include "game/world/roomlightpool.h"

#include <algorithm>
#include <stdexcept>

playerresources::PointPool::PointPool(unsigned count) : count(count), records(new PointRecord[count]), last(nullptr) {
    if (!count) throw std::runtime_error("Original point pool requires at least one record.");
    last = records.get() + count - 1;
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
