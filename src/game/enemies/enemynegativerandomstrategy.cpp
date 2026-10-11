#include "game/enemies/enemynegativerandomstrategy.h"
#include "game/enemies/enemystrategyfactory.h"

void enemystrategy::NegativeRandom::groundedSound() {
    Negative::groundedSound();
    auto& object = actor();
    const auto direction = environment.suction.pose.horizontalDirection(static_cast<std::uint16_t>(factory.random()));
    const float speed = object.velocity[1] > 4 ? 14.0f : 0.0f;
    object.setVelocity({speed * direction[0], object.velocity[1], speed * direction[2]});
}
