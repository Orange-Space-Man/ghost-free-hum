#include "game/player/playerliferecovery.h"
#include "game/player/playeractorcomponents.h"

#include <stdexcept>

unsigned playerevents::loseMoney(playeractor::Player& player, std::uint32_t amount, LifeEnvironment& environment) {
    auto& money = player.state.fields12F4[9];
    if (static_cast<std::int32_t>(amount) <= 0 || static_cast<std::int32_t>(money) <= 0) return 0;
    const auto count = money < amount ? money : amount;
    money -= count;
    if (!environment.scatterMoney) throw std::runtime_error("Original player money manager is not bound.");
    const auto& weapon = *player.components->weapon->first;
    environment.scatterMoney(weapon.position, weapon.field610, static_cast<std::uint8_t>(count), 150, 30, 0);
    return 1;
}

void playerevents::setLife(playeractor::Player& player, std::int16_t life, LifeEnvironment& environment) {
    const auto previous = static_cast<std::int16_t>(player.life);
    if (life == previous) return;
    int next = life < 0 ? 0 : life;
    const auto maximum = static_cast<std::int16_t>(player.state.maximumLife);
    if (maximum < next) next = maximum;
    player.life = static_cast<std::uint16_t>(next);
    if (next < previous) {
        const int lost = previous - next;
        player.playFeedback(lost < 2 ? 15 : lost < 10 ? 16 : 17);
        auto position = player.position;
        if (player.type() == 0) {
            position = {};
            if (player.model) {
                const auto& joint = player.model->matrix(0, 13);
                const auto& matrix = player.matrix;
                for (unsigned i = 0; i < 3; ++i) position[i] =
                    ((matrix[i * 4] * joint[3] + matrix[i * 4 + 1] * joint[7]) + matrix[i * 4 + 2] * joint[11]) + matrix[i * 4 + 3];
            }
        }
        environment.effects.createLifeLoss(0, position, lost);
        const unsigned requested = lost == 1 ? (player.life & 1) : unsigned(lost >> 1);
        loseMoney(player, requested, environment);
        environment.sound(0x839, player.position);
        if (next < 1) {
            if (!environment.deathSound) throw std::runtime_error("Original death audio is not bound.");
            environment.deathSound(player.position);
        }
    } else if (previous < next) environment.sound(0x282b, player.position);
    environment.audioLife = static_cast<std::int16_t>(player.life);
}

void playerevents::recoverLife(playeractor::Player& player, std::uint32_t amount, LifeEnvironment& environment) {
    if (!amount) setLife(player, static_cast<std::int16_t>(player.state.maximumLife), environment);
    else {
        const auto sum = amount + static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(player.life)));
        float value = static_cast<float>(static_cast<std::int32_t>(sum));
        const float maximum = static_cast<float>(static_cast<std::int16_t>(player.state.maximumLife));
        if (value < 0) value = 0;
        if (maximum < value) value = maximum;
        setLife(player, static_cast<std::int16_t>(static_cast<std::int32_t>(value)), environment);
    }
    environment.sound(0x83d, player.position);
}
