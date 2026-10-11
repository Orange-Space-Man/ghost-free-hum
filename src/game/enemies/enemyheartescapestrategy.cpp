#include "game/enemies/enemyheartescapestrategy.h"

void enemystrategy::HeartEscape::groundedSound() {
    Heart::groundedSound();
    std::uint8_t escape = 0;
    if (!itemSource.table || itemSource.row < 0 ||
        !itemSource.table->getValue(itemSource.row, "IsEscape", escape) || !escape) return;
    auto& object = actor();
    const auto& position = *player().view.position;
    const float x = static_cast<float>(double(object.position[0]) - double(position[0]));
    const float z = object.position[2] - position[2];
    const auto heading = environment.suction.pose.angle(z, x);
    const auto direction = environment.suction.pose.horizontalDirection(heading);
    const float speed = object.velocity[1] > 4 ? 14.0f : 0.0f;
    object.setVelocity({speed * direction[0], object.velocity[1], speed * direction[2]});
}
