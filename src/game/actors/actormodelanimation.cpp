#include "game/actors/moveactor.h"

#include <algorithm>
#include <stdexcept>

namespace {
    unsigned half(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 8) | bytes.at(offset + 1);
    }
}

const s3dmodel::Model& actors::MoveActor::surface() const {
    const auto& entry = construction.resources.entry(construction.catalog.type(actorType).model);
    if (!entry.archive) throw std::runtime_error("Actor surface requires its manager-owned archive.");
    return entry.archive->surface();
}

void actors::MoveActor::updateMorph() {
    if (fieldF8 == -1) return;
    const auto& entry = construction.resources.entry(construction.catalog.type(actorType).model);
    if (!entry.archive) throw std::runtime_error("Actor morph update requires its manager-owned archive.");
    const auto& pair = entry.archive->morphs.at(static_cast<unsigned>(fieldF8));
    auto& primary = model->effects[0];
    if (primary.field4 != pair[0]) {
        primary.value = 0;
        primary.field4 = pair[0];
    }
    primary.field8 = pair[1];
    if (!primary.field8) throw std::runtime_error("Actor morph update requires the original weight keys.");
    const auto& keys = primary.field8->bytes;
    const float duration = float(half(keys, 4));
    primary.value += 1;
    if (primary.value >= duration) {
        if (!half(keys, 6)) primary.value -= duration;
        else primary.value = duration;
    }
    auto& positions = entry.archive->surface().positions;
    for (auto& effect : model->effects) {
        if (!effect.field4) continue;
        if (!effect.field8) throw std::runtime_error("Actor morph slot requires its retained weight keys.");
        if (effect.decodedShapes != effect.field4 || effect.decodedKeys != effect.field8) {
            s3dmorph::Animation animation;
            std::string error;
            if (!animation.read(effect.field4->bytes, effect.field8->bytes, positions.size(), error))
                throw std::runtime_error(error);
            effect.animation = std::move(animation);
            effect.decodedShapes = effect.field4;
            effect.decodedKeys = effect.field8;
        }
        effect.animation.apply(effect.value, positions);
    }
}

void actors::MoveActor::updateMaterials() {
    if (!model->field110) return;
    const auto& entry = construction.resources.entry(construction.catalog.type(actorType).model);
    if (!entry.archive) throw std::runtime_error("Actor material update requires its manager-owned archive.");
    auto& materials = entry.archive->surface().materials;
    if (model->decodedMaterial != model->field110) {
        s3denvironment::MaterialAnimation animation;
        std::string error;
        if (!animation.read(model->field110->bytes, materials.size(), error)) throw std::runtime_error(error);
        model->materialAnimation = std::move(animation);
        model->decodedMaterial = model->field110;
    }
    const auto& bytes = model->field110->bytes;
    const float duration = float(half(bytes, 6));
    const bool reversed = (construction.catalog.type(actorType).animations.at(animationState)[5] & 2) != 0;
    const float previous = model->materialFrame;
    if (reversed) model->materialFrame = std::max((duration - 1) - previous, 0.0f);
    model->materialAnimation.apply(model->materialFrame, materials);
    model->materialFrame += 1;
    if (model->materialFrame >= duration) {
        if (!half(bytes, 2)) model->materialFrame -= duration;
        else model->materialFrame = duration - 1;
    }
    if (reversed) model->materialFrame = previous + 1;
}
