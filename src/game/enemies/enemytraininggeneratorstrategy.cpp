#include "game/enemies/enemytraininggeneratorstrategy.h"

#include <stdexcept>

enemystrategy::TrainingGenerator::TrainingGenerator(gameflags::State& flags) : flags(flags) {}

charactergenerator::Generator& enemystrategy::TrainingGenerator::generator() const {
    if (!context) throw std::runtime_error("Training generator requires its original owner.");
    return *static_cast<charactergenerator::Generator*>(context);
}

void enemystrategy::TrainingGenerator::initializeBehavior() {
    if (state != 0x104) return;
    signal = 0x57;
    auto& parent = generator();
    auto& path = parent.path();
    if (!path.attached() || path.count() == 0) return;
    const auto maximum = parent.parameter("max_enemy_once");
    if (maximum > 0 && path.count() >= static_cast<unsigned>(maximum) && path.count() < 32)
        setNextState(0x106);
}

void enemystrategy::TrainingGenerator::behavior() {
    if (state == 0x106 && flags.value(signal) && generator().count < 1)
        throw std::runtime_error("Training ghost release requires its original selection and portrait-effect services.");
    if (state == 0x107 && frames == static_cast<std::uint32_t>(generator().parameter("appear_time")) &&
        flags.value(signal) && generator().count < 1)
        throw std::runtime_error("Training ghost release requires its original selected-child creation services.");
}

void enemystrategy::TrainingGenerator::receive(charactergenerator::Event& event) {
    if (event.message == 0 && generator().count < 1) flags.setOff(signal);
}
