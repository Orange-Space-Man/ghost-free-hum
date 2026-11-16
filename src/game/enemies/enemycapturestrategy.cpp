#include "game/enemies/enemycapturestrategy.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    int angleDelta(std::uint16_t first, std::uint16_t second) {
        int magnitude = std::abs(int(second) - int(first));
        if (magnitude > 0x8000) magnitude = 0x10000 - magnitude;
        if (first < 0x8000) return first < second && second <= unsigned(first) + 0x8000 ? magnitude : -magnitude;
        return int(first) - 0x8000 < second && second <= first ? -magnitude : magnitude;
    }
    float number(unsigned bits) {
        float result; std::memcpy(&result, &bits, 4); return result;
    }
    ghostcapture::Vector normalized(ghostcapture::Vector vector) {
        const float length = std::sqrt((vector[0] * vector[0] + vector[1] * vector[1]) + vector[2] * vector[2]);
        if (length >= 0.0001f) {
            const float inverse = static_cast<float>(1.0 / double(length));
            for (auto& component : vector) component *= inverse;
        } else vector.fill(0);
        return vector;
    }
}

enemystrategy::Capture::Capture(CaptureServices services, const gamearchive::Archive& common) :
    steering(services.random), services(std::move(services)) {
    if (!this->services.random || !this->services.frame || !this->services.nearestPlayerDelta ||
        !this->services.captureSource || !this->services.releaseReference) {
        throw std::invalid_argument("Capture requires its original clocks, player and ownership services.");
    }
    parameters.add("mBasisFrame", 4, 30);
    parameters.add("mRepeatTableCol", 4, 30);
    const auto entry = common.findPathResource("/param/th/EnBasicTsuri.prm");
    if (entry >= 0) {
        std::string error;
        if (!parameters.load(common.resource(entry), error)) throw std::runtime_error(error);
    }
}

void enemystrategy::Capture::update() {
    services.bookkeeping.update(state, services.actor, services.character,
        static_cast<int>(parameters.entries()[0].bits), static_cast<int>(parameters.entries()[1].bits),
        services.parameters.parameters(services.actor.type()).entries()[22].bits);
    Strategy::update();
}

void enemystrategy::Capture::animation(const char* name, unsigned duration) {
    if (services.actor.selectAnimation(name)) {
        services.character.field8cc = duration;
        services.character.field8d0 = 0;
    }
}

bool enemystrategy::Capture::unlinked() {
    auto& character = services.character;
    if (character.captureRecord) return false;
    auto& actor = services.actor;
    actor.fieldE4 = (actor.fieldE4 & 0xffffff00) | 255;
    actor.fieldE8 = 4;
    actor.previousRotation[0] = 0;
    if (character.actorUsers[0]) {
        services.releaseReference(character.actorUsers[0]);
        character.flags904[2] = false;
    }
    setNextState(character.captureRecord ? 0x101 : 0x102);
    return true;
}

ghostcapture::SteeringInput enemystrategy::Capture::steeringInput() const {
    return {services.actor.position, services.collision.center, services.collision.boundaryNormal,
        services.collision.contact != 0, services.collision.changed};
}

void enemystrategy::Capture::initializeEscape() {
    const auto delta = services.nearestPlayerDelta();
    const auto away = services.pose.angle(-double(delta[2]), -double(delta[0]));
    const auto nearest = services.map.nearestPolygon(services.actor.position, 150);
    int turn;
    if (nearest.polygon == 0xffff) {
        turn = services.random() & 1 ? 0x4000 : -0x4000;
        escapeHeight -= 180;
        escapeHeight = 170 - escapeHeight;
        escapeHeight += 180;
    } else {
        ghostcapture::Vector normal;
        if (!services.map.polygonNormal(nearest.polygon, normal)) throw std::runtime_error("Capture escape polygon is absent.");
        if (normal[1] >= -0.7f) {
            const auto direction = services.pose.angle(normal[2], normal[0]);
            turn = angleDelta(away, direction) > 0 ? 0x4000 : -0x4000;
        } else turn = services.random() & 1 ? 0x4000 : -0x4000;
    }
    services.actor.previousRotation[1] = static_cast<std::int16_t>(away + turn);
}

