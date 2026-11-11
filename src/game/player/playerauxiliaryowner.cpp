#include "game/player/playerauxiliaryowner.h"
#include "game/actors/moveactor.h"
#include "resources/dolresources.h"
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
