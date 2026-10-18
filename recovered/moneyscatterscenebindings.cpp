#include "moneyscatterscenebindings.h"
#include "moneyscattermodels.h"

#include <stdexcept>
#include <utility>

moneyscatter::SceneBindings::SceneBindings(SceneDependencies dependencies) : dependencies(std::move(dependencies)) {
    if (!this->dependencies.dispatch || !this->dependencies.attraction || !this->dependencies.positionalSound)
        throw std::runtime_error("Money scene requires original player dispatch, attraction and positional sound routes.");
}

std::uint32_t moneyscatter::SceneBindings::random() {
    auto& state = dependencies.randomState;
    state = state * 0x19660du + 0x3c6ef35fu;
    return state;
}

std::uint32_t moneyscatter::SceneBindings::roomAt(const Vector& position, std::uint32_t previousRoom) {
    return dependencies.rooms.resolve(position, previousRoom);
}

std::uint16_t moneyscatter::SceneBindings::visibilityFlags(std::uint32_t room) const {
    return dependencies.visibility.record(room, 0).flags;
}

mapcollision::SphereContact moneyscatter::SceneBindings::sweep(const Vector& position, float radius, const Vector& velocity) {
    return dependencies.collision.sweepSphere(position, radius, velocity);
}

moneyscatter::Vector& moneyscatter::SceneBindings::playerPosition() { return dependencies.playerPosition; }
float moneyscatter::SceneBindings::playerTop() { return dependencies.playerContext.height; }

void moneyscatter::SceneBindings::playerEvent(unsigned event, const playerevents::Payload& payload) {
    dependencies.dispatch(event, payload);
}

void moneyscatter::SceneBindings::collectionEffect(const Vector& position) {
    dependencies.effects.createCoinCollection(0, position);
}

std::uint32_t moneyscatter::SceneBindings::roomFlags(std::uint32_t room) {
    return dependencies.roomFlags.at(room >> 24);
}

void moneyscatter::SceneBindings::notifyPlacedCollection(std::uint32_t room) {
    const unsigned index = room >> 24;
    dependencies.roomFlags.at(index) |= 0x40;
    if (dependencies.mapMode == 2) {
        const unsigned savedIndex = dependencies.rooms.entries().at(index).id & 0xff;
        dependencies.savedRoomFlags.at(savedIndex) |= 8;
    }
}

int moneyscatter::SceneBindings::suction(float radius, Particle& particle) {
    return dependencies.attraction(radius, particle);
}

moneyscatter::Vector moneyscatter::SceneBindings::suctionPosition() {
    if (!dependencies.playerContext.suction.position) throw std::runtime_error("Money suction requires the refreshed original scene position.");
    return *dependencies.playerContext.suction.position;
}

float moneyscatter::SceneBindings::suctionMinimumScale() { return dependencies.minimumScale; }

void moneyscatter::SceneBindings::bounceSound(const Vector& position) {
    dependencies.positionalSound(0x1841, position, 0);
}

void moneyscatter::SceneBindings::sparkle(const Vector& position) { dependencies.sparkles.emit(position); }
void moneyscatter::SceneBindings::updateModel(Particle& particle) { moneyscatter::updateModel(particle); }
