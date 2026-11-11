#include "game/player/playerauxiliaryowner.h"
#include "game/actors/moveactor.h"
#include "resources/dolresources.h"
#include "game/actors/actortimingsampler.h"
#include "game/world/dooranimationpose.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

playerresources::AuxiliaryOwner::AuxiliaryOwner(const std::vector<std::uint8_t>& executable, const playermodels::SharedModels& shared,
    playermodels::VacuumEffectGlobals& globals, playermodels::PipeParameters& pipeParameters) : executable(executable) {
    vacuum = std::make_unique<playermodels::VacuumEffectOwner>(shared, globals);
    weapon = std::make_unique<playerweapons::Owner>(executable, shared, pipeParameters);
    std::vector<std::uint8_t> bytes;
    std::string error;
    constexpr unsigned beamStrengthTable = 0x804db460;
    if (!dolresources::read(executable, beamStrengthTable, 12, bytes, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < beamStrengths.size(); ++i) {
        const std::uint32_t bits = (unsigned(bytes[i * 4]) << 24) | (unsigned(bytes[i * 4 + 1]) << 16) |
            (unsigned(bytes[i * 4 + 2]) << 8) | bytes[i * 4 + 3];
        std::memcpy(&beamStrengths[i], &bits, sizeof(float));
    }
}

void playerresources::AuxiliaryOwner::attach(actors::MoveActor& nextActor, actors::MoveActor& nextHead,
    actors::MoveActor& nextBody) {
    if (field3C) {
        actor->detachChild(*head);
        actor->detachChild(*body);
    }
    field3C = 1;
    actor = &nextActor;
    head = &nextHead;
    body = &nextBody;
    field18 = actor->position;
    const auto headParameters = playerweapons::readParameters(executable, actor->type(), head->type());
    actor->attachChild(*head, headParameters[7], nullptr, 1);
    const auto bodyParameters = playerweapons::readParameters(executable, actor->type(), body->type());
    actor->attachChild(*body, bodyParameters[7], nullptr, 1);
}

void playerresources::AuxiliaryOwner::startBeam(int mode, actors::MoveActor& nextTarget) {
    if (!field3C) return;
    field3D = 1;
    target = &nextTarget;
    if (mode >= 0 && mode < static_cast<int>(beamStrengths.size())) {
        beamStrength = beamStrengths[mode];
        beamMode = mode == 2 ? 1 : 0;
    }
}

void playerresources::AuxiliaryOwner::stopBeam(
    const std::function<void(const playermodels::VacuumPoint&, unsigned, float)>& particles) {
    if (!field3C) return;
    vacuum->stop(particles);
    field3D = 0;
}

playerresources::AuxiliaryOwner::~AuxiliaryOwner() {
    weapon.reset();
    vacuum.reset();
}

void playerresources::AuxiliaryOwner::update(const AuxiliaryFrameServices& services) {
    if (!field3C) return;
    if (!actor || !head || !body || (field3D && !target))
        throw std::runtime_error("Cinematic equipment update requires its attached actors.");
    weapon->update(executable, *actor, head, body, 0, 0, 0, true, 0, services.actors.pose, services.random);
    const auto type = actor->type();
    weapon->updatePipe(services.pipe, *head, *body, 1, true, type == 2 || type == 3 || type == 4,
        false, services.collision);
    if (field3D) {
        playermodels::VacuumEffectOwner::Input input;
        input.origin = weapon->first->position;
        input.direction = weapon->first->field610;
        input.angle = static_cast<std::uint16_t>(actor->rotation[1]);
        input.room = actor->room();
        auto matrix = target->matrix;
        if (target->model) matrix = dooranimation::concatenate(matrix,
            target->model->matrix(0, services.strategies.attachmentJoint(target->type())));
        input.target = {matrix[3], matrix[7], matrix[11]};
        input.strength = beamStrength;
        input.mode = beamMode;
        vacuum->start(input, services.effect);
    }
    vacuum->updateMotion(services.vacuum, services.actors.pose, services.collision, services.feedback);
    if (vacuum->state) {
        const auto direction = services.actors.pose.horizontalDirection(
            static_cast<std::uint16_t>(vacuum->input.angle + 0x3fff));
        vacuum->model->publishJoints({direction[2], 0, direction[0]});
    }
    const auto& definition = services.actors.catalog.type(type);
    const auto timing = services.actors.resources.timing(definition.model, definition.animations.at(actor->animationState)[0]);
    field14 = actoranimation::timingCrossing(timing.get(), "hfoot_timing",
        static_cast<float>(double(actor->animationFrame) - double(actor->animationFrameStep)), actor->animationFrame);
    for (unsigned i = 0; i < 3; ++i) field24[i] = actor->position[i] - field18[i];
    const float speed = std::sqrt((field24[0] * field24[0] + field24[1] * field24[1]) + field24[2] * field24[2]);
    if (speed < 0.0001f) {
        const auto rotation = services.actors.pose.rotation(actor->rotation);
        field30 = {rotation[2], rotation[6], rotation[10]};
        const float length = std::sqrt((field30[0] * field30[0] + field30[1] * field30[1]) + field30[2] * field30[2]);
        if (length >= 0.0001f) {
            const float inverse = 1 / length;
            for (auto& component : field30) component *= inverse;
        } else field30.fill(0);
    } else {
        const float inverse = 1 / speed;
        for (unsigned i = 0; i < 3; ++i) field30[i] = field24[i] * inverse;
    }
    if (field14 > 0) {
        if (!services.sound) throw std::runtime_error("Cinematic equipment footstep requires its sound service.");
        services.sound(0x828, actor->position, speed);
    }
    field18 = actor->position;
}
