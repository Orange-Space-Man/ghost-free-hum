#include "playerfurnitureframe.h"
#include "game/actors/actorcollisionvolume.h"
#include "resources/dolresources.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    unsigned word(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }

    float scalar(unsigned bits) {
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }

    void normalize(std::array<float, 3>& direction) {
        const float length = std::sqrt((direction[0] * direction[0] + direction[1] * direction[1]) + direction[2] * direction[2]);
        if (length >= 0.0001f) for (auto& value : direction) value *= 1 / length;
        else direction.fill(0);
    }

    void react(playeractor::Player& player, unsigned reaction) {
        std::array<std::uint32_t, 12> payload{};
        payload[2] = reaction;
        player.components->actions->request(5, &payload);
    }

    actorcollision::Volume volume(const actors::MoveActor& actor, const actorcollision::Manager& collision) {
        if (actor.field788) return *actor.field788;
        const auto& element = *collision.elements.at(actor.actorType);
        return {element.field8, element.secondary, element.extended};
    }
}

playerinteraction::FurnitureFrame::FurnitureFrame(const std::vector<std::uint8_t>& executable) :
    reachRatio(scalar(word(executable, 0x804db570))), collisionScale(scalar(word(executable, 0x804db574))),
    wallLimit(scalar(word(executable, 0x804db578))), turnBlend(scalar(word(executable, 0x804d2bac))),
    actorEvent(word(executable, 0x804d2c84)) {}

bool playerinteraction::FurnitureFrame::actor(playeractor::Player& player, FurnitureServices& services) const {
    auto expanded = volume(player, services.collision);
    if (expanded.field8 != 1) return false;
    if (!expanded.secondary) throw std::runtime_error("Player interaction requires its original cylinder descriptor.");
    const float radius = scalar((*expanded.secondary)[0]) * collisionScale;
    std::memcpy(&(*expanded.secondary)[0], &radius, 4);
    for (const auto& slot : services.enemies.slots()) {
        if (slot.state != 2 || !slot.owner) continue;
        auto& target = dynamic_cast<actors::MoveActor&>(slot.owner->actor());
        if ((target.flags & 0xc000) || !(services.catalog.type(target.actorType).flags & 0x10)) continue;
        std::array<float, 3> direction;
        for (unsigned i = 0; i < 3; ++i) direction[i] = target.position[i] - player.position[i];
        const auto& facing = player.state.direction;
        if (((direction[0] * facing[0] + direction[1] * facing[1]) + direction[2] * facing[2]) <= 0) continue;
        if (!actorcollision::intersect(direction, expanded, player.position, volume(target, services.collision), target.position)) continue;
        react(player, 2);
        player.playFeedback(3);
        player.state.field1070 = 5;
        player.state.field1074 = target.position;
        player.state.field1074[1] = player.position[1];
        player.state.field109C = turnBlend;
        player.setVelocity({});
        slot.owner->event(characterevent::Event(actorEvent));
        return true;
    }
    return false;
}

bool playerinteraction::FurnitureFrame::wall(playeractor::Player& player, const mapcollision::State& collision) const {
    if (player.fieldB0 < 0 || !(player.position[0] < wallLimit)) return false;
    std::array<float, 3> normal;
    if (!collision.polygonNormal(player.fieldB0, normal)) return false;
    const auto& direction = player.state.direction;
    if (!(((normal[0] * direction[0] + normal[1] * direction[1]) + normal[2] * direction[2]) < 0)) return false;
    const auto range = collision.polygonHeightRange(player.fieldB0);
    if (!(player.position[1] + reachRatio * player.state.fieldF58 <= range[1] &&
        range[0] <= player.position[1] + player.state.fieldF58)) return false;
    player.sound.field64 = 0xff;
    player.state.field1070 = 7;
    player.state.field1090 = {-normal[0], 0, -normal[2]};
    normalize(player.state.field1090);
    player.state.field109C = turnBlend;
    react(player, 1);
    player.playFeedback(4);
    return true;
}

void playerinteraction::FurnitureFrame::run(playeractor::Player& player, FurnitureServices& services) const {
    auto& state = player.state;
    const auto& actions = *player.components->actions;
    const bool eligible = !state.field11A0 && actions.field310 != 2 && !state.flags1140[2];
    bool requested = eligible && state.field1130 == 0 && state.field1338 == 1 && player.components->controls->flagsC[4];
    bool pressed = requested && state.field1170 == 0;
    const bool automatic = eligible;
    if (pressed && actor(player, services)) pressed = requested = false;
    services.cache.refresh(player.fieldB4, services.initializeSheet);
    const auto& room = services.rooms.entries().at(player.fieldB4 >> 24);
    const playerscanner::FurnitureInput input{player.position, state.direction, state.fieldF54, state.fieldF58,
        reachRatio * state.fieldF58, (float(room.bounds[1]) + float(room.bounds[4])) * 0.5f, state.flags1330[1] != 0};
    const auto hit = pressed ? services.cache.press(input, player.components->grids->fields16BC, services.random) :
        automatic ? services.cache.automatic(input) : playerscanner::FurnitureInteraction{};
    if (hit.target < 0) {
        const bool unhandled = pressed ? !wall(player, services.map) : requested;
        if (unhandled && services.mapMode == 2) {
            player.components->attached->startFirstTransition(7, 1);
            player.components->attached->startSecondTransition(7, 1);
        }
        return;
    }
    if (pressed) {
        react(player, hit.reaction);
        services.furniture.markStruck(services.cache.row(hit.target), hit.reaction == 2);
        player.sound.field64 = hit.sound;
    }
    if (hit.invert) state.field1324 ^= 0x20;
    else {
        state.field1070 = 7;
        state.field1090 = hit.direction;
        state.field1090[1] = 0;
        normalize(state.field1090);
        state.field109C = turnBlend;
        player.setVelocity({});
    }
    if (hit.reaction == 1) player.playFeedback(4);
    else if (hit.reaction == 2) player.playFeedback(3);
    else if (hit.reaction == 3) player.playFeedback(5);
}
