#include "game/player/playervacuumeffects.h"

namespace {
    playermodels::VacuumEntry* find(const playermodels::VacuumEntries& owner, const actors::MoveActor* actor,
        int ordinal, actors::MoveActor* playermodels::VacuumEntry::*field) {
        int found = 0;
        for (const auto& entry : owner.entries) {
            if ((entry->state ? entry.get()->*field : nullptr) == actor) {
                if (ordinal == found) return entry.get();
                ++found;
            }
        }
        return nullptr;
    }

    int index(const std::array<actors::MoveActor*, 6>& actors, const actors::MoveActor* actor) {
        for (unsigned i = 0; i < actors.size(); ++i) if (actors[i] == actor) return static_cast<int>(i);
        return -1;
    }
}

void playermodels::VacuumEntry::setTracking(const VacuumTracking& value) {
    if (!state) return;
    tracking = value;
    trackingChanged = 1;
}

void playermodels::VacuumEntry::setBlend(const VacuumTrackingBlend& value) {
    if (!state) return;
    blend = value;
    blendChanged = 1;
}

bool playermodels::VacuumEntries::containsSource(const actors::MoveActor* actor) const { return findSource(actor, 0) != nullptr; }
bool playermodels::VacuumEntries::containsTarget(const actors::MoveActor* actor) const { return findTarget(actor, 0) != nullptr; }

playermodels::VacuumEntry* playermodels::VacuumEntries::findSource(const actors::MoveActor* actor, int ordinal) const {
    return find(*this, actor, ordinal, &VacuumEntry::source);
}

playermodels::VacuumEntry* playermodels::VacuumEntries::findTarget(const actors::MoveActor* actor, int ordinal) const {
    return find(*this, actor, ordinal, &VacuumEntry::target);
}

bool playermodels::VacuumEntries::setTracking(const actors::MoveActor* actor, const VacuumTracking& value) {
    bool result = false;
    for (auto& entry : entries) {
        if ((entry->state ? entry->source : nullptr) == actor) {
            entry->setTracking(value);
            result = true;
        }
    }
    return result;
}

bool playermodels::VacuumEntries::setBlend(const actors::MoveActor* actor, const VacuumTrackingBlend& value) {
    bool result = false;
    for (auto& entry : entries) {
        if ((entry->state ? entry->target : nullptr) == actor) {
            entry->setBlend(value);
            result = true;
        }
    }
    return result;
}

int playermodels::VacuumEntries::sourceIndex(const actors::MoveActor* actor) const { return index(fieldA38, actor); }
int playermodels::VacuumEntries::targetIndex(const actors::MoveActor* actor) const { return index(fieldA50, actor); }

std::array<std::uint32_t, 12>* playermodels::VacuumEntries::sourceRecord(const actors::MoveActor* actor) {
    const auto index = sourceIndex(actor);
    return index < 0 ? nullptr : &sourceRecords[index];
}

std::array<std::uint32_t, 7>* playermodels::VacuumEntries::targetRecord(const actors::MoveActor* actor) {
    const auto index = targetIndex(actor);
    return index < 0 ? nullptr : &targetRecords[index];
}
