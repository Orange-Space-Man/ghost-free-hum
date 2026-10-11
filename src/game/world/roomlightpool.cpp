#include "game/world/roomlightpool.h"

namespace {
    void assign(roomlighting::Entry& entry, std::uint8_t priority, const modellighting::RoomLight& light) {
        entry.priority = priority;
        entry.light = light;
        if (!entry.light.cutoff) entry.light.direction = {0, 0, 1};
    }
}

roomlighting::Entry* roomlighting::Pool::allocate(std::uint8_t priority) {
    if (used < entries.size()) {
        selected = used++;
        return &entries[selected];
    }
    std::uint8_t lowest = 10;
    unsigned candidate = 0;
    for (unsigned i = 1; i < used; ++i) {
        const auto value = entries[i].priority;
        if (value < priority && value < lowest) {
            lowest = value;
            candidate = i;
        }
    }
    if (lowest == 10) return nullptr;
    selected = candidate;
    return &entries[selected];
}

void roomlighting::Pool::beginFrame() {
    used = 0;
    modellighting::RoomLight light;
    light.referenceDistance = light.brightnessPercent = 10;
    add(0, light);
}

bool roomlighting::Pool::add(std::uint8_t priority, const modellighting::RoomLight& light) {
    if (disabled == 1) return false;
    auto* destination = allocate(priority);
    if (!destination) return false;
    assign(*destination, priority, light);
    return true;
}

bool roomlighting::Pool::setPrimary(std::uint8_t priority, const modellighting::RoomLight& light) {
    if (disabled == 1) return false;
    auto primary = light;
    primary.cutoff = 0;
    assign(entries[0], priority, primary);
    return true;
}

std::vector<modellighting::RoomLight> roomlighting::Pool::lights() const {
    std::vector<modellighting::RoomLight> result;
    result.reserve(used);
    for (unsigned i = 0; i < used; ++i) result.push_back(entries[i].light);
    return result;
}

const roomlighting::Entry& roomlighting::Pool::entry(unsigned index) const { return entries.at(index); }
unsigned roomlighting::Pool::count() const { return used; }
unsigned roomlighting::Pool::last() const { return selected; }
