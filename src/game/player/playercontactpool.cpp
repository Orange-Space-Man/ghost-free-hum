#include "game/player/playerresourcepools.h"
#include "game/actors/actorcollisionvolume.h"

void playerresources::RecordPool44::finishContacts() {
    fieldC = field8;
    field8 = 0;
}

bool playerresources::RecordPool44::append(const actorcollision::Volume& volume, const std::array<float, 3>& position) {
    if (field8 >= count) return false;
    records[field8++] = {volume, position};
    return true;
}

bool playerresources::RecordPool44::contact(std::array<float, 3>& displacement, const actorcollision::Volume& volume,
    const std::array<float, 3>& position) const {
    bool hit = false;
    auto corrected = position;
    for (unsigned i = 0; i < field8; ++i) {
        std::array<float, 3> correction{};
        const auto& record = records[i];
        if (!actorcollision::intersect(correction, volume, corrected, record.volume, record.position)) continue;
        for (unsigned axis = 0; axis < 3; ++axis) corrected[axis] += correction[axis];
        hit = true;
    }
    for (unsigned axis = 0; axis < 3; ++axis) displacement[axis] = corrected[axis] - position[axis];
    return hit;
}
