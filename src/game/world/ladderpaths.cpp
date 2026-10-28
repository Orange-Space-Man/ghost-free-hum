#include "game/world/ladderpaths.h"
#include "game/player/playerorientation.h"

#include <cmath>

namespace {
    using ladders::Vector;

    Vector subtract(const Vector& first, const Vector& second) {
        return {first[0] - second[0], first[1] - second[1], first[2] - second[2]};
    }

    float magnitude(const Vector& value) {
        return std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
    }

    Vector cross(const Vector& first, const Vector& second) {
        return {first[1] * second[2] - first[2] * second[1], first[2] * second[0] - first[0] * second[2],
            first[0] * second[1] - first[1] * second[0]};
    }

    void normalize(Vector& value) {
        const float length = magnitude(value);
        if (length >= 0.0001f) {
            const float inverse = 1 / length;
            for (auto& component : value) component *= inverse;
        } else value.fill(0);
    }

    actormodel::Matrix basis(Vector up, Vector forward) {
        auto right = cross(up, forward);
        normalize(right);
        normalize(up);
        normalize(forward);
        return {right[0], up[0], forward[0], 0, right[1], up[1], forward[1], 0,
            right[2], up[2], forward[2], 0};
    }
}

ladders::Paths::Paths(const mission::Resources& mission) : mission(mission) {}

unsigned ladders::Paths::count() const {
    const auto* rails = mission.getJmpResource("RailInfo");
    unsigned result = 0;
    if (rails) for (unsigned row = 0; row < rails->count(); ++row) {
        std::uint32_t use = 0xffffffff;
        rails->getValue(row, "use", use);
        if (use == 1) ++result;
    }
    return result;
}

const jmp::ToolData* ladders::Paths::path(unsigned ladder) const {
    const auto* rails = mission.getJmpResource("RailInfo");
    if (!rails) return nullptr;
    unsigned index = 0;
    for (unsigned row = 0; row < rails->count(); ++row) {
        std::uint32_t use = 0xffffffff;
        rails->getValue(row, "use", use);
        if (use != 1) continue;
        if (index++ != ladder) continue;
        std::string_view name;
        return rails->getValue(row, "name", name) ? mission.getPathResource(name) : nullptr;
    }
    return nullptr;
}

int ladders::Paths::entryCount(unsigned ladder) const {
    const auto* data = path(ladder);
    return data ? static_cast<int>(data->count() >> 1) : -1;
}

bool ladders::Paths::position(unsigned ladder, int index, Vector& output) const {
    const auto* data = path(ladder);
    if (!data || index < 0 || static_cast<unsigned>(index) >= (data->count() >> 1) * 2) return false;
    data->getValue(index, "pnt0_x", output[0]);
    data->getValue(index, "pnt0_y", output[1]);
    data->getValue(index, "pnt0_z", output[2]);
    return true;
}

bool ladders::Paths::center(unsigned ladder, int entry, Vector& output) const {
    Vector first{}, second{};
    if (!position(ladder, entry * 2, first) || !position(ladder, entry * 2 + 1, second)) return false;
    for (unsigned i = 0; i < 3; ++i) output[i] = (first[i] + second[i]) * 0.5f;
    return true;
}

bool ladders::Paths::normal(unsigned ladder, int entry, Vector& output) const {
    const int index = entry * 2;
    Vector first{}, second{}, third{};
    if (!position(ladder, index, first)) return false;
    if (!position(ladder, index + 1, second) || !position(ladder, index + 2, third)) {
        if (!position(ladder, index - 1, second) || !position(ladder, index + 1, third)) return false;
    }
    output = cross(subtract(third, first), subtract(second, first));
    normalize(output);
    return true;
}

bool ladders::Paths::tangent(unsigned ladder, int entry, Vector& output) const {
    Vector first{}, second{};
    if (!position(ladder, entry * 2, first) || !position(ladder, (entry + 1) * 2, second)) return false;
    output = subtract(second, first);
    normalize(output);
    return true;
}

bool ladders::Paths::segmentLength(unsigned ladder, int entry, float& output) const {
    Vector first{}, second{};
    if (!center(ladder, entry, first) || !center(ladder, entry + 1, second)) return false;
    output = magnitude(subtract(second, first));
    return true;
}

float ladders::Paths::length(unsigned ladder) const {
    float result = 0;
    for (int entry = 0; entry < entryCount(ladder) - 1; ++entry) {
        float segment;
        if (!segmentLength(ladder, entry, segment)) return 0;
        result += segment;
    }
    return result;
}

bool ladders::Paths::endpoint(unsigned ladder, bool end, Vector& position, Vector& normal) const {
    const int entry = end ? entryCount(ladder) - 1 : 0;
    return center(ladder, entry, position) && this->normal(ladder, entry, normal);
}

bool ladders::Paths::sample(unsigned ladder, double distance, actormodel::Matrix& output) const {
    const int count = entryCount(ladder);
    float accumulated = 0, fraction = 1;
    int selected = -1;
    for (int entry = 0; entry < count; ++entry) {
        float segment;
        if (!segmentLength(ladder, entry, segment)) break;
        const float end = accumulated + segment;
        if (accumulated <= distance && distance < end) {
            selected = entry;
            fraction = static_cast<float>(distance - accumulated) / segment;
            break;
        }
        accumulated = end;
    }
    if (selected < 0 && distance == length(ladder)) selected = count - 1;
    Vector first{}, second{}, forward{}, up{};
    if (selected < 0 || !center(ladder, selected, first) || !center(ladder, selected + 1, second) ||
        !normal(ladder, selected, forward) || !tangent(ladder, selected, up)) return false;
    auto result = basis(up, forward);
    for (unsigned i = 0; i < 3; ++i) result[i * 4 + 3] = first[i] + (second[i] - first[i]) * fraction;
    output = result;
    return true;
}

bool ladders::Paths::sampleSmoothed(unsigned ladder, double distance, double radius, actormodel::Matrix& output) const {
    const int count = entryCount(ladder);
    int selected = -1, neighbor = -1;
    float accumulated = 0, fraction = 0, blend = 0;
    for (int entry = 0; entry < count; ++entry) {
        float segment;
        if (!segmentLength(ladder, entry, segment)) break;
        const float end = accumulated + segment;
        if (accumulated <= distance && distance < end) {
            selected = neighbor = entry;
            const float offset = static_cast<float>(distance - accumulated);
            fraction = offset / segment;
            if (entry >= 1 && offset <= radius) {
                neighbor = entry - 1;
                blend = 0.5f * (1 - static_cast<float>(offset / radius));
            } else {
                const float remaining = static_cast<float>(end - distance);
                if (entry < count - 2 && remaining <= radius) {
                    neighbor = entry + 1;
                    blend = 0.5f * (1 - static_cast<float>(remaining / radius));
                }
            }
            break;
        }
        accumulated = end;
    }
    if (selected < 0 && distance == length(ladder)) selected = neighbor = count - 1;
    Vector first{}, second{}, forward{}, adjacentForward{}, up{}, adjacentUp{};
    if (selected < 0 || !center(ladder, selected, first) || !center(ladder, selected + 1, second) ||
        !normal(ladder, selected, forward) || !normal(ladder, neighbor, adjacentForward) ||
        !tangent(ladder, selected, up) || !tangent(ladder, neighbor, adjacentUp)) return false;
    auto result = playerorientation::blend(blend, basis(up, forward), basis(adjacentUp, adjacentForward));
    for (unsigned i = 0; i < 3; ++i) result[i * 4 + 3] = first[i] + (second[i] - first[i]) * fraction;
    output = result;
    return true;
}
