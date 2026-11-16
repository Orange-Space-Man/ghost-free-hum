#include "game/enemies/enemyghostmovement.h"
#include <cmath>
#include <stdexcept>
#include <utility>

float ghostmotion::vertical(const actormodel::InitialPose& pose, float amplitude, float maximum,
    float height, float floor, float ceiling, float targetHeight, unsigned actor, std::uint32_t frame, unsigned phaseStep) {
    if (targetHeight < 0) targetHeight = 0.5f * (ceiling - floor);
    const float relative = (height - floor) / targetHeight;
    const auto phase = static_cast<std::uint16_t>(phaseStep * (frame + actor * 3u));
    float result = amplitude * pose.horizontalDirection(phase)[0];
    if (!(relative >= 0.6f)) result = static_cast<float>(double(result) + 0.5 * double(amplitude));
    else if (relative >= 1.4f) {
        if (relative >= 1.8f) result -= amplitude;
        else result = static_cast<float>(double(result) - 0.5 * double(amplitude));
    }
    if (maximum <= std::abs(result)) result = maximum * (result > 0 ? 1.0f : result < 0 ? -1.0f : 0.0f);
    return result;
}

std::array<float, 3> ghostmotion::horizontal(const actormodel::InitialPose& pose,
    const std::array<float, 3>& velocity, float acceleration, float maximum, std::uint16_t angle) {
    const auto direction = pose.horizontalDirection(angle);
    const float projected = velocity[0] * direction[0] + velocity[2] * direction[2];
    if (projected <= 0) acceleration *= 4;
    const float accelerated = projected + acceleration;
    const float speed = maximum >= accelerated ? accelerated : maximum;
    return {direction[0] * speed, velocity[1], direction[2] * speed};
}

enemystrategy::RandomMovement::RandomMovement(RandomMovementServices value) : services(std::move(value)) {
    if (!services.random || !services.frame)
        throw std::runtime_error("Original ghost movement requires its shared random and simulation clocks.");
}

void enemystrategy::RandomMovement::initializeBehavior() {
    if (state == 3) services.actor.previousRotation[1] = static_cast<std::int16_t>(services.character.field94c);
}

void enemystrategy::RandomMovement::move(float height) {
    auto& actor = services.actor;
    const auto& parameters = services.character.fields93c;
    const auto floor = services.collision.floorHeight(actor.position);
    const auto ceiling = height < 0 ? services.collision.ceilingHeight(actor.position) : 0.0f;
    auto velocity = actor.velocity;
    velocity[1] = ghostmotion::vertical(services.pose, parameters[1], parameters[3], actor.position[1],
        floor, ceiling, height, actor.id(), services.frame());
    velocity = ghostmotion::horizontal(services.pose, velocity, parameters[0], parameters[2],
        static_cast<std::uint16_t>(actor.rotation[1]));
    actor.setVelocity(velocity);
}

void enemystrategy::RandomMovement::behavior() {
    auto& actor = services.actor;
    auto& character = services.character;
    switch (state) {
    case 0:
        if (services.source.table && services.source.row >= 0) {
            std::uint32_t height = 0;
            services.source.table->getValue(services.source.row, "floating_height", height);
            floatingHeight = static_cast<float>(height);
        } else {
            const auto floor = services.collision.floorHeight(actor.position);
            const auto ceiling = services.collision.ceilingHeight(actor.position);
            floatingHeight = 0.5f * (ceiling - floor);
        }
        setNextState(1);
        break;
    case 1:
        if (frames > 9) contactEnabled = true;
        polygon = actor.fieldB0;
        if (contactEnabled && polygon >= 0) {
            setNextState(2);
        } else {
            move(floatingHeight);
            if (services.random() % 100 < 2) setNextState(3);
        }
        break;
    case 2:
        if (polygon < 0) {
            character.field94c = static_cast<std::uint16_t>(actor.rotation[1] + (services.random() & 0x7fff) + 0x4000);
        } else {
            std::array<float, 3> normal;
            if (!services.collision.polygonNormal(polygon, normal))
                throw std::runtime_error("Original ghost movement requires its contact polygon.");
            if (normal[1] < 0) {
                contactEnabled = false;
                setNextState(1);
                return;
            }
            character.field94c = static_cast<std::uint16_t>(services.pose.angle(normal[2], normal[0]) +
                (services.random() & 0x7fff) + 0xc000);
        }
        contactEnabled = false;
        setNextState(3);
        break;
    case 3:
        move(-1);
        actor.previousRotation[1] = static_cast<std::int16_t>(character.field94c);
        if (static_cast<std::uint16_t>(actor.rotation[1]) == character.field94c) setNextState(1);
        break;
    }
}
