#include "game/player/playerresourcepools.h"

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
