#include "game/player/playermanager.h"

players::Manager::Manager(const gamearchive::Archive& common, const j3dmodel::MaterialDefaults& defaults, unsigned mapMode,
    const gamearchive::Archive* mapArchive, Globals& globals, playerinput::MotorState& motors) : globals(globals) {
    models = std::make_unique<playermodels::SharedModels>(common, defaults, mapMode, mapArchive);
    state = std::make_unique<playerresources::EffectState1C>();
    points = std::make_unique<playerresources::PointPool>(0x300);
    effects80 = std::make_unique<playerresources::EffectPool80>(8);
    effects20 = std::make_unique<playerresources::EffectPool20>(4);
    curves = std::make_unique<playerresources::CurveOwner>();
    records = std::make_unique<playerresources::RecordPool44>(32);
    for (auto& controller : controllers) controller = std::make_unique<playerinput::ControllerState>(motors, nullptr);
    globals.field8BF8 = 0;
    globals.field8C28 = 0;
}

players::Manager::~Manager() {
    auxiliary.reset();
    field3C.reset();
    auxiliaryState.reset();
    entries.reset();
    for (auto& controller : controllers) controller.reset();
    records.reset();
    curves.reset();
    effects20.reset();
    effects80.reset();
    points.reset();
    state.reset();
    models.reset();
}

void players::Manager::initializeEntries() {
    entries = std::make_unique<playermodels::VacuumEntries>(*models, globals.effects);
    auxiliaryState = std::make_unique<playerresources::AuxiliaryState>();
}

void players::Manager::initializeAuxiliary(const std::vector<std::uint8_t>& executable) {
    if (!auxiliary) auxiliary = std::make_unique<playerresources::AuxiliaryOwner>(executable, *models, globals.effects, globals.pipe);
}
