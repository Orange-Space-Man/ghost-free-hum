#include "game/enemies/enemytraininggeneratorstrategy.h"

#include <stdexcept>
#include <utility>

enemystrategy::TrainingGenerator::TrainingGenerator(gameflags::State& flags) : flags(flags) {}

void enemystrategy::TrainingGenerator::bindServices(TrainingServices value) { services = std::move(value); }

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
    if (state != 0x106 && state != 0x107) return;
    auto& parent = generator();
    if (state == 0x106 && flags.value(signal) && parent.count < 1) {
        if (!services.random || !services.showAllPortraits || !services.portrait)
            throw std::runtime_error("Training ghost release requires its original selection and portrait-effect services.");
        const auto count = parent.path().count();
        const auto maximum = parent.parameter("max_enemy_once");
        if (count >= available.size() || maximum < 1 || static_cast<unsigned>(maximum) > count)
            throw std::runtime_error("Training selection requires its initialized path capacity.");
        for (unsigned index = 0; index < count; ++index) {
            available[index] = 1;
            if (services.showAllPortraits()) services.portrait(index);
        }
        for (unsigned choice = 0; choice < static_cast<unsigned>(maximum); ++choice) {
            auto rank = services.random() % (count - choice);
            for (unsigned index = 0; index < count; ++index) {
                if (!available[index]) continue;
                if (rank-- == 0) {
                    if (!services.showAllPortraits()) services.portrait(index);
                    available[index] = 0;
                    break;
                }
            }
        }
        setNextState(0x107);
    }
    if (state == 0x107 && frames == static_cast<std::uint32_t>(parent.parameter("appear_time")) &&
        flags.value(signal) && parent.count < 1) {
        if (!services.createChild)
            throw std::runtime_error("Training ghost release requires its original selected-child creation services.");
        for (unsigned index = 0; index < parent.path().count(); ++index)
            if (!available.at(index)) services.createChild(index);
    }
}

void enemystrategy::TrainingGenerator::receive(charactergenerator::Event& event) {
    if (event.message == 0 && generator().count < 1) flags.setOff(signal);
}
