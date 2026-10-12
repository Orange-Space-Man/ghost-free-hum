#include "game/player/playeractorcomponents.h"

playeractor::Components::Components(const std::vector<std::uint8_t>& executable, actors::MoveActor& actor, const State& state,
    players::Manager& manager, players::Globals& globals, gameplaycamera::CameraOwner& camera) {
    controls = std::make_unique<playercontrols::State>();
    empty = std::make_unique<playercomponents::EmptyState>();
    quaternion = std::make_unique<playercomponents::QuaternionState>(state.quaternion);
    door = std::make_unique<playercomponents::DoorState>();
    state28 = std::make_unique<playercomponents::State28>();
    state80 = std::make_unique<playercomponents::State80>();
    scanner = std::make_unique<playermodels::ScannerModels>(*manager.models, camera);
    grids = std::make_unique<playermodels::GridOwner>(*manager.models);
    weapon = std::make_unique<playerweapons::Owner>(executable, *manager.models, globals.pipe);
    vacuum = std::make_unique<playercomponents::Vacuum>(*manager.models, globals.vacuum);
    light = std::make_unique<playermodels::LightModel>(*manager.models, camera);
    actions = std::make_unique<playeractions::Actions>(executable, actor.actorType);
    reactions = std::make_unique<playeractions::Reactions>(executable, actor.actorType);
    attached = std::make_unique<playercomponents::AttachedState>(executable, actor.actorType, *actor.model, &actor);
    single = std::make_unique<playercomponents::AttachedSingle>(executable, actor.actorType, *actor.model, &actor);
    flags = std::make_unique<playercomponents::FlagBuffer>(nullptr);
    bits[0] = std::make_unique<playercomponents::BitBuffer>(nullptr, 0x180, 0x30, 1);
    bits[1] = std::make_unique<playercomponents::BitBuffer>(nullptr, 0x180, 0x30, 1);
    bits[2] = std::make_unique<playercomponents::BitBuffer>(nullptr, 0x40, 8, 1);
    bits[3] = std::make_unique<playercomponents::BitBuffer>(nullptr, 0x40, 8, 1);
    bits[4] = std::make_unique<playercomponents::BitBuffer>(nullptr, 0x18, 6, 2);
}

playeractor::Components::~Components() {
    for (auto i = bits.size(); i != 0; --i) bits[i - 1].reset();
    flags.reset();
    single.reset();
    attached.reset();
    reactions.reset();
    actions.reset();
    light.reset();
    vacuum.reset();
    weapon.reset();
    grids.reset();
    scanner.reset();
    state80.reset();
    state28.reset();
    door.reset();
    quaternion.reset();
    empty.reset();
    controls.reset();
}
