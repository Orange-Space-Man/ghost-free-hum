#include "game/world/eventeffectplacements.h"

#include <stdexcept>

particles::EventEffectPlacements::EventEffectPlacements(ArchiveResources& resources, EmitterPool& emitters) :
    resources(resources), emitters(emitters) {}

bool particles::EventEffectPlacements::message(jmp::ToolDataRef& entry, const char* command) {
    std::string_view name;
    if (!entry.table || !entry.table->getValue(entry.row, "name", name))
        throw std::runtime_error("Original event effect placement requires its name field.");
    if (name != "effect") return false;
    if (std::string_view(command) != "ev_ent_kie")
        throw std::runtime_error("Event effect command requires its original resource and emitter bindings.");
    std::array<float, 3> position{};
    const char* coordinates[] = {"pos_x", "pos_y", "pos_z"};
    for (unsigned i = 0; i < position.size(); ++i)
        if (!entry.table->getValue(entry.row, coordinates[i], position[i]))
            throw std::runtime_error("Original event effect position is missing.");
    auto* emitter = emitters.create(resources.get(0x4b485f45, "ev_ent_kie.pcr"),
        resources.get(0x4b485f45, "obake_s4.bti"));
    if (emitter) {
        emitter->position = position;
        emitter->field220 = emitter->field22C = {};
        emitter->fieldC4 = nullptr;
        emitter->fieldC8 = nullptr;
    }
    return true;
}
