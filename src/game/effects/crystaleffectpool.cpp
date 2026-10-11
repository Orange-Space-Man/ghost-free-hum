#include "game/effects/crystaleffectpool.h"

#include <stdexcept>
#include <utility>

namespace {
    modeleffects::Definition definition() {
        modeleffects::Definition result;
        result.type = 10;
        result.capacity = 1;
        result.resourceType = 0x4434385f;
        return result;
    }

    void writeColor(j3dmodel::ModelData& model, const std::array<std::uint8_t, 4>& color) {
        for (auto& material : model.materials.entries) {
            if (!(material->identityFlags & 0xc0000000)) {
                if (!material->original) throw std::runtime_error("Original crystal material has no source.");
                material->identityFlags |= 0x80000000;
                material->mode = material->original->mode;
                material->fieldE = material->original->fieldE;
                material->animation.reset();
            }
            if (material->tev && material->tev->capacity) material->tev->constants[0] = color;
        }
    }
}

modeleffects::CrystalOwner::CrystalOwner() : Owner({}) { reset(); }

void modeleffects::CrystalOwner::reset() {
    flags = 6;
    frame = 0;
    position = {};
}

void modeleffects::CrystalOwner::release() { flags |= 0x40; }

void modeleffects::CrystalOwner::setFrame(double) {}

modeleffects::CrystalPool::CrystalPool(particles::EffectFactory& particles, std::uint32_t& randomState,
    Sound sound, Scratch scratch) : Pool(::definition(), nullptr), particles(particles), randomState(randomState),
    sound(std::move(sound)), scratch(std::move(scratch)) {
    if (!this->sound) throw std::invalid_argument("Original crystal effect requires positional audio delivery.");
    owners.push_back(std::make_unique<CrystalOwner>());
}

void modeleffects::CrystalPool::load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) {
    auto resource = [&](const char* name) {
        auto result = resources.get(definition.resourceType, name);
        if (!result) throw std::runtime_error(std::string("Missing original crystal resource: ") + name);
        return result;
    };
    std::string error;
    transform = std::make_shared<j3dmodel::Animation>();
    colorAnimation = std::make_shared<j3dmodel::Animation>();
    textureAnimation = std::make_shared<j3dmodel::Animation>();
    if (!transform->read(*resource("ftc.bck"), error) || !colorAnimation->read(*resource("ftc.bpk"), error) ||
        !textureAnimation->read(*resource("ftc.btk"), error)) throw std::runtime_error(error);
    auto model = std::make_shared<j3dmodel::ModelData>();
    if (!model->read(*resource("ftc.bmd"), 0x14020000, defaults, error)) throw std::runtime_error(error);
    owners[0]->model = std::make_unique<j3dmodel::ModelInstance>(model, 0x10000);
    j3dmodel::bindAnimation(*model, transform);
    j3dmodel::prepareMaterialAnimations(*model);
    j3dmodel::bindAnimation(*model, colorAnimation);
    j3dmodel::bindAnimation(*model, textureAnimation);
}

modeleffects::Owner* modeleffects::CrystalPool::available(const j3dmodel::Matrix& matrix) {
    auto& owner = static_cast<CrystalOwner&>(*owners[0]);
    owner.reset();
    owner.position = {matrix[3], matrix[7], matrix[11]};
    particles.create(0x46, owner.position, {});
    sound(0x18c3, owner.position, 0);
    return &owner;
}

void modeleffects::CrystalPool::advance(Owner& object, bool visible) {
    auto& owner = dynamic_cast<CrystalOwner&>(object);
    if (!(owner.flags & 1) || !visible) return;
    if (!transform || !colorAnimation || !textureAnimation || !owner.model) {
        throw std::runtime_error("Original crystal resources are unavailable.");
    }
    transform->frame = colorAnimation->frame = textureAnimation->frame = static_cast<float>(owner.frame);
    if (owner.flags & 0x10) return;
    const unsigned increment = owner.frame < 300 ? 2 : 1;
    sound(0x10c4, owner.position, 0);
    auto& model = *owner.model->model;
    if (owner.frame < 300 && (owner.flags & 0x40)) {
        if (model.materials.entries.empty() || !model.materials.entries.front()->tev) {
            throw std::runtime_error("Original crystal fade material is unavailable.");
        }
        auto color = model.materials.entries.front()->tev->constants[0];
        if (color[3] < 25) owner.flags &= 0xfe;
        else {
            color[3] -= 25;
            writeColor(model, color);
        }
        return;
    }
    if (!scratch) throw std::runtime_error("Original crystal RGB stack values require a captured source.");
    const auto rgb = scratch();
    randomState = randomState * 0x5eedf715u + 0x1b0cb173u;
    const auto alpha = static_cast<std::uint8_t>(static_cast<int>(255.0f - (128.0f * 0x1p-32f) * static_cast<float>(randomState)));
    writeColor(model, {rgb[0], rgb[1], rgb[2], alpha});
    owner.frame = static_cast<std::int32_t>(static_cast<std::uint32_t>(owner.frame) + increment);
    if (owner.frame > 329) {
        owner.frame = 0;
        owner.flags &= 0xfe;
    }
}
