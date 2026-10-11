#include "game/enemies/itemsuctioncontroller.h"

#include <cmath>
#include <stdexcept>

itemsuction::Controller::Controller(Environment& environment, std::uint8_t retainedRemaining) :
    remaining(retainedRemaining), environment(environment) {}

void itemsuction::Controller::initialize(const actors::MoveActor& actor, std::uint8_t useCountdown, std::uint8_t durationBase) {
    const auto room = environment.rooms.resolve(actor.position) >> 24;
    const auto& bounds = environment.rooms.entries().at(room).bounds;
    for (unsigned i = 0; i < 3; ++i) {
        minimum[i] = static_cast<float>(bounds[i]);
        maximum[i] = static_cast<float>(bounds[i + 3]);
    }
    acceleration = {};
    scale = originalScale = animationRate = 1;
    countdown = useCountdown;
    duration = 255;
    minimumScale = 0.3f;
    scaleDistance = 200;
    field42 = attracting = 0;
    parameters = {0.73f, 2500, durationBase};
    heightMargin = 0;
}

bool itemsuction::Controller::visible(const actors::MoveActor& actor) const {
    return (environment.visibility.record(actor.fieldB4, 0).flags & 1) != 0;
}

void itemsuction::Controller::begin(actors::MoveActor& actor) {
    if (attracting) return;
    attracting = 1;
    environment.events.attraction(actor.id());
}

void itemsuction::Controller::restrictHeight(const actors::MoveActor& actor, Vector& velocity) const {
    const float predicted = actor.position[1] + velocity[1];
    const float margin = heightMargin * scale;
    if (!(predicted <= maximum[1] - margin) || predicted < minimum[1] + margin) velocity[1] = 0;
}

bool itemsuction::Controller::update(actors::MoveActor& actor, double heightOffset, double arrivalRadius,
    const Vector& unwrittenDirection) {
    if (!visible(actor)) return false;
    auto position = actor.position;
    position[1] += static_cast<float>(heightOffset * scale);
    auto velocity = actor.velocity;
    const unsigned result = environment.force.apply(arrivalRadius, position, velocity, acceleration, remaining, duration,
        countdown != 0, parameters, unwrittenDirection);
    if (result == 2) {
        complete(actor);
        return true;
    }
    if (result == 1) {
        begin(actor);
        actor.animationSpeed = 2 * animationRate;
        for (unsigned i = 0; i < 3; ++i) velocity[i] += acceleration[i];
        restrictHeight(actor, velocity);
        for (auto& component : velocity) component *= 2;
        actor.setVelocity(velocity);
        orient(actor, velocity);
        updateScale(actor);
        return true;
    }
    recover(actor);
    return false;
}

bool itemsuction::Controller::updateVelocity(actors::MoveActor& actor, Vector& output, const Vector& unwrittenDirection) {
    if (!visible(actor)) return false;
    auto velocity = actor.velocity;
    const unsigned result = environment.force.apply(25, actor.position, velocity, acceleration, remaining, duration,
        countdown != 0, parameters, unwrittenDirection);
    if (result == 1) {
        begin(actor);
        actor.animationSpeed = 2 * animationRate;
        for (unsigned i = 0; i < 3; ++i) velocity[i] += acceleration[i];
        restrictHeight(actor, velocity);
        output = velocity;
        return true;
    }
    recover(actor);
    return false;
}

void itemsuction::Controller::orient(actors::MoveActor& actor, const Vector& velocity) {
    if (!environment.context.position) throw std::runtime_error("Item suction orientation requires the scene position.");
    const float horizontal = std::sqrt(velocity[0] * velocity[0] + velocity[2] * velocity[2]);
    actor.rotation[1] = static_cast<std::int16_t>(environment.pose.angle(horizontal, velocity[1]));
    actor.previousRotation[0] = static_cast<std::int16_t>(environment.pose.angle(
        actor.position[2] - (*environment.context.position)[2], actor.position[0] - (*environment.context.position)[0]));
    actor.previousRotation[2] = 0;
}

void itemsuction::Controller::updateScale(actors::MoveActor& actor) {
    if (!environment.context.position) throw std::runtime_error("Item suction scale requires the scene position.");
    Vector delta;
    for (unsigned i = 0; i < 3; ++i) delta[i] = actor.position[i] - (*environment.context.position)[i];
    const float distance = std::sqrt(std::fma(delta[2], delta[2], delta[0] * delta[0]) + delta[1] * delta[1]);
    if (!(distance <= scaleDistance)) scale = originalScale;
    else {
        const float target = minimumScale + (distance * (originalScale - minimumScale)) / scaleDistance;
        if (target >= scale || !duration || int(duration) - int(remaining) >= 30) scale = target;
        else {
            const float decayed = 0.96f * scale;
            scale = decayed >= target ? decayed : target;
        }
    }
    actor.scale = {scale, scale, scale};
}

void itemsuction::Controller::recoverScale(actors::MoveActor& actor) {
    scale *= 1.1f;
    if (originalScale < scale) scale = originalScale;
    actor.scale = {scale, scale, scale};
}

void itemsuction::Controller::recover(actors::MoveActor& actor) {
    recoverScale(actor);
    actor.animationSpeed = animationRate;
    actor.previousRotation[2] = 0;
    attracting = 0;
}

void itemsuction::Controller::complete(actors::MoveActor& actor) {
    if (completionKind != 4) {
        const auto position = actor.position;
        orient(actor, actor.velocity);
        updateScale(actor);
        Completion payload;
        if (completionKind == 0 || completionKind == 3) {
            payload.kind = 3;
            payload.amount = 1;
            payload.type = actor.type();
            if (completionKind == 3) payload.collectionBit = static_cast<std::int32_t>(completionValue);
        }
        else if (completionKind == 1) {
            payload.delayEnabled = 1;
            payload.delay = completionValue;
        }
        else if (completionKind == 2) environment.flags.setOn(static_cast<std::uint8_t>(completionValue));
        if (emitEffects) {
            if (!environment.context.direction) throw std::runtime_error("Item suction completion requires the scene direction.");
            environment.effects.create(0, position, *environment.context.direction);
            environment.effects.create(1, position, *environment.context.direction);
        }
        environment.events.completion(payload);
    }
    environment.events.collect(actor);
}
