#include "game/player/playermanager.h"
#include "game/actors/actorregistry.h"

#include <stdexcept>

void players::Manager::bindActors(enemies::Manager& enemies, actors::Registry& actors,
    const std::array<playerrecord::Record*, 2>& playerRecords, playerinput::Pad* pad,
    void* field798, std::int32_t& audioLife) {
    if (!enemies.actorsInitialized()) throw std::runtime_error("Player binding requires the original actor pool.");
    for (unsigned i = 0; i < enemies.playerCount(); ++i) {
        const auto first = enemies.createActor(6, -1);
        const auto second = enemies.createActor(5, -1);
        const auto third = enemies.createActor(10, -1);
        auto* firstActor = static_cast<std::int32_t>(first) < 0 ? nullptr : actors.actor(first);
        auto* secondActor = static_cast<std::int32_t>(second) < 0 ? nullptr : actors.actor(second);
        auto* thirdActor = static_cast<std::int32_t>(third) < 0 ? nullptr : actors.actor(third);
        const auto flags = static_cast<std::uint16_t>((firstActor && secondActor && thirdActor ? 0xc : 0) |
            (i == 0 ? 0x13 : 0x12));
        auto* playerPad = i == 0 ? pad : nullptr;
        controllers.at(i)->reset(playerPad);
        auto* player = dynamic_cast<playeractor::Player*>(actors.actor(enemies.appearPoint(i).actor));
        if (!player) throw std::runtime_error("Original Player binding references a non-Player actor.");
        auto* owner = dynamic_cast<characterowner::Owner*>(enemies.activeOwner(*player));
        if (!owner) throw std::runtime_error("Original Player binding requires its active character owner.");
        player->bind({static_cast<std::int32_t>(i), playerRecords.at(i), flags, playerPad, field798,
            controllers[i].get(), firstActor, secondActor, thirdActor}, *owner, audioLife);
    }
}
