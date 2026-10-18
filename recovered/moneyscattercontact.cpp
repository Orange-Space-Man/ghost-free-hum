#include "moneyscattercontact.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
    using moneyscatter::Vector;

    constexpr float collisionRadius = 20;
    constexpr float audibleSpeed = 5;
    constexpr float rollingSpeed = 20;
    constexpr float settlingSpeed = 2.5f;
    constexpr float reboundDamping = 0.75f;
    constexpr float reflectionScale = 2;
    constexpr float baseSpin = 16;
    constexpr float speedSpin = 1.2f;
    constexpr float angleUnits = 182.04167f;
    constexpr float spinInfluence = 10;
    constexpr float collectionRadius = 60;
    constexpr float coincidentOffset = 0.00001f;
    constexpr unsigned collectEvent = 0x26;
    constexpr unsigned collectionCountOffset = 0x94;
    constexpr std::uint32_t placedCollectionSuppressed = 0x40;

    float magnitude(const Vector& value) {
        return std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
    }

    void normalize(Vector& value) {
        const float factor = 1.0f / magnitude(value);
        for (auto& component : value) component *= factor;
    }

    void reflect(Vector& velocity, const Vector& normal) {
        for (auto& component : velocity) component *= reboundDamping;
        const float product = (-velocity[0] * normal[0] - velocity[1] * normal[1]) - velocity[2] * normal[2];
        const float scale = reflectionScale * product;
        for (unsigned i = 0; i < 3; ++i) velocity[i] += normal[i] * scale;
    }

    double foldedSpin(float spin, const std::optional<double>& retained) {
        if (spin >= 0 && spin < 90) return spin;
        if (spin > 90 && spin < 180) return 180.0f - spin;
        if (spin >= 180 && spin < 270) return spin - 180.0f;
        if (spin > 270 && spin <= 360) return 360.0f - spin;
        if (!retained) throw std::runtime_error("Money settling requires the original retained folded-spin register.");
        return *retained;
    }
}

void moneyscatter::move(Particle& particle, bool ordinaryMotion) {
    if (ordinaryMotion) particle.scale = std::min(particle.scale * 1.15f, 1.0f);
    if (particle.motion >= 2) return;
    Vector force = particle.acceleration;
    force[1] -= 2.5f;
    for (unsigned i = 0; i < 3; ++i) particle.velocity[i] = particle.velocity[i] * 0.99f + force[i];
    if (particle.placedMotion && particle.lifetime > 999) {
        particle.scale = 1;
        if (particle.velocity[1] < -5) particle.velocity[1] = -5;
        particle.placedMotion = false;
    }
    for (unsigned i = 0; i < 3; ++i) particle.position[i] += particle.velocity[i];
}

moneyscatter::CollisionResult moneyscatter::collide(Particle& particle, bool ordinaryMotion,
    const actormodel::InitialPose& pose, ContactServices& services, const std::optional<double>& retainedFoldedSpin) {
    const auto contact = services.sweep(particle.position, collisionRadius * particle.scale, particle.velocity);
    CollisionResult result;
    if (contact.floor < 0 && contact.wall < 0) return result;
    for (unsigned i = 0; i < 3; ++i) particle.position[i] += contact.correction[i];
    const float speed = magnitude(particle.velocity);
    result.audible = speed > audibleSpeed;
    if (contact.floor >= 0 && ordinaryMotion) {
        if (speed <= settlingSpeed) {
            const auto folded = foldedSpin(particle.spin, retainedFoldedSpin);
            particle.motion = 2;
            particle.velocity = {};
            particle.acceleration = {};
            particle.forward[1] = 0;
            if (magnitude(particle.forward) > 0) normalize(particle.forward);
            particle.angles = directionAngles(particle.forward, pose);
            const float spin = baseSpin + speedSpin * speed;
            const float contribution = static_cast<float>(folded * spinInfluence);
            const float cosine = pose.horizontalDirection(static_cast<std::uint16_t>(particle.angles[1]))[2];
            const float angularVelocity = cosine <= 0 ? -angleUnits * spin - contribution : angleUnits * spin + contribution;
            particle.angularVelocity = static_cast<std::int16_t>(static_cast<std::int32_t>(angularVelocity));
            result.audible = false;
        } else {
            reflect(particle.velocity, contact.floorNormal);
            if (speed <= rollingSpeed) particle.motion = 1;
        }
    }
    if (contact.wall >= 0) {
        result.wall = true;
        reflect(particle.velocity, contact.wallNormal);
    }
    return result;
}

bool moneyscatter::collect(const Request& request, Particle& particle, bool wall, ContactServices& services) {
    auto& player = services.playerPosition();
    Vector delta{particle.position[0] - player[0], 0, particle.position[2] - player[2]};
    const float distance = magnitude(delta);
    if (!(distance <= collectionRadius && player[1] <= particle.position[1] && particle.position[1] <= services.playerTop()))
        return false;
    playerevents::Payload payload;
    payload.word(collectionCountOffset, 1);
    services.playerEvent(collectEvent, payload);
    services.collectionEffect(particle.position);
    particle.lifetime = 0;
    if (request.placedMoney && !(services.roomFlags(particle.currentRoom) & placedCollectionSuppressed))
        services.notifyPlacedCollection(particle.currentRoom);
    if (distance <= 0) delta[0] += coincidentOffset;
    else normalize(delta);
    if (!wall) {
        particle.position[0] = player[0] + collectionRadius * delta[0];
        particle.position[2] = player[2] + collectionRadius * delta[2];
    } else {
        player[0] = particle.position[0] - collectionRadius * delta[0];
        player[2] = particle.position[2] - collectionRadius * delta[2];
    }
    return true;
}

void moneyscatter::collectSuction(Pool& pool, Request& request, Particle& particle, ContactServices& services) {
    constexpr unsigned suctionEvent = 0x13;
    constexpr unsigned suctionTypeOffset = 0x60;
    constexpr unsigned suctionCountOffset = 0x64;
    constexpr unsigned coinType = 5;
    playerevents::Payload payload;
    payload.word(suctionTypeOffset, coinType);
    payload.word(suctionCountOffset, 1);
    pool.recycle(request, particle);
    services.playerEvent(suctionEvent, payload);
    if (request.placedMoney && !(services.roomFlags(particle.currentRoom) & placedCollectionSuppressed))
        services.notifyPlacedCollection(particle.currentRoom);
}

