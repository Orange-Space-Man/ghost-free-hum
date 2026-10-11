#include "game/enemies/enemymanagerruntime.h"

#include <stdexcept>

namespace {
    bool bit(const playercomponents::BitBuffer* buffer, unsigned index) {
        if (!buffer || !buffer->data || (index >> 3) >= buffer->bytes) {
            throw std::runtime_error("Original actor admission bitfield is unavailable.");
        }
        return (buffer->data[index >> 3] & (1u << (index & 7))) != 0;
    }
}

enemies::ManagerRuntime::ManagerRuntime(const enemystrategy::Catalog& strategies, characterowner::Runtime& owners,
    actors::Registry& actors, const roomlocation::Rooms& rooms, const roomtraversal::State& visibility,
    const std::vector<std::uint32_t>& roomFlags, const gameflags::State& flags,
    const unsigned& mapMode, std::uint32_t& randomState) : strategies(strategies), owners(owners), actors(actors),
    rooms(rooms), visibility(visibility), roomFlags(roomFlags), flags(flags), mapMode(mapMode), randomState(randomState),
    sentinel(characterowner::Kind::Null, strategies, owners) {}

void enemies::ManagerRuntime::bind(Manager& value) { manager = &value; }

std::unique_ptr<enemies::Owner> enemies::ManagerRuntime::constructOwner(unsigned constructor, unsigned) {
    return characterowner::construct(constructor, strategies, owners);
}

void enemies::ManagerRuntime::resetActorIndex() { actors.resetIndex(); }

enemies::Actor& enemies::ManagerRuntime::constructActor(unsigned type, unsigned capacity) {
    return actors.create(type, capacity);
}

enemies::Owner& enemies::ManagerRuntime::absentOwner() { return sentinel; }

unsigned enemies::ManagerRuntime::resolveRoom(const std::array<float, 3>& position) { return rooms.resolve(position); }

bool enemies::ManagerRuntime::roomVisible(unsigned room, unsigned buffer) {
    return (visibility.record(room, buffer).flags & 1) != 0;
}

const playeractor::View& enemies::ManagerRuntime::player(unsigned index) const {
    if (!manager) throw std::runtime_error("Original actor runtime has no attached EnManager.");
    auto* actor = dynamic_cast<playeractor::Player*>(actors.actor(manager->appearPoint(index).actor));
    if (!actor) throw std::runtime_error("Original actor runtime requires an initialized Player view.");
    return actor->view;
}

bool enemies::ManagerRuntime::doorOpened(unsigned index, unsigned door) { return bit(player(index).flags, door); }

bool enemies::ManagerRuntime::typeCaptured(unsigned index, unsigned type) { return bit(player(index).bits[0], type); }

bool enemies::ManagerRuntime::roomCleared(unsigned roomGroup) {
    return !(mapMode == 2 && flags.value(0x3d)) && (roomFlags.at(roomGroup) & 0x8000) != 0;
}

std::uint32_t enemies::ManagerRuntime::random() {
    randomState = randomState * 0x19660d + 0x3c6ef35f;
    return randomState;
}

std::array<float, 3> enemies::ManagerRuntime::playerPosition(unsigned index) {
    const auto* position = player(index).position;
    if (!position) throw std::runtime_error("Original Player view has no position.");
    return *position;
}

unsigned enemies::ManagerRuntime::playerRoom(unsigned index) { return player(index).previousRoom; }

bool enemies::ManagerRuntime::playerSpecialState(unsigned index) { return player(index).fieldF2C != 0; }