void enemystrategy::Capture::initializeBehavior() {
    auto& actor = services.actor;
    auto& character = services.character;
    switch (state) {
    case 0: services.bookkeeping.reset(); break;
    case 1:
        escapeHeight = 180;
        actor.flags &= 0xffff3bff;
        actor.fieldE0 = (actor.fieldE0 & 0xffffff00) | 255;
        actor.fieldE4 = (actor.fieldE4 & 0xffffff00) | 255;
        character.field8cc = 0;
        character.field8d0 = 0;
        actor.selectAnimation(0);
        animation("BIRIBIRI", 1);
        character.fields93c[2] = (30.0f * static_cast<float>(static_cast<std::int32_t>(
            services.parameters.parameters(actor.type()).entries()[2].bits))) / 15.0f;
        character.fields93c[0] = 5;
        actor.setVelocity({});
        baseYaw = static_cast<std::uint16_t>(actor.rotation[1]);
        character.field94c = static_cast<std::uint16_t>(baseYaw + (services.random() & 1 ? 0x1000 : -0x1000));
        actor.previousRotation[1] = static_cast<std::int16_t>(character.field94c);
        break;
    case 2:
        animation("MOGAKU_A", 0);
        actor.previousRotation[0] = actor.previousRotation[2] = 0;
        services.collision.reset();
        motion.damping = 0.95f; motion.force = {}; motion.forceMagnitude = 4; motion.maximumSpeed = 40;
        motion.recoilEnabled = false; motion.recoilFrames = 0;
        motion.overrideHeight = motion.preserveVertical = false; motion.height = 0;
        blocked = false;
        motion.track(actor.position, services.captureSource());
        steering.begin(motion, steeringInput());
        animation("MOGAKU_A", 0);
        actor.previousRotation[0] = 0;
        escapeHeight = 180;
        initializeEscape();
        break;
    }
}

void enemystrategy::Capture::faceForce() {
    const auto forward = normalized({motion.force[0], 0, motion.force[2]});
    if (forward == ghostcapture::Vector{}) return;
    const auto right = normalized({forward[2], 0, -forward[0]});
    const auto up = normalized({0, forward[2] * right[0] - forward[0] * right[2], 0});
    const actormodel::Matrix matrix{right[0], up[0], forward[0], 0, right[1], up[1], forward[1], 0,
        right[2], up[2], forward[2], 0};
    auto& actor = services.actor;
    actor.rotation[0] = actor.previousRotation[0] = 0;
    actor.previousRotation[1] = services.pose.angles(matrix)[1];
    actor.rotation[2] = actor.previousRotation[2] = 0;
}

void enemystrategy::Capture::behavior() {
    auto& actor = services.actor;
    auto& character = services.character;
    switch (state) {
    case 0:
        if (!unlinked()) setNextState(1);
        break;
    case 1: {
        if (unlinked()) break;
        if (character.field8d0 & 1) { actor.setVelocity({}); setNextState(2); }
        auto velocity = actor.velocity;
        velocity[1] = ghostmotion::vertical(services.pose, 16, 16, actor.position[1],
            services.map.floorHeight(actor.position), services.map.ceilingHeight(actor.position), -1,
            actor.id(), services.frame(), 0x2000);
        actor.setVelocity(velocity);
        if (character.field94c == static_cast<std::uint16_t>(actor.rotation[1])) {
            const auto delta = -angleDelta(baseYaw, character.field94c);
            character.field94c = static_cast<std::uint16_t>(baseYaw + (delta > 0 ? 1 : delta < 0 ? -1 : 0) * 0x1000);
            actor.previousRotation[1] = static_cast<std::int16_t>(character.field94c);
        }
        break;
    }
    case 2:
        if (unlinked() || blocked) break;
        services.collision.update(actor, 70);
        motion.track(actor.position, services.captureSource());
        if (!motion.recoil(services.collision.contact != 0, services.collision.boundaryNormal)) steering.update(motion, steeringInput());
        faceForce();
        actor.setVelocity(motion.velocity(actor.position, actor.velocity, services.collision.minimum[1],
            number(services.parameters.parameters(actor.type()).entries()[13].bits),
            services.collision.polygonContact ? &services.collision.polygonNormal : nullptr));
        break;
    }
}
