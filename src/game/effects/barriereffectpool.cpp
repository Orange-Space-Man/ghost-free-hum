#include "game/effects/barriereffectpool.h"
#include "jlib/j3d/j3dmaterialcalculation.h"
#include "jlib/j3d/j3dmaterialdisplaylist.h"
#include "jlib/j3d/j3dtextureanimation.h"

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

bool modeleffects::BarrierPool::prepare(const roomlocation::Rooms& rooms, const roomtraversal::State& visibility,
    const j3dmodel::Matrix& view, const j3dmodel::TransformTables& tables, unsigned lightMask,
    const std::array<std::unique_ptr<j3dmodel::DrawBuffer>, 2>& buffers) {
    auto& owner = static_cast<BarrierOwner&>(*owners[0]);
    if ((owner.flags & 5) != 5) return false;
    if (!owner.model || !textureAnimation) throw std::runtime_error("Original barrier drawing requires its loaded resources.");
    auto& instance = *owner.model;
    const auto& matrix = instance.matrix;
    const auto room = rooms.resolve({matrix[3], matrix[7], matrix[11]}, 0xffffffff);
    if ((visibility.record(room, 0).flags & 3) != 1) return false;
    for (auto& entry : instance.model->materials.entries) {
        auto& material = *entry;
        if (!(material.identityFlags & 0xc0000000) && material.original) {
            material.identityFlags |= 0x80000000;
            material.mode = material.original->mode;
            material.fieldE = material.original->fieldE;
            material.animation.reset();
        }
        auto& channel = material.color->channels[0];
        channel = static_cast<std::uint16_t>((channel & 0xffc3) | ((lightMask << 2) & 0x3c));
        channel = static_cast<std::uint16_t>((channel & 0x87ff) | ((lightMask << 7) & 0x1800));
    }
    textureAnimation->frame = owner.animationFrame;
    j3dmodel::calculateBasic(instance, tables);
    const auto& geometry = instance.model->geometry;
    std::vector<int> pending;
    if (geometry.hierarchy.root >= 0) pending.push_back(geometry.hierarchy.root);
    while (!pending.empty()) {
        const auto joint = pending.back();
        pending.pop_back();
        const auto& links = geometry.hierarchy.joints[joint];
        if (links.sibling >= 0) pending.push_back(links.sibling);
        if (links.child >= 0) pending.push_back(links.child);
        const auto& jointMatrix = instance.jointMatrices[geometry.joints[joint].index];
        for (int index = links.material; index >= 0; index = geometry.hierarchy.materials[index].next) {
            auto& packet = instance.materials.at(index);
            auto& material = *packet.material;
            j3dmodel::animateTextureMatrices(material);
            j3dmodel::calculateMaterial(material, jointMatrix, view, instance.field8 & 12, tables);
            auto& buffer = *buffers[(material.mode & 3) ? 0 : 1];
            if (buffer.sortType != 0) throw std::runtime_error("Original barrier requires material-sort drawing.");
            packet.materialIdentity = material.identity;
            packet.identityFlags = material.identityFlags;
            if (buffer.entryMaterial(packet, geometry.textureOffset)) j3dmodel::updateMaterialPacket(packet);
        }
    }
    j3dmodel::calculateViewMatrices(instance, view);
    return true;
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
