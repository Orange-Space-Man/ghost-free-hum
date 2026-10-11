#include "game/world/cobwebplacementmanager.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
    bool matches(const jmp::ToolDataRef& entry) {
        std::string_view name;
        if (!entry.table || entry.row < 0 || !entry.table->getValue(entry.row, "name", name)) {
            throw std::runtime_error("Original cobweb placement requires its name field.");
        }
        return name == "net";
    }

    std::int16_t angle(float value) {
        if (value < 0) value += 180;
        value *= 182.0416717529296875f;
        const auto integer = !std::isfinite(value) || value < -2147483648.0 || value >= 2147483648.0 ?
            std::numeric_limits<std::int32_t>::min() : static_cast<std::int32_t>(value);
        return static_cast<std::int16_t>(integer);
    }
}

modeleffects::CobwebPlacements::CobwebPlacements(Factory& factory, const actormodel::InitialPose& pose)
    : factory(factory), pose(pose) { initialize(); }

void modeleffects::CobwebPlacements::initialize() {
    clear();
    for (auto& record : records) {
        record = {};
        free.push_back(&record);
    }
}

void modeleffects::CobwebPlacements::clear() {
    active.clear();
    free.clear();
}

bool modeleffects::CobwebPlacements::create(jmp::ToolDataRef& entry) {
    if (!matches(entry) || free.empty()) return false;
    auto* record = free.front();
    free.pop_front();
    record->owner = nullptr;
    record->row = entry.row;
    const std::array<const char*, 6> names{"pos_x", "pos_y", "pos_z", "dir_x", "dir_y", "dir_z"};
    std::array<float, 6> values;
    for (unsigned i = 0; i < names.size(); ++i) {
        if (!entry.table->getValue(entry.row, names[i], values[i])) {
            throw std::runtime_error("Original cobweb placement reads an unwritten coordinate.");
        }
    }
    auto matrix = pose.rotation({angle(values[3]), angle(values[4]), angle(values[5])});
    for (unsigned i = 0; i < 3; ++i) matrix[i * 4 + 3] = values[i];
    record->owner = createCobweb(factory, matrix);
    if (!record->owner) {
        *record = {};
        free.push_back(record);
        return false;
    }
    active.push_back(record);
    return true;
}

bool modeleffects::CobwebPlacements::remove(jmp::ToolDataRef& entry) {
    if (!matches(entry)) return false;
    for (auto it = active.begin(); it != active.end(); ++it) {
        auto* record = *it;
        if (record->row != entry.row) continue;
        record->owner->release();
        active.erase(it);
        free.push_back(record);
        *record = {};
        return true;
    }
    return false;
}

bool modeleffects::CobwebPlacements::message(jmp::ToolDataRef&, const char*) { return false; }
