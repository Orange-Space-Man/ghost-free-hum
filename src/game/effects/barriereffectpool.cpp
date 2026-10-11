#include "game/effects/barriereffectpool.h"

#include <stdexcept>

namespace {
    modeleffects::Definition definition(unsigned variant) {
        if (variant > 1) throw std::invalid_argument("Invalid original barrier variant.");
        modeleffects::Definition result;
        result.type = 0x24 + variant;
        result.capacity = 1;
        result.resourceType = 0x4435355f;
        return result;
    }
}

modeleffects::BarrierOwner::BarrierOwner() : Owner({}) { reset(); }

void modeleffects::BarrierOwner::reset() {
    flags = 6;
    animationFrame = 0;
    speed = 1;
}

void modeleffects::BarrierOwner::setFrame(double) {}

modeleffects::BarrierPool::BarrierPool(unsigned variant, particles::EmitterPool& emitters)
    : Pool(::definition(variant), nullptr), variant(variant), emitters(emitters) {
    owners.push_back(std::make_unique<BarrierOwner>());
}

void modeleffects::BarrierPool::load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) {
    const std::string base = variant ? "kek_b" : "kek_a";
    auto resource = [&](const char* extension) {
        auto result = resources.get(definition.resourceType, base + extension);
        if (!result) throw std::runtime_error("Missing original barrier resource: " + base + extension);
        return result;
    };
    std::string error;
    textureAnimation = std::make_shared<j3dmodel::Animation>();
    if (!textureAnimation->read(*resource(".btk"), error)) throw std::runtime_error(error);
    auto model = std::make_shared<j3dmodel::ModelData>();
    if (!model->read(*resource(".bmd"), 0x14020000, defaults, error)) throw std::runtime_error(error);
    owners[0]->model = std::make_unique<j3dmodel::ModelInstance>(model, 0x10000);
    j3dmodel::prepareMaterialAnimations(*model);
    j3dmodel::bindAnimation(*model, textureAnimation);
}

modeleffects::Owner* modeleffects::BarrierPool::available() {
    owners[0]->reset();
    return owners[0].get();
}

void modeleffects::BarrierPool::advance(Owner& object, bool visible) {
    auto& owner = dynamic_cast<BarrierOwner&>(object);
    if (!(owner.flags & 1) || !visible) {
        owner.flags &= 0xf7;
        return;
    }
    if (owner.flags & 0x10) return;
    if (!textureAnimation) throw std::runtime_error("Original barrier animation is not loaded.");
    owner.animationFrame += owner.speed;
    const auto end = static_cast<float>(textureAnimation->frameCount);
    if (owner.animationFrame < end) {
        if ((owner.flags & 0x80) && owner.animationFrame < 5) owner.release();
        return;
    }
    owner.animationFrame = 0;
    if (!(owner.flags & 0x40) || (owner.flags & 0x80)) return;
    owner.speed += 1;
    if (!(owner.speed > 10)) return;
    owner.speed = -0.5f;
    owner.flags |= 0x80;
    owner.animationFrame = end - 10;
    if (!owner.model) throw std::runtime_error("Original barrier effect has no model instance.");
    const auto& matrix = owner.model->matrix;
    auto* emitter = emitters.create(owner.particle, owner.texture);
    if (!emitter) return;
    emitter->position = {matrix[3], matrix[7] + 180.0f, matrix[11]};
    emitter->field220 = {};
    emitter->field22C = {};
    emitter->fieldC4 = 0;
    emitter->fieldC8 = 0;
    emitter->parameters.field14C = 90;
}
