#include "game/effects/rivereffectpool.h"

#include <stdexcept>
#include <utility>

namespace {
    modeleffects::Definition definition() {
        modeleffects::Definition result;
        result.type = 0x29;
        result.capacity = 1;
        result.resourceType = 0x52495645;
        return result;
    }

    void increment(std::int32_t& value) { value = static_cast<std::int32_t>(static_cast<std::uint32_t>(value) + 1); }

    void writeColor(j3dmodel::ModelData& model, const std::array<std::uint16_t, 4>& color) {
        for (auto& material : model.materials.entries) {
            if (!(material->identityFlags & 0xc0000000)) {
                if (!material->original) throw std::runtime_error("Original river material has no source.");
                material->identityFlags |= 0x80000000;
                material->mode = material->original->mode;
                material->fieldE = material->original->fieldE;
                material->animation.reset();
            }
            if (material->tev && material->tev->capacity) material->tev->colors[0] = color;
        }
    }
}

modeleffects::RiverOwner::RiverOwner() : Owner({}) { reset(); }

void modeleffects::RiverOwner::reset() {
    flags = 6;
    frame = secondFrame = 0;
}

void modeleffects::RiverOwner::setFrame(double) {}

modeleffects::RiverPool::RiverPool(particles::ArchiveResources& resources, particles::EmitterPool& emitters,
    const elements::Pool& elements, const elements::Projectile& ice, const elements::Projectile& water,
    gameflags::State& flags, mapcollision::State& collision, Sound sound)
    : Pool(::definition(), nullptr), resources(resources), emitters(emitters), elements(elements), ice(ice), water(water),
    flags(flags), collision(collision), sound(std::move(sound)) {
    if (!this->sound) throw std::invalid_argument("Original river requires positional audio delivery.");
    owners.push_back(std::make_unique<RiverOwner>());
}

void modeleffects::RiverPool::load(particles::ArchiveResources& source, const j3dmodel::MaterialDefaults& defaults) {
    auto resource = [&](const char* name) {
        auto result = source.get(definition.resourceType, name);
        if (!result) throw std::runtime_error(std::string("Missing original river resource: ") + name);
        return result;
    };
    std::string error;
    textures[0] = std::make_shared<j3dmodel::Animation>();
    textures[1] = std::make_shared<j3dmodel::Animation>();
    if (!textures[0]->read(*resource("water_1.btk"), error) || !textures[1]->read(*resource("water_2.btk"), error)) {
        throw std::runtime_error(error);
    }
    auto model = std::make_shared<j3dmodel::ModelData>();
    if (!model->read(*resource("water.bmd"), 0x54020000, defaults, error)) throw std::runtime_error(error);
    j3dmodel::prepareMaterialAnimations(*model);
    for (auto& texture : textures) j3dmodel::bindAnimation(*model, texture);
    owners[0]->model = std::make_unique<j3dmodel::ModelInstance>(model, 0x10000);
}

void modeleffects::RiverPool::createIce(RiverOwner& owner) {
    if (owner.emitter) return;
    const auto parameters = resources.get(0x4b485f45, "lg_eledmg_i_a.pcr");
    const auto texture = resources.get(0x4b485f45, "star4_i.bti");
    owner.emitter = emitters.create(parameters, texture);
    if (owner.emitter) {
        owner.emitter->position = owner.position;
        owner.emitter->field220 = {1, 0, -1};
        owner.emitter->field22C = {50, 400, 200};
        owner.emitter->fieldC4 = nullptr;
        owner.emitter->fieldC8 = nullptr;
        owner.emitter->parameters.fieldsD0[0] = 0x42b40000;
        owner.emitter->parameters.fieldsD0[1] = 0x42480000;
        owner.emitter->parameters.fieldsD0[2] = 0x42b40000;
    }
}

modeleffects::Owner* modeleffects::RiverPool::available(const j3dmodel::Matrix& matrix) {
    auto& owner = static_cast<RiverOwner&>(*owners[0]);
    if (owner.flags & 1) return nullptr;
    owner.position = {matrix[3], matrix[7], matrix[11]};
    if (flags.value(0x40)) {
        owner.flags |= 0x80;
        createIce(owner);
        if (!owner.model) throw std::runtime_error("Original river model is unavailable.");
        writeColor(*owner.model->model, {56, 68, 68, 255});
    }
    return &owner;
}

void modeleffects::RiverPool::updateInteraction(RiverOwner& owner) {
    if (!(owner.flags & 0xc0)) {
        const actorcollision::Cylinder cylinder{350, 10, {}};
        const actorcollision::Shape shape{1, &cylinder, nullptr};
        if (elements.intersects(shape, owner.position, 10) || elements::intersects(ice, water, shape, owner.position, 10)) {
            increment(owner.hits);
            if (owner.hits > 59) {
                owner.hits = owner.transition = 0;
                owner.flags |= 0x40;
            }
        } else owner.hits = owner.hits < 1 ? 0 : owner.hits - 1;
    }
    if (!(owner.flags & 0x40)) return;
    const auto step = static_cast<std::uint32_t>(owner.transition);
    auto color = [&](unsigned start, unsigned end) {
        const auto value = static_cast<std::int32_t>(step * end + (60u - step) * start);
        return static_cast<std::uint16_t>((value / 60) & 255);
    };
    if (!owner.model) throw std::runtime_error("Original river model is unavailable.");
    writeColor(*owner.model->model, {color(8, 56), color(105, 68), color(8, 68), 255});
    sound(0x109f, owner.position, 0);
    if (!(owner.flags & 0x80)) {
        increment(owner.transition);
        if (owner.transition > 60) {
            owner.flags = (owner.flags | 0x80) & 0xbf;
            owner.transition = 60;
            flags.setOn(0x40);
            collision.disable(7);
            createIce(owner);
        }
    }
}

void modeleffects::RiverPool::advance(Owner& object, bool visible) {
    auto& owner = dynamic_cast<RiverOwner&>(object);
    if (!(owner.flags & 1) || !visible) return;
    updateInteraction(owner);
    if (!textures[0] || !textures[1]) throw std::runtime_error("Original river texture animations are unavailable.");
    textures[0]->frame = static_cast<float>(owner.frame);
    textures[1]->frame = static_cast<float>(owner.secondFrame);
    if (!(owner.flags & 0x10) && (owner.flags & 2) && !(owner.flags & 0x80)) {
        increment(owner.frame);
        increment(owner.secondFrame);
        if (owner.frame >= textures[0]->frameCount) owner.frame = 0;
        if (owner.secondFrame >= textures[1]->frameCount) owner.secondFrame = 0;
    }
}
