#include "game/player/playerattachedstate.h"
#include "game/actors/moveactor.h"
#include "game/actors/actortimingsampler.h"

#include <algorithm>
#include <stdexcept>

void playercomponents::AttachedState::updateOverrides() {
    for (unsigned i = 0; i < 2; ++i) {
        auto& effect = effects[i];
        if (!field36C && !(i ? field2D0 : field2C0)) {
            if (fields2B8[i]) {
                effect.flags &= ~1u;
                effect.field74 = effect.field76 = 6;
                effect.field4.reset();
                model.resetCursors();
                fields2B8[i] = 0;
                if (!i && field2DC != -1) {
                    field2DC = -1;
                    sound.bind(nullptr, 1, 0);
                    field368 = 1;
                }
            }
        } else if (!fields2B8[i] && effect.field4) {
            effect.flags |= 1;
            if (!i) model.resetCursors();
            fields2B8[i] = 1;
        }
    }
}

void playercomponents::AttachedState::advance(double rate, const actormodel::Catalog& catalog,
    const actormodel::Resources& resources, const std::function<void(unsigned, const std::array<float, 3>&)>& emit) {
    if (!actor) throw std::runtime_error("Attached animation advance requires its original actor owner.");
    const auto modelIndex = catalog.type(actor->actorType).model;
    const auto& entry = resources.entry(modelIndex);
    const auto key = [&](unsigned selection) {
        if (!entry.archive) throw std::runtime_error("Attached animation advance requires its model archive.");
        const auto resource = entry.archive->keys.at(selection);
        if (!resource) throw std::runtime_error("Attached animation advance requires its selected key.");
        return resource;
    };
    const auto duration = [&](unsigned selection) {
        const auto resource = key(selection & 0xffff);
        return float((unsigned(resource->bytes.at(4)) << 8) | resource->bytes.at(5));
    };
    const auto bind = [&](unsigned index, unsigned selection) {
        auto& effect = effects[index];
        const auto resource = key(selection);
        if (!fields2B8[index] || effect.field4 != resource) {
            effect.field4 = resource;
            effect.field74 = effect.field76 = 6;
            model.resetCursors();
        }
    };
    if (!field2C0) field2CC = 0;
    else {
        field2CC = static_cast<float>(double(field2C8) * rate);
        field2C4 += field2CC;
        const auto selection = fields24C.at(field2C0);
        if (field2C4 < duration(selection)) {
            if (field2DC != static_cast<std::int32_t>(selection)) {
                field2DC = static_cast<std::int32_t>(selection);
                const auto soundIndex = static_cast<std::int32_t>(catalog.model(modelIndex).keyMetadata.at(selection)[5]);
                const auto& sounds = entry.archive->auxiliary[4];
                sound.bind(soundIndex >= 0 && static_cast<unsigned>(soundIndex) < sounds.size() ? sounds[soundIndex] : nullptr, 1, 0);
                field368 = 1;
            }
            bind(0, selection);
            effects[0].field8 = field2C4;
            if (field2C0 >= 1 && field2C0 <= 3) {
                const auto timing = resources.timing(modelIndex, selection & 0xffff);
                const float previous = std::max(static_cast<float>(double(field2C4) - double(field2C8)), 0.0f);
                if (actoranimation::timingCrossing(timing.get(), "vhead_timing", previous, field2C4) > 0)
                    emit(0x805, actor->position);
            }
        } else field2C0 = 0;
    }
    if (field2D0) {
        field2D4 += static_cast<float>(double(field2D8) * rate);
        const auto selection = fields24C.at(field2D0);
        if (field2D4 < duration(selection)) {
            bind(1, selection);
            effects[1].field8 = field2D4;
        } else field2D0 = 0;
    }
    updateOverrides();
}

void playercomponents::AttachedState::finish(double rate, const actormodel::Catalog& catalog,
    const actormodel::Resources& resources, const std::function<void(unsigned, const std::array<float, 3>&)>& emit,
    const actoranimation::SoundServices& playback) {
    advance(rate, catalog, resources, emit);
    field36C = 0;
    if (field368) sound.update(field2C4, field2CC, playback);
}
