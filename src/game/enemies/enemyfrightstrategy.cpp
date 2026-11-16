#include "game/enemies/enemyfrightstrategy.h"
#include <stdexcept>
#include <utility>

enemystrategy::Fright::Fright(FrightServices value, float valueRate) : services(std::move(value)), rate(valueRate) {
    if (!services.playerPosition || !services.beginAttack || !services.scaleAttack || !services.frightenPlayer ||
        !services.projectFloor || !services.drawAttack || !services.recoveryDuration)
        throw std::runtime_error("Original fright requires its player, effect, floor and parameter services.");
}

void enemystrategy::Fright::animation(const char* name, unsigned duration) {
    if (services.actor.selectAnimation(name)) {
        services.character.field8cc = duration;
        services.character.field8d0 = 0;
    }
}

void enemystrategy::Fright::initializeBehavior() {
    auto& actor = services.actor;
    switch (state) {
    case 1: {
        animation("APPEAR1", 0);
        actor.animationSpeed = rate;
        actor.flags &= 0xffff3bff;
        services.character.field94e = 255;
        const auto divisor = static_cast<int>(30.0f / rate);
        if (divisor == 0) throw std::runtime_error("Original fright alpha rate has a zero divisor.");
        const auto step = static_cast<std::uint8_t>(255 / divisor + 1);
        actor.fieldE4 = (actor.fieldE4 & 0xffffff00) | 255;
        actor.fieldE8 = step ? step : 4;
        break;
    }
    case 2:
        animation("ODOKASI2", 4);
        success = false;
        services.beginAttack();
        break;
    case 3: animation("ODOKASI2", 0); break;
    case 4:
    case 6: {
        const int duration = services.recoveryDuration(state == 4);
        if (duration < 1) setNextState(state == 4 ? 0x100 : 0x101);
        else animation(state == 4 ? "TYAKASU" : "ORORO", static_cast<unsigned>(duration));
        break;
    }
    case 5: animation("SIPPAI", 0); break;
    }
}

std::array<float, 3> enemystrategy::Fright::facePlayer() {
    const auto player = services.playerPosition(services.character.field950);
    const auto& position = services.actor.position;
    const std::array<float, 3> delta{player[0] - position[0], player[1] - position[1], player[2] - position[2]};
    services.actor.previousRotation[1] = static_cast<std::int16_t>(services.pose.angle(delta[2], delta[0]));
    return delta;
}

void enemystrategy::Fright::attack() {
    const auto delta = facePlayer();
    const float radius = static_cast<float>(frames) / 32.0f;
    if (static_cast<float>(frames) < 32.0f && delta[0] * delta[0] + delta[2] * delta[2] < radius * radius)
        success = services.frightenPlayer(services.character.field950);
    const float scale = static_cast<float>(double(0.1f) * double(radius));
    services.scaleAttack({scale, 1, scale});
    auto position = services.actor.position;
    services.projectFloor(position);
    services.drawAttack(radius, 10, position, {255, 0, 0, 64});
}

void enemystrategy::Fright::behavior() {
    if (state == 0) { setNextState(1); return; }
    if (state < 1 || state > 6) return;
    if (state == 2) {
        services.actor.setVelocity({0, 0, 0});
        attack();
        if (services.character.field8d0 & 1) {
            services.character.field894 = nullptr;
            services.character.field898 = 0xffffffff;
            setNextState(success ? 4 : 5);
        }
        return;
    }
    facePlayer();
    services.actor.setVelocity({0, 0, 0});
    if (!(services.character.field8d0 & 1)) return;
    switch (state) {
    case 1: setNextState(2); break;
    case 3: setNextState(4); break;
    case 4: setNextState(0x100); break;
    case 5: setNextState(6); break;
    case 6: setNextState(0x101); break;
    }
}

bool enemystrategy::Fright::attacking() const { return state > 1; }
bool enemystrategy::Fright::succeeded() const { return success; }
