#include "game/player/playericeeffectpool.h"

#include <stdexcept>

namespace {
    modeleffects::Definition definition() {
        modeleffects::Definition result;
        result.type = 0xb;
        result.capacity = 1;
        result.resourceType = 0x4b30382d;
        return result;
    }

    void increment(std::int32_t& value) {
        value = static_cast<std::int32_t>(static_cast<std::uint32_t>(value) + 1);
    }
}

modeleffects::PlayerIceOwner::PlayerIceOwner() : Owner({}) { reset(); }

void modeleffects::PlayerIceOwner::reset() {
    flags = 6;
    textureFrame = frame = 0;
    resumeAtSix = true;
    emitters = {};
}

void modeleffects::PlayerIceOwner::setMatrix(const j3dmodel::Matrix& matrix) {
    if (flags & 0x10) return;
    if (!model) throw std::runtime_error("Original player ice effect has no model instance.");
    model->matrix = matrix;
}

void modeleffects::PlayerIceOwner::setFrame(double) {}

modeleffects::PlayerIcePool::PlayerIcePool(particles::ArchiveResources& resources, particles::EmitterPool& emitters)
    : Pool(::definition(), nullptr), resources(resources), emitters(emitters) {
    owners.push_back(std::make_unique<PlayerIceOwner>());
}

void modeleffects::PlayerIcePool::load(particles::ArchiveResources& source, const j3dmodel::MaterialDefaults& defaults) {
    auto resource = [&](const char* name) {
        auto result = source.get(definition.resourceType, name);
        if (!result) throw std::runtime_error(std::string("Missing original player ice resource: ") + name);
        return result;
    };
    std::string error;
    transform = std::make_shared<j3dmodel::Animation>();
    if (!transform->read(*resource("lg_icestate.bck"), error)) throw std::runtime_error(error);
    textureAnimation = std::make_shared<j3dmodel::Animation>();
    if (!textureAnimation->read(*resource("lg_icestate.btk"), error)) throw std::runtime_error(error);
    auto model = std::make_shared<j3dmodel::ModelData>();
    if (!model->read(*resource("lg_icestate.bmd"), 0x14020000, defaults, error)) throw std::runtime_error(error);
    owners[0]->model = std::make_unique<j3dmodel::ModelInstance>(model, 0x10000);
    j3dmodel::bindAnimation(*model, transform);
    j3dmodel::prepareMaterialAnimations(*model);
    j3dmodel::bindAnimation(*model, textureAnimation);
}

modeleffects::Owner* modeleffects::PlayerIcePool::available() {
    auto& owner = static_cast<PlayerIceOwner&>(*owners[0]);
    for (auto* emitter : owner.emitters) if (emitter) emitter->release();
    owner.reset();
    auto first = resources.get(0x4b485f45, "lg_eledmg_i_a.pcr");
    auto firstTexture = resources.get(0x4b485f45, "star6_ex.bti");
    auto second = resources.get(0x4b485f45, "lg_eledmg_i_b.pcr");
    auto secondTexture = resources.get(0x4b485f45, "ice_smok.bti");
    for (unsigned i = 0; i < owner.emitters.size(); ++i) {
        auto* emitter = emitters.create(i ? second : first, i ? secondTexture : firstTexture);
        if (emitter) {
            emitter->position = {};
            emitter->field220 = {};
            emitter->field22C = {};
            emitter->fieldC4 = 0;
            emitter->fieldC8 = 0;
        }
        owner.emitters[i] = emitter;
    }
    return &owner;
}

void modeleffects::PlayerIcePool::update(const roomlocation::Rooms&, const roomtraversal::State&) {
    advance(*owners[0], false);
}

void modeleffects::PlayerIcePool::advance(Owner& object, bool) {
    auto& owner = dynamic_cast<PlayerIceOwner&>(object);
    if (!(owner.flags & 1)) return;
    if (!transform || !textureAnimation) throw std::runtime_error("Original player ice animations are not loaded.");
    transform->frame = static_cast<float>(owner.frame);
    textureAnimation->frame = static_cast<float>(owner.textureFrame);
    if (owner.flags & 0x10) return;
    if (!owner.model) throw std::runtime_error("Original player ice effect has no model instance.");
    const auto& matrix = owner.model->matrix;
    for (auto* emitter : owner.emitters) if (emitter) emitter->position = {matrix[3], matrix[7], matrix[11]};
    increment(owner.textureFrame);
    if (owner.textureFrame >= textureAnimation->frameCount) owner.textureFrame = 0;
    if (owner.resumeAtSix && owner.frame == 6) {
        owner.setRunning(true);
        owner.resumeAtSix = false;
    }
    if (!(owner.flags & 2)) return;
    increment(owner.frame);
    if (owner.frame >= transform->frameCount) {
        for (auto* emitter : owner.emitters) if (emitter) emitter->release();
        owner.release();
        owner.frame = 0;
    }
}
