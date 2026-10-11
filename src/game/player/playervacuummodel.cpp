#include "game/player/playervacuummodel.h"

#include <stdexcept>

playermodels::PathParameters::PathParameters() {
    reset();
}

void playermodels::PathParameters::reset() {
    fields.fill(nullptr);
    field14 = 1;
    field18 = 0;
    field1C = field1D = 0;
    field20 = 500;
    field24.fill(0.5f);
    field30 = field34 = 0;
    field38 = 1;
}

playermodels::VacuumParameters::VacuumParameters() {
    reset();
}

void playermodels::VacuumParameters::reset() {
    PathParameters::reset();
    field3C = field44 = 0;
    field40 = 0;
    field48 = field60 = 0;
    field54 = field58 = 1;
    field5C = 0;
    field64 = 255;
    field4C = 0;
    field50 = 1;
}

playermodels::VacuumPath::VacuumPath(unsigned count) : count(count) {
    if (!count) throw std::runtime_error("Original vacuum model requires a nonzero point count.");
    points.reset(new VacuumPoint[count]);
    vectors.reset(new std::array<float, 3>[count]);
    interval = 1.0f / static_cast<float>(count - 1);
}

playermodels::VacuumPath::~VacuumPath() {
    vectors.reset();
    points.reset();
}

void playermodels::VacuumPath::initialize(float blend, const PathParameters& parameters, const PathParameters* target) {
    if (target && blend > 0) {
        auto blended = parameters;
        const auto mix = [blend](float from, float to) {
            return from + static_cast<float>(static_cast<double>(blend) * static_cast<double>(to - from));
        };
        blended.field20 = mix(parameters.field20, target->field20);
        for (unsigned i = 0; i < 3; ++i) blended.field24[i] = mix(parameters.field24[i], target->field24[i]);
        blended.field30 = mix(parameters.field30, target->field30);
        blended.field34 = mix(parameters.field34, target->field34);
        initialize(0, blended, nullptr);
        return;
    }
    const auto* start = parameters.fields[0];
    const auto* end = parameters.fields[1];
    if (end) {
        if (!start) throw std::runtime_error("Original vacuum endpoint initialization requires a start position.");
        for (unsigned i = 0; i < count; ++i) {
            const float amount = static_cast<float>(i) * interval;
            for (unsigned axis = 0; axis < 3; ++axis) {
                const float difference = (*end)[axis] - (*start)[axis];
                const float scaled = difference * amount;
                points[i].position[axis] = (*start)[axis] + scaled;
            }
            points[i].previous.fill(0);
        }
    } else {
        const auto* direction = parameters.fields[2];
        if (!direction) throw std::runtime_error("Original vacuum initialization reads an undefined direction when both direction and endpoint are absent.");
        const auto origin = start ? *start : std::array<float, 3>{};
        const float amount = parameters.field20 * interval;
        std::array<float, 3> step;
        for (unsigned axis = 0; axis < 3; ++axis) step[axis] = (*direction)[axis] * amount;
        for (unsigned i = 0; i < count; ++i) {
            for (unsigned axis = 0; axis < 3; ++axis) {
                points[i].position[axis] = i ? points[i - 1].position[axis] + step[axis] : origin[axis];
            }
            points[i].previous.fill(0);
            points[i].field18 = 0;
        }
    }
}

playermodels::VacuumModel::VacuumModel(const SharedModels& shared, unsigned count, unsigned kind) : VacuumPath(count) {
    segments.reset(new VacuumSegment[count - 1]);
    data = shared.model(kind == 1 ? SharedModel::Koopa : SharedModel::Vacuum);
    textureAnimation = shared.animation(kind == 1 ? 9 : 0);
    colorAnimation = shared.animation(kind == 1 ? 10 : 1);
    if (!data || !textureAnimation || !colorAnimation) {
        throw std::runtime_error("Original vacuum model dependencies have not been constructed.");
    }
    instance = std::make_unique<j3dmodel::ModelInstance>(data, 0x10000);
    if (!data->geometry.jointNames) throw std::runtime_error("Original vacuum model has no joint name table.");
    bool found = false;
    for (unsigned i = 0; i < data->geometry.jointNames->size(); ++i) {
        const auto* name = data->geometry.jointNames->name(static_cast<std::uint16_t>(i));
        if (name && *name == "chn15") {
            joint = static_cast<std::uint16_t>(i);
            found = true;
            break;
        }
    }
    if (!found) throw std::runtime_error("Original vacuum model is missing joint chn15.");
}

playermodels::VacuumModel::~VacuumModel() {
    segments.reset();
    instance.reset();
}
