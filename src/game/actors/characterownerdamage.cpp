#include "game/actors/characterownerdamage.h"

#include <stdexcept>

void characterowner::updateDamage(Owner& owner, const charactercreation::Properties& properties, const actormodel::Catalog& models,
    playerrecord::Record& player, const std::function<void(unsigned, const std::array<float, 3>&)>& effect) {
    auto& actor = dynamic_cast<actors::MoveActor&>(owner.actor());
    auto& state = owner.state();
    if (properties.group(actor.actorType) == 3) {
        const int index = properties.savedIndex(actor.actorType);
        if (index >= 0) {
            player.at(4 + index * 2) = static_cast<std::uint8_t>(actor.life >> 8);
            player.at(5 + index * 2) = static_cast<std::uint8_t>(actor.life);
        }
    }
    if (static_cast<std::int32_t>(state.field8a0) > 0) --state.field8a0;
    const auto life = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(actor.life)));
    if (!state.field8a0 && static_cast<std::int32_t>(life - state.life) < 0 && models.type(actor.actorType).field4) {
        if (!effect) throw std::runtime_error("Original owner damage particle delivery is unbound.");
        effect(0, actor.position);
        state.field8a0 = 6;
    }
    state.life = life;
}
