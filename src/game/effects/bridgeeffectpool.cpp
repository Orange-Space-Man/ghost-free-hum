#include "game/effects/bridgeeffectpool.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
    modeleffects::Definition definition() {
        modeleffects::Definition result;
        result.type = 0x22;
        result.capacity = 1;
        result.resourceType = 0x4b31312d;
        return result;
    }

    std::int32_t integer(float value) {
        if (!std::isfinite(value) || value < -2147483648.0 || value >= 2147483648.0) return std::numeric_limits<std::int32_t>::min();
        return static_cast<std::int32_t>(value);
    }
}

modeleffects::BridgeOwner::BridgeOwner() : Owner({}) { reset(); }

void modeleffects::BridgeOwner::reset() {
    flags = 6;
    frame = 0;
}

void modeleffects::BridgeOwner::setFrame(double) {}

modeleffects::BridgePool::BridgePool() : Pool(::definition(), nullptr) {
    owners.push_back(std::make_unique<BridgeOwner>());
}

void modeleffects::BridgePool::load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) {
    auto resource = [&](const char* name) {
        auto result = resources.get(definition.resourceType, name);
        if (!result) throw std::runtime_error(std::string("Missing original bridge effect resource: ") + name);
        return result;
    };
    std::string error;
    colorAnimation = std::make_shared<j3dmodel::Animation>();
    if (!colorAnimation->read(*resource("starbridge.bpk"), error)) throw std::runtime_error(error);
    textureAnimation = std::make_shared<j3dmodel::Animation>();
    if (!textureAnimation->read(*resource("starbridge.btk"), error)) throw std::runtime_error(error);
    auto model = std::make_shared<j3dmodel::ModelData>();
    if (!model->read(*resource("starbridge.bmd"), 0x14020000, defaults, error)) throw std::runtime_error(error);
    owners[0]->model = std::make_unique<j3dmodel::ModelInstance>(model, 0x10000);
    j3dmodel::prepareMaterialAnimations(*model);
    j3dmodel::bindAnimation(*model, colorAnimation);
    j3dmodel::bindAnimation(*model, textureAnimation);
}

modeleffects::Owner* modeleffects::BridgePool::available() {
    owners[0]->reset();
    return owners[0].get();
}

void modeleffects::BridgePool::advance(Owner& owner, bool visible) {
    if (!(owner.flags & 1) || !visible || (owner.flags & 0x10)) return;
    if (!textureAnimation || !colorAnimation) throw std::runtime_error("Original bridge animations are not loaded.");
    textureAnimation->frame = static_cast<float>(owner.frame);
    owner.frame = static_cast<std::int32_t>(static_cast<std::uint32_t>(owner.frame) + 1);
    if (owner.frame >= textureAnimation->frameCount) owner.frame = 0;
    const auto colorFrame = integer(colorAnimation->frame);
    if (colorFrame < colorAnimation->frameCount) {
        colorAnimation->frame = static_cast<float>(static_cast<std::int32_t>(static_cast<std::uint32_t>(colorFrame) + 1));
    }
}
