#include "game/effects/waterfalleffectpool.h"

#include <cstring>
#include <stdexcept>

namespace {
    modeleffects::Definition definition() {
        modeleffects::Definition result;
        result.type = 0x27;
        result.capacity = 1;
        result.resourceType = 0x4b31352d;
        return result;
    }

    void release(particles::Emitter*& emitter) {
        if (!emitter) return;
        emitter->release();
        emitter = nullptr;
    }

    void increment(std::int32_t& value) {
        value = static_cast<std::int32_t>(static_cast<std::uint32_t>(value) + 1);
    }
}

modeleffects::WaterfallOwner::WaterfallOwner() : Owner({}) { reset(); }

void modeleffects::WaterfallOwner::reset() {
    flags = 6;
    frame = 0;
    for (auto& emitter : emitters) ::release(emitter);
}

void modeleffects::WaterfallOwner::setFrame(double) {}

std::uint8_t modeleffects::WaterfallOwner::fade() {
    if (!model || model->model->materials.entries.empty()) throw std::runtime_error("Original waterfall has no material.");
    auto& materials = model->model->materials.entries;
    auto& first = *materials.front()->tev;
    const unsigned index = first.colors[0][3] == 0 ? 1 : 0;
    auto color = first.colors[index];
    const auto alpha = static_cast<std::int16_t>(color[3]);
    color[3] = alpha < 2 ? 0 : static_cast<std::uint16_t>(alpha - 2);
    for (auto& material : materials) {
        if (!(material->identityFlags & 0xc0000000)) {
            if (!material->original) throw std::runtime_error("Original waterfall material has no source.");
            material->identityFlags |= 0x80000000;
            material->mode = material->original->mode;
            material->fieldE = material->original->fieldE;
            material->animation.reset();
        }
        if (material->tev && material->tev->capacity) material->tev->colors[index] = color;
    }
    if (!index) return 255;
    const auto value = static_cast<float>(static_cast<std::int16_t>(color[3]));
    const std::array<float, 2> rates{(1.75f * value) / 255.0f, (1.5f * value) / 255.0f};
    for (unsigned i = 0; i < emitters.size(); ++i) {
        if (emitters[i]) std::memcpy(&emitters[i]->parameters.fieldF4, &rates[i], sizeof(float));
    }
    return static_cast<std::uint8_t>(color[3]);
}

modeleffects::WaterfallPool::WaterfallPool(particles::ArchiveResources& resources, particles::EmitterPool& emitters,
    const gameflags::State& flags, SoundEmitter sound)
    : Pool(::definition(), nullptr), resources(resources), emitters(emitters), flags(flags), sound(std::move(sound)) {
    if (!this->sound) throw std::invalid_argument("Original waterfall requires positional audio delivery.");
    owners.push_back(std::make_unique<WaterfallOwner>());
}

void modeleffects::WaterfallPool::load(particles::ArchiveResources& source, const j3dmodel::MaterialDefaults& defaults) {
    auto resource = [&](const char* name) {
        auto result = source.get(definition.resourceType, name);
        if (!result) throw std::runtime_error(std::string("Missing original waterfall resource: ") + name);
        return result;
    };
    std::string error;
    textureAnimation = std::make_shared<j3dmodel::Animation>();
    if (!textureAnimation->read(*resource("taki.btk"), error)) throw std::runtime_error(error);
    auto model = std::make_shared<j3dmodel::ModelData>();
    if (!model->read(*resource("taki.bmd"), 0x54020000, defaults, error)) throw std::runtime_error(error);
    j3dmodel::prepareMaterialAnimations(*model);
    j3dmodel::bindAnimation(*model, textureAnimation);
    owners[0]->model = std::make_unique<j3dmodel::ModelInstance>(model, 0x10000);
}

modeleffects::Owner* modeleffects::WaterfallPool::available() {
    throw std::runtime_error("Original waterfall creation requires its placement matrix.");
}

modeleffects::Owner* modeleffects::WaterfallPool::available(const j3dmodel::Matrix& matrix) {
    auto& owner = static_cast<WaterfallOwner&>(*owners[0]);
    if (!(owner.flags & 1)) {
        owner.position = {matrix[3], matrix[7], matrix[11]};
        const std::array<const char*, 2> parameters{"taki_smoke.pcr", "taki_shibuki.pcr"};
        const std::array<const char*, 2> textures{"ice_smok.bti", "watball1.bti"};
        for (unsigned i = 0; i < owner.emitters.size(); ++i) {
            auto parameter = resources.get(0x4b485f45, parameters[i]);
            auto texture = resources.get(0x4b485f45, textures[i]);
            auto* emitter = emitters.create(parameter, texture);
            if (emitter) {
                emitter->position = owner.position;
                emitter->field220 = {};
                emitter->field22C = {};
                emitter->fieldC4 = 0;
                emitter->fieldC8 = 0;
            }
            owner.emitters[i] = emitter;
        }
        if (owner.emitters[0] && owner.emitters[1]) return &owner;
    }
    for (auto& emitter : owner.emitters) ::release(emitter);
    return nullptr;
}

void modeleffects::WaterfallPool::advance(Owner& object, bool visible) {
    auto& owner = dynamic_cast<WaterfallOwner&>(object);
    if (!(owner.flags & 1) || !visible) return;
    if (!flags.value(0x3f)) sound(0x108c, owner.position, 0);
    else if (owner.soundFrame < 162) {
        sound(0x108c, owner.position, 0);
        increment(owner.soundFrame);
    } else if (owner.soundFrame == 162) {
        sound(0x189e, owner.position, 0);
        increment(owner.soundFrame);
    } else if (!owner.fade()) {
        owner.release();
        return;
    }
    if (!textureAnimation) throw std::runtime_error("Original waterfall animation is not loaded.");
    textureAnimation->frame = static_cast<float>(owner.frame);
    if (!(owner.flags & 0x10) && (owner.flags & 2)) {
        increment(owner.frame);
        if (owner.frame >= textureAnimation->frameCount) owner.frame = 0;
    }
}
