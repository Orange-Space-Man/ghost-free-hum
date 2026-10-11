#include "game/actors/actorregistry.h"
#include "game/actors/actorposeblend.h"

#include <stdexcept>

actors::Registry::Registry(const std::vector<std::uint8_t>& executable, const Construction& construction,
    players::Manager& manager, players::Globals& sharedGlobals, gameplaycamera::CameraOwner& camera,
    playeractor::Globals& globals, const actorcollision::Catalog& collisions,
    const std::vector<std::uint32_t>& roomFlags, unsigned mapMode, bool flag3D) : construction(construction),
    environment{collisions, objects, roomFlags, mapMode, flag3D},
    playerConstruction{executable, construction, manager, sharedGlobals, camera, globals, environment} {
}

actors::Registry::~Registry() {
    destroyActors();
}

actors::MoveActor& actors::Registry::create(unsigned type, unsigned capacity) {
    if (used >= objects.size()) throw std::runtime_error("Original actor registry capacity exceeded.");
    if (owned[used]) throw std::runtime_error("Actor registry reused before its previous actors were destroyed.");
    const unsigned id = used;
    if (type == 0) owned[id] = std::make_unique<playeractor::Player>(playerConstruction, id, type, capacity);
    else if (type == 1 || type == 0xb3) owned[id] = std::make_unique<playeractor::Alternate>(playerConstruction, id, type, capacity);
    else owned[id] = std::make_unique<MoveActor>(construction, id, type, capacity);
    objects[id] = owned[id].get();
    ++used;
    return *objects[id];
}

void actors::Registry::resetIndex() {
    used = 0;
}

void actors::Registry::destroyActors() {
    for (unsigned i = 0; i < used; ++i) owned[i].reset();
}

void actors::Registry::updateCaptures(actorcapture::Motion& motion, const playermodels::VacuumPath* path,
    const actorcapture::Vector* suctionDirection, actormodel::Matrix& retainedTransform,
    const actorcapture::Vector* originalScratch, const float* originalRegister, const bool* emptyCompletion) {
    for (unsigned i = 0; i < used; ++i) {
        auto& actor = *objects[i];
        if (actor.flags & 0x80000) actor.updateCapture(motion, path, suctionDirection, retainedTransform,
            originalScratch, originalRegister, emptyCompletion);
    }
}

actors::MoveActor* actors::Registry::actor(unsigned index) const {
    return objects.at(index);
}

unsigned actors::Registry::count() const {
    return used;
}

const std::array<actors::MoveActor*, 128>& actors::Registry::entries() const {
    return objects;
}


void actors::Registry::updateContacts(enemies::Manager& manager, const actorcollision::Manager& collision,
    const playerresources::RecordPool44& pool, const mapcollision::State& map,
    const actorcollision::Catalog& catalog, gameflags::State& flags) {
    for (unsigned i = 0; i < used; ++i) objects[i]->field9C = -1;
    for (unsigned i = 0; i < used; ++i) objects[i]->fieldA0 = -1;
    for (unsigned i = 0; i < used; ++i) objects[i]->updateContactFrame(*this, manager, collision, pool, map, catalog, flags);
}


void actors::Registry::updatePoses(const std::function<void()>& captureUpdate,
    const std::function<void(MoveActor&)>& animationSound) {
    for (unsigned i = 0; i < used; ++i) objects[i]->samplePose();
    for (unsigned i = 0; i < used; ++i) {
        auto& actor = *objects[i];
        if (actor.flags & 0x60) continue;
        actormodel::blendPose(*actor.model);
        actormodel::updateEnvelopes(*actor.model);
    }
    for (unsigned i = 0; i < used; ++i) objects[i]->updateMorph();
    for (unsigned i = 0; i < used; ++i) objects[i]->updateMaterials();
    for (unsigned i = 0; i < used; ++i) objects[i]->finishFrame();
    captureUpdate();
    for (unsigned i = 0; i < used; ++i) objects[i]->copyLinkedPose();
    for (unsigned i = 0; i < used; ++i) objects[i]->updateChildren();
    for (unsigned i = 0; i < used; ++i) if (!(objects[i]->flags & 0x60)) animationSound(*objects[i]);
}
