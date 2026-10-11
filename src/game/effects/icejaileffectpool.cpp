#include "game/effects/icejaileffectpool.h"

#include <stdexcept>

namespace {
    modeleffects::Definition definition() {
        modeleffects::Definition result;
        result.type = 0xe;
        result.capacity = 1;
        result.resourceType = 0x4b31322d;
        return result;
    }
}

modeleffects::IceJailOwner::IceJailOwner() : Owner({}) { reset(); }

void modeleffects::IceJailOwner::reset() {
    flags = 6;
    colorFrame = 0;
    frame = 0;
}

void modeleffects::IceJailOwner::setFrame(double value) { colorFrame = static_cast<float>(value); }

modeleffects::IceJailPool::IceJailPool() : Pool(::definition(), nullptr) {
    owners.push_back(std::make_unique<IceJailOwner>());
}

void modeleffects::IceJailPool::load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) {
    auto resource = [&](const char* name) {
        auto result = resources.get(definition.resourceType, name);
        if (!result) throw std::runtime_error(std::string("Missing original ice jail resource: ") + name);
        return result;
    };
    std::string error;
    auto model = std::make_shared<j3dmodel::ModelData>();
    if (!model->read(*resource("icejail.bmd"), 0x14020000, defaults, error)) throw std::runtime_error(error);
    textureAnimation = std::make_shared<j3dmodel::Animation>();
    if (!textureAnimation->read(*resource("icejail.btk"), error)) throw std::runtime_error(error);
    colorAnimation = std::make_shared<j3dmodel::Animation>();
    if (!colorAnimation->read(*resource("icejail.bpk"), error)) throw std::runtime_error(error);
    j3dmodel::prepareMaterialAnimations(*model);
    j3dmodel::bindAnimation(*model, textureAnimation);
    j3dmodel::bindAnimation(*model, colorAnimation);
    owners[0]->model = std::make_unique<j3dmodel::ModelInstance>(model, 0x10000);
}

modeleffects::Owner* modeleffects::IceJailPool::available() {
    owners[0]->reset();
    return owners[0].get();
}

void modeleffects::IceJailPool::advance(Owner& object, bool visible) {
    auto& owner = dynamic_cast<IceJailOwner&>(object);
    if (!(owner.flags & 1) || !visible) {
        owner.flags &= 0xf7;
        return;
    }
    if (!textureAnimation || !colorAnimation) throw std::runtime_error("Original ice jail animations are not loaded.");
    textureAnimation->frame = static_cast<float>(owner.frame);
    colorAnimation->frame = owner.colorFrame;
    if (owner.flags & 0x10) return;
    owner.frame = static_cast<std::int32_t>(static_cast<std::uint32_t>(owner.frame) + 1);
    if (owner.frame >= textureAnimation->frameCount) owner.frame = 0;
}
