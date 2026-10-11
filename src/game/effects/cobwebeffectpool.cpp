#include "game/effects/cobwebeffectpool.h"

#include <cmath>
#include <stdexcept>

namespace {
    using Vector = std::array<float, 3>;

    modeleffects::Definition definition() {
        modeleffects::Definition result;
        result.type = 0x2b;
        result.capacity = 10;
        result.resourceType = 0x4434345f;
        return result;
    }

    float square(const Vector& vector) {
        return std::fma(vector[2], vector[2], vector[0] * vector[0]) + vector[1] * vector[1];
    }

    float dot(const Vector& first, const Vector& second) {
        return std::fma(first[0], second[0], first[1] * second[1]) + first[2] * second[2];
    }

    Vector cross(const Vector& first, const Vector& second) {
        return {first[1] * second[2] - first[2] * second[1], first[2] * second[0] - first[0] * second[2],
            first[0] * second[1] - first[1] * second[0]};
    }

    void normalize(Vector& vector) {
        const float length = square(vector);
        const float estimate = 1.0f / std::sqrt(length);
        const float inverse = (3.0f - (estimate * estimate) * length) * (estimate * 0.5f);
        for (auto& component : vector) component *= inverse;
    }

    void increment(std::int32_t& value, std::int32_t amount = 1) {
        value = static_cast<std::int32_t>(static_cast<std::uint32_t>(value) + static_cast<std::uint32_t>(amount));
    }
}

modeleffects::CobwebOwner::CobwebOwner() : Owner({}) { reset(); }

void modeleffects::CobwebOwner::reset() {
    flags = 6;
    frame = 0;
    inverseDistance = 1;
    step = 1;
}

void modeleffects::CobwebOwner::setFrame(double) {}

modeleffects::CobwebPool::CobwebPool(const playeractor::View& player, particles::EffectFactory& particles)
    : Pool(::definition(), nullptr), player(player), particles(particles) {
    for (unsigned i = 0; i < definition.capacity; ++i) owners.push_back(std::make_unique<CobwebOwner>());
}

void modeleffects::CobwebPool::load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) {
    auto resource = [&](const char* name) {
        auto result = resources.get(definition.resourceType, name);
        if (!result) throw std::runtime_error(std::string("Missing original cobweb resource: ") + name);
        return result;
    };
    std::string error;
    transform = std::make_shared<j3dmodel::Animation>();
    if (!transform->read(*resource("snet_a.bck"), error)) throw std::runtime_error(error);
    auto first = std::make_shared<j3dmodel::ModelData>();
    if (!first->read(*resource("snet_a.bmd"), 0x54020000, defaults, error)) throw std::runtime_error(error);
    j3dmodel::bindAnimation(*first, transform);
    for (auto& owner : owners) owner->model = std::make_unique<j3dmodel::ModelInstance>(first, 0x10000);
    secondaryTransform = std::make_shared<j3dmodel::Animation>();
    if (!secondaryTransform->read(*resource("snet_b.bck"), error)) throw std::runtime_error(error);
    const auto colors = resource("snet_b.brk");
    for (auto& object : owners) {
        auto& owner = static_cast<CobwebOwner&>(*object);
        owner.color = std::make_shared<j3dmodel::Animation>();
        if (!owner.color->read(*colors, error)) throw std::runtime_error(error);
    }
    const auto second = resource("snet_b.bmd");
    for (auto& object : owners) {
        auto& owner = static_cast<CobwebOwner&>(*object);
        auto model = std::make_shared<j3dmodel::ModelData>();
        if (!model->read(*second, 0x54020000, defaults, error)) throw std::runtime_error(error);
        j3dmodel::bindAnimation(*model, secondaryTransform);
        j3dmodel::prepareMaterialAnimations(*model);
        j3dmodel::bindAnimation(*model, owner.color);
        owner.secondary = std::make_unique<j3dmodel::ModelInstance>(model, 0x10000);
    }
}

modeleffects::Owner* modeleffects::CobwebPool::available() {
    for (auto& owner : owners) if (!(owner->flags & 1)) return owner.get();
    return nullptr;
}

