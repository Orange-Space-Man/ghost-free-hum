#include "game/effects/icicleeffectpool.h"

#include <stdexcept>

namespace {
    modeleffects::Definition definition(unsigned count, std::int32_t lifetime, unsigned type) {
        modeleffects::Definition result;
        result.type = type;
        result.capacity = count;
        result.lifetime = lifetime;
        result.resourceType = 0x4b30392d;
        return result;
    }

    void increment(std::int32_t& value) { value = static_cast<std::int32_t>(static_cast<std::uint32_t>(value) + 1); }
}

modeleffects::IcicleOwner::IcicleOwner() : Owner({}) { reset(); }

void modeleffects::IcicleOwner::reset() {
    flags = 6;
    frame = textureFrame = elapsed = 0;
}

void modeleffects::IcicleOwner::setMatrix(const j3dmodel::Matrix& matrix) {
    if (flags & 0x10) return;
    if (!model) throw std::runtime_error("Original icicle effect has no model instance.");
    model->matrix = matrix;
}

void modeleffects::IcicleOwner::setFrame(double) {}

modeleffects::IciclePool::IciclePool(unsigned count, unsigned variant, std::int32_t lifetime, unsigned type) :
    Pool(::definition(count, lifetime, type), nullptr), variant(variant) {
    if (variant > 1) throw std::runtime_error("Invalid original icicle effect variant.");
    for (unsigned i = 0; i < count; ++i) owners.push_back(std::make_unique<IcicleOwner>());
}

void modeleffects::IciclePool::load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) {
    const std::string base = variant ? "hyochu" : "tsurara_s";
    auto resource = [&](const char* extension) {
        const auto name = base + extension;
        auto result = resources.get(definition.resourceType, name);
        if (!result) throw std::runtime_error("Missing original icicle effect resource: " + name);
        return result;
    };
    std::string error;
    transform = std::make_shared<j3dmodel::Animation>();
    if (!transform->read(*resource(".bck"), error)) throw std::runtime_error(error);
    for (auto& object : owners) {
        auto& owner = dynamic_cast<IcicleOwner&>(*object);
        auto model = std::make_shared<j3dmodel::ModelData>();
        if (!model->read(*resource(".bmd"), 0x14020000, defaults, error)) throw std::runtime_error(error);
        owner.textureAnimation = std::make_shared<j3dmodel::Animation>();
        if (!owner.textureAnimation->read(*resource(".btk"), error)) throw std::runtime_error(error);
        j3dmodel::bindAnimation(*model, transform);
        j3dmodel::prepareMaterialAnimations(*model);
        j3dmodel::bindAnimation(*model, owner.textureAnimation);
        owner.model = std::make_unique<j3dmodel::ModelInstance>(model, 0x10000);
    }
}

void modeleffects::IciclePool::advance(Owner& object, bool visible) {
    auto& owner = dynamic_cast<IcicleOwner&>(object);
    if (!(owner.flags & 1) || !visible) {
        owner.flags &= 0xf7;
        return;
    }
    if (!owner.textureAnimation || !transform) throw std::runtime_error("Original icicle animations are not loaded.");
    owner.textureAnimation->frame = static_cast<float>(owner.textureFrame);
    if (owner.flags & 0x10) return;
    increment(owner.textureFrame);
    if (owner.textureFrame >= owner.textureAnimation->frameCount) owner.textureFrame = 0;
    if (!(owner.flags & 2)) return;
    increment(owner.frame);
    if (owner.frame >= transform->frameCount) owner.frame = 0;
    if (definition.lifetime) {
        increment(owner.elapsed);
        if (owner.elapsed == definition.lifetime) owner.release();
    }
}
