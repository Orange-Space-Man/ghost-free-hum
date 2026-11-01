#include "playersheetplayerbinding.h"
#include "game/player/playeractor.h"

#include <stdexcept>

playersheets::PlayerBinding::PlayerBinding(playerevents::Dispatcher& dispatcher, int index) :
    dispatcher(dispatcher), index(index) {}

playeractor::Player& playersheets::PlayerBinding::player() const {
    auto* value = dispatcher.player(index);
    if (!value) throw std::runtime_error("Cloth requires its original manager-owned player.");
    return *value;
}

const playersheets::SuctionOwner& playersheets::PlayerBinding::owner() {
    const auto& view = player().view;
    if (!view.field8A4 || !view.field8A8) throw std::runtime_error("Cloth requires the published weapon position and direction.");
    binding.emplace(SuctionOwner{view.field8B4, *view.field8A4, *view.field8A8});
    return *binding;
}

bool playersheets::PlayerBinding::holding() const { return player().view.fieldF10 != 0; }
bool playersheets::PlayerBinding::pulling() const { return player().view.fieldF11 != 0; }

bool playersheets::PlayerBinding::request(Grid& sheet, const playerscanner::Vector& center, float range, float elasticity) {
    playerevents::Payload payload;
    payload.pull(&center, &sheet, range, elasticity);
    return dispatcher.send(index, 0x34, &payload) != 0;
}

void playersheets::PlayerBinding::release(Grid& sheet) {
    playerevents::Payload payload;
    payload.pull(nullptr, &sheet, 0, 0);
    dispatcher.send(index, 0x35, &payload);
}

void playersheets::PlayerBinding::complete(const playerevents::Payload& payload) {
    dispatcher.send(index, 0x13, &payload);
}
