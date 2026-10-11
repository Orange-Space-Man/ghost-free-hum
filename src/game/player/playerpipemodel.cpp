#include "game/player/playerpipemodel.h"

#include <stdexcept>
#include <cmath>
#include <limits>

playermodels::PipeModel::PipeModel(const SharedModels& shared, unsigned count) : VacuumPath(count) {
    values.reset(new float[count]);
    instance = std::make_unique<j3dmodel::ModelInstance>(shared.model(SharedModel::Pipe), 0x10000);
    const auto& names = instance->model->geometry.jointNames;
    if (!names) throw std::runtime_error("Original pipe model has no joint name table.");
    for (unsigned i = 0; i < names->size(); ++i) {
        const auto* name = names->name(static_cast<std::uint16_t>(i));
        if (name && *name == "jnt1") {
            joint = static_cast<std::uint16_t>(i);
            return;
        }
    }
    throw std::runtime_error("Original pipe model is missing joint jnt1.");
}

playermodels::PipeModel::~PipeModel() {
    instance.reset();
    values.reset();
}

std::int32_t playermodels::PipeModel::pulse(double speed, double amount) {
    if (speed <= 0) return 0;
    for (auto& section : sections) {
        if (section.state) continue;
        section.state = 1;
        section.field4 = {0, static_cast<float>(speed), static_cast<float>(amount)};
        const float duration = static_cast<float>(static_cast<float>(count) / speed);
        if (!std::isfinite(duration) || duration < -2147483648.0 || duration >= 2147483648.0) {
            return std::numeric_limits<std::int32_t>::min();
        }
        return static_cast<std::int32_t>(duration);
    }
    return 0;
}
