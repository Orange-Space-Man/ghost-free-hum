#include "game/player/playerfurnitureowner.h"

#include <stdexcept>

playerinteraction::FurnitureOwner::FurnitureOwner(playeractor::Player& player, playerevents::Dispatcher& events,
    playermodels::SharedModels& shared, roomfurniture::Owner& furniture, const gameflags::State& flags,
    jaudio::PositionalPlayback& playback, particles::EffectFactory& effects, const std::vector<std::uint8_t>& executable) :
    player(player), furniture(furniture), playback(playback), effects(effects), binding(events, 0), cache(executable, furniture, flags),
    sheets(*player.components->grids, shared, furniture, executable), frame(executable) {
    if (events.player(0) != &player) throw std::invalid_argument("Furniture sheets require the original player-zero owner.");
}

void playerinteraction::FurnitureOwner::update(const roomlocation::Rooms& rooms, enemies::Manager& enemies,
    const actormodel::Catalog& catalog, const actorcollision::Manager& collision, const mapcollision::State& map,
    unsigned mapMode, const std::function<std::uint32_t()>& random) {
    FurnitureServices services{cache, furniture, rooms, enemies, catalog, collision, map, mapMode,
        [this](const playerscanner::SheetInput& input) { sheets.initialize(input, false); }, random};
    frame.run(player, services);
}

void playerinteraction::FurnitureOwner::updateSheets() { sheets.update(*this); }

const playersheets::SuctionOwner* playerinteraction::FurnitureOwner::owner(int index) {
    if (index != 0) throw std::out_of_range("Furniture sheet update requires player zero.");
    return &binding.owner();
}

bool playerinteraction::FurnitureOwner::holding(int index) {
    if (index != 0) throw std::out_of_range("Furniture sheet feedback requires player zero.");
    return binding.holding();
}

bool playerinteraction::FurnitureOwner::pulling(int index) {
    if (index != 0) throw std::out_of_range("Furniture sheet feedback requires player zero.");
    return binding.pulling();
}

void playerinteraction::FurnitureOwner::sound(unsigned id, const playerscanner::Vector& position) { playback.play(id, position); }
void playerinteraction::FurnitureOwner::completion(const playerevents::Payload& payload) { binding.complete(payload); }

void playerinteraction::FurnitureOwner::rustle(unsigned id, const playerscanner::Vector& position, float volume) {
    auto* handle = playback.play(id, position);
    if (handle) playback.animationServices(position).volume(*handle, volume);
}

bool playerinteraction::FurnitureOwner::requestPull(playersheets::Grid& sheet, const playerscanner::Vector& center,
    float range, float elasticity) { return binding.request(sheet, center, range, elasticity); }

void playerinteraction::FurnitureOwner::releasePull(playersheets::Grid& sheet) { binding.release(sheet); }

std::uint32_t playerinteraction::FurnitureOwner::roomColor(const playerscanner::Vector&) {
    throw std::runtime_error("Bed-cover animation requires the original room color service.");
}

void playerinteraction::FurnitureOwner::completionEffect(unsigned channel, const playerscanner::Vector& position,
    const playerscanner::Vector& direction) {
    effects.createCapture(channel, position, direction);
}

void playerinteraction::FurnitureOwner::impact(float, const playerscanner::Vector&, unsigned) {
    throw std::runtime_error("Elastic sheet impact requires the original furniture response service.");
}

bool playerinteraction::FurnitureOwner::elasticReaction(const playerscanner::Vector&, unsigned, unsigned) {
    throw std::runtime_error("Elastic sheet feedback requires the original player reaction service.");
}

void playerinteraction::FurnitureOwner::secondaryTexture() {
    throw std::logic_error("Sheet texture replacement belongs to its retained frame.");
}
