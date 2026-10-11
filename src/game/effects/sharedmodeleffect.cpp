#include "game/effects/sharedmodeleffect.h"

#include <stdexcept>
#include <cstring>

namespace {
    modeleffects::Definition definition() {
        modeleffects::Definition result;
        result.type = 0x2d;
        result.capacity = 16;
        result.resourceType = 0x4433395f;
        return result;
    }
}

modeleffects::SharedOwner::SharedOwner() : Owner({}) { reset(); }

void modeleffects::SharedOwner::reset() {
    flags = 6;
    field14 = 0;
    colorFrame = 0;
    frame = 0;
}

void modeleffects::SharedOwner::setPosition(const std::array<float, 3>& position) {
    if (!model) throw std::runtime_error("Original shared effect has no model instance.");
    model->matrix = {scaleFactor, 0, 0, position[0], 0, scaleFactor, 0, position[1] + 15,
        0, 0, scaleFactor, position[2]};
}

modeleffects::SharedPool::SharedPool() : Pool(::definition(), nullptr) {
    for (unsigned i = 0; i < definition.capacity; ++i) owners.push_back(std::make_unique<SharedOwner>());
}

void modeleffects::SharedPool::load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) {
    auto resource = [&](const char* name) {
        auto result = resources.get(definition.resourceType, name);
        if (!result) throw std::runtime_error(std::string("Missing original shared effect resource: ") + name);
        return result;
    };
    std::string error;
    textureAnimation = std::make_shared<j3dmodel::Animation>();
    if (!textureAnimation->read(*resource("gshadow.btk"), error)) throw std::runtime_error(error);
    colorAnimation = std::make_shared<j3dmodel::Animation>();
    if (!colorAnimation->read(*resource("gshadow.bpk"), error)) throw std::runtime_error(error);
    const auto data = resource("gshadow.bmd");
    for (auto& owner : owners) {
        auto model = std::make_shared<j3dmodel::ModelData>();
        if (!model->read(*data, 0x14020000, defaults, error)) throw std::runtime_error(error);
        j3dmodel::prepareMaterialAnimations(*model);
        j3dmodel::bindAnimation(*model, textureAnimation);
        j3dmodel::bindAnimation(*model, colorAnimation);
        owner->model = std::make_unique<j3dmodel::ModelInstance>(model, 0x10000);
    }
}

modeleffects::Owner* modeleffects::SharedPool::available() {
    for (auto& owner : owners) if (!(owner->flags & 1)) return owner.get();
    return nullptr;
}

void modeleffects::SharedPool::advance(Owner& object, bool visible) {
    auto& owner = dynamic_cast<SharedOwner&>(object);
    if (!(owner.flags & 1) || !visible) return;
    if (!textureAnimation || !colorAnimation) throw std::runtime_error("Original shared effect animations are not loaded.");
    textureAnimation->frame = static_cast<float>(owner.frame);
    colorAnimation->frame = static_cast<float>(owner.colorFrame);
    if ((owner.flags & 0x10) || !(owner.flags & 2)) return;
    owner.frame = static_cast<std::int32_t>(static_cast<std::uint32_t>(owner.frame) + 1);
    if (owner.frame >= textureAnimation->frameCount) owner.frame = 0;
    owner.colorFrame = static_cast<std::int32_t>(static_cast<std::uint32_t>(owner.colorFrame) + 1);
    if (owner.colorFrame >= colorAnimation->frameCount) owner.colorFrame = 0;
}

modeleffects::SharedOwner* modeleffects::createShared(Factory& factory, const enemytypes::Manager& parameters,
    unsigned type, const std::array<float, 3>& position) {
    const auto& values = parameters.parameters(type).entries();
    float range;
    const auto bits = values.at(23).bits;
    std::memcpy(&range, &bits, sizeof(range));
    const float scale = 0.025f * range;
    const auto color = values.at(24).bits;
    auto* owner = dynamic_cast<SharedOwner*>(factory.create(0x2d, position));
    if (!owner) return nullptr;
    owner->flags |= 1;
    owner->scaleFactor = scale;
    owner->setScale({scale, scale, scale});
    for (auto& material : owner->model->model->materials.entries) {
        if (!(material->identityFlags & 0xc0000000)) {
            if (!material->original) throw std::runtime_error("Original shared effect material has no source.");
            material->identityFlags |= 0x80000000;
            material->mode = material->original->mode;
            material->fieldE = material->original->fieldE;
            material->animation.reset();
        }
        if (material->tev && material->tev->capacity) {
            for (unsigned i = 0; i < 4; ++i) material->tev->colors[0][i] = static_cast<std::uint16_t>((color >> (24 - i * 8)) & 255);
        }
    }
    return owner;
}