modeleffects::CobwebOwner* modeleffects::createCobweb(Factory& factory, const j3dmodel::Matrix& matrix) {
    auto* owner = dynamic_cast<CobwebOwner*>(factory.create(0x2b, matrix));
    if (owner) {
        if (!owner->secondary) throw std::runtime_error("Original cobweb secondary model is unavailable.");
        owner->secondary->matrix = owner->model->matrix;
    }
    return owner;
}

void modeleffects::CobwebPool::update(const roomlocation::Rooms& rooms, const roomtraversal::State& visibility) {
    for (auto& owner : owners) {
        bool visible = false;
        if ((owner->flags & 0x81) && !(owner->flags & 0x10)) {
            if (!owner->model) throw std::runtime_error("Original cobweb model is unavailable.");
            const auto& matrix = owner->model->matrix;
            const auto room = rooms.resolve({matrix[3], matrix[7], matrix[11]}, 0xffffffff);
            const auto flags = visibility.record(room, 0).flags;
            visible = (flags & 1) && !(flags & 2);
        }
        advance(*owner, visible);
    }
}

void modeleffects::CobwebPool::advance(Owner& object, bool visible) {
    auto& owner = dynamic_cast<CobwebOwner&>(object);
    if (owner.flags & 0x10) return;
    if (!visible) return;
    if (owner.flags & 0x80) {
        if (!transform) throw std::runtime_error("Original cobweb animation is unavailable.");
        if (owner.frame < transform->frameCount) increment(owner.frame);
        return;
    }
    if (!(owner.flags & 1)) return;
    if (!owner.model) throw std::runtime_error("Original cobweb model is unavailable.");
    auto& matrix = owner.model->matrix;
    Vector position{matrix[3], matrix[7], matrix[11]}, axis{matrix[2], matrix[6], matrix[10]};
    if (!(owner.flags & 0x40)) {
        Vector direction;
        if (!player.field8B4 || !player.field8B4->intersects(200, direction, position)) {
            if (owner.frame) increment(owner.frame, -1);
            owner.step = 1;
            return;
        }
        if (!player.field8A8) throw std::runtime_error("Original cobweb suction direction is unavailable.");
        if (dot(*player.field8A8, axis) >= -0.3f) {
            increment(owner.frame, owner.step);
            if (owner.frame >= 55) {
                owner.frame = 55;
                owner.step = -1;
            } else if (owner.frame < 1) {
                owner.frame = 0;
                owner.step = 1;
            }
        } else {
            increment(owner.frame);
            if (owner.frame >= 55) {
                if (!player.field8A4) throw std::runtime_error("Original cobweb suction position is unavailable.");
                owner.flags |= 0x40;
                for (unsigned i = 0; i < 3; ++i) position[i] -= (*player.field8A4)[i];
                owner.inverseDistance = 1.0f / square(position);
                owner.anchor = *player.field8A4;
            }
        }
        return;
    }
    Vector left{matrix[0], matrix[4], matrix[8]};
    for (unsigned i = 0; i < 3; ++i) position[i] = 0.8f * position[i] + (1.0f - 0.8f) * owner.anchor[i];
    if (player.field8B4) for (unsigned i = 0; i < 3; ++i) axis[i] = owner.anchor[i] - position[i];
    normalize(axis);
    auto up = cross(axis, left);
    normalize(up);
    left = cross(up, axis);
    normalize(left);
    for (unsigned i = 0; i < 3; ++i) {
        matrix[i * 4] = left[i];
        matrix[i * 4 + 1] = up[i];
        matrix[i * 4 + 2] = axis[i];
        matrix[i * 4 + 3] = position[i];
        position[i] -= owner.anchor[i];
    }
    const float distance = square(position);
    float scale = (4.0f * distance) * owner.inverseDistance;
    if (scale > 1) scale = 1;
    owner.model->scale.fill(scale);
    increment(owner.frame);
    if (!transform) throw std::runtime_error("Original cobweb animation is unavailable.");
    if (owner.frame >= transform->frameCount || distance < 500) {
        if (!player.field8A4 || !player.field8A8) throw std::runtime_error("Original cobweb particle attachment is unavailable.");
        owner.model->scale = {1, 1, 1};
        owner.flags |= 0x80;
        particles.create(0, *player.field8A4, *player.field8A8);
        particles.create(1, *player.field8A4, *player.field8A8);
    }
}
