#include "game/player/playervacuummodel.h"

#include <stdexcept>
#include <algorithm>
#include <cmath>
#include "game/player/playerorientationupdate.h"

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

void playermodels::VacuumModel::publishJoints(const std::array<float, 3>& frameDirection) {
    const auto jointCount = data->geometry.joints.size();
    const auto firstJoint = static_cast<unsigned>(joint) + 1;
    if (count < 2 || firstJoint > jointCount || count > jointCount - firstJoint)
        throw std::runtime_error("Vacuum point chain exceeds its original model joint range.");
    textureAnimation->frame = fieldBC;
    colorAnimation->frame = (50.0f * static_cast<float>(parameters.field64)) / 255.0f;
    instance->scale = {1, 1, 1};
    instance->matrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    const auto normalize = [](std::array<float, 3>& value) {
        const float length = std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
        if (length >= 0.0001f) {
            const float inverse = 1.0f / length;
            for (auto& component : value) component *= inverse;
        } else value.fill(0);
    };
    const auto cross = [](const std::array<float, 3>& first, const std::array<float, 3>& second) {
        return std::array<float, 3>{first[1] * second[2] - first[2] * second[1],
            first[2] * second[0] - first[0] * second[2], first[0] * second[1] - first[1] * second[0]};
    };
    j3dmodel::Matrix matrix{};
    for (unsigned i = 0; i < count; ++i) {
        if (i + 1 < count) {
            std::array<float, 3> forward;
            for (unsigned axis = 0; axis < 3; ++axis) forward[axis] = points[i + 1].position[axis] - points[i].position[axis];
            normalize(forward);
            auto side = cross(frameDirection, forward);
            auto up = cross(forward, side);
            normalize(forward);
            normalize(side);
            normalize(up);
            float width = 0;
            if (!i) width = parameters.field48;
            else if (parameters.field3C == 0) width = fieldB0 * static_cast<float>(i);
            else if (parameters.field3C == 1) width = fieldAC * std::sqrt(static_cast<float>(i) * interval);
            else if (parameters.field3C == 2) width = fieldAC * std::sqrt(std::sqrt(static_cast<float>(i) * interval));
            width *= 0.01f;
            for (unsigned row = 0; row < 3; ++row) {
                matrix[row * 4] = forward[row] * width;
                matrix[row * 4 + 1] = side[row] * width;
                matrix[row * 4 + 2] = up[row] * width;
            }
        }
        for (unsigned row = 0; row < 3; ++row) matrix[row * 4 + 3] = points[i].position[row];
        instance->jointMatrices[firstJoint + i] = matrix;
    }
    for (unsigned i = firstJoint + count; i < jointCount; ++i) instance->jointMatrices[i] = matrix;
    trailingMatrix = matrix;
}

namespace {
    using PathVector = std::array<float, 3>;

    PathVector pathDirection(const PathVector& from, const PathVector& to) {
        PathVector result;
        for (unsigned axis = 0; axis < 3; ++axis) result[axis] = to[axis] - from[axis];
        const float length = std::sqrt((result[0] * result[0] + result[1] * result[1]) + result[2] * result[2]);
        if (length >= 0.0001f) {
            const float inverse = 1.0f / length;
            for (auto& value : result) value *= inverse;
        } else result.fill(0);
        return result;
    }

    void constrainPoint(playermodels::VacuumPoint& point, const playermodels::VacuumPoint& neighbor,
        const playermodels::VacuumPoint* previous, const playermodels::PathParameters& parameters,
        const PathVector* endpointDirection, float length, float progress) {
        auto direction = pathDirection(neighbor.position, point.position);
        if (endpointDirection) {
            double weight;
            if (parameters.field30 == parameters.field34) weight = parameters.field30 <= progress ? 0 : 1;
            else weight = parameters.field38 * std::clamp((progress - parameters.field34) /
                (parameters.field30 - parameters.field34), 0.0f, 1.0f);
            const auto tangent = previous ? pathDirection(previous->position, neighbor.position) : *endpointDirection;
            direction = playerorientation::direction(playerorientation::interpolate(weight,
                playerorientation::heading(direction, false), playerorientation::heading(tangent, false)));
        }
        for (unsigned axis = 0; axis < 3; ++axis) {
            const float scaled = direction[axis] * length;
            const float target = neighbor.position[axis] + scaled;
            const float correction = target - point.position[axis];
            point.position[axis] += correction * parameters.field24[1];
            point.previous[axis] += correction * parameters.field24[2];
        }
    }
}

void playermodels::VacuumPath::update(float blend, const PathParameters& parameters, const PathParameters* target,
    const PathCollision& collision) {
    if (target && blend > 0) {
        auto blended = parameters;
        const auto mix = [blend](float from, float to) {
            return from + static_cast<float>(static_cast<double>(blend) * static_cast<double>(to - from));
        };
        blended.field20 = mix(parameters.field20, target->field20);
        for (unsigned axis = 0; axis < 3; ++axis) blended.field24[axis] = mix(parameters.field24[axis], target->field24[axis]);
        blended.field30 = mix(parameters.field30, target->field30);
        blended.field34 = mix(parameters.field34, target->field34);
        update(0, blended, nullptr, collision);
        return;
    }
    if (parameters.field1C && !collision) throw std::runtime_error("Vacuum path collision requires the original room query.");
    const float length = parameters.field20 * interval;
    for (unsigned iteration = 0; iteration < parameters.field14; ++iteration) {
        for (unsigned i = 0; i < count; ++i) {
            vectors[i] = points[i].position;
            for (auto& value : points[i].previous) value *= 1.0f - (1.0f - parameters.field24[0]);
        }
        for (unsigned i = 0; i < count; ++i) points[i].previous[1] += parameters.field18;
        for (unsigned i = 0; i < count; ++i) {
            auto& point = points[i];
            if (!i) {
                if (parameters.fields[0]) {
                    point.position = *parameters.fields[0];
                    point.previous.fill(0);
                }
            } else if (i == count - 1 && parameters.fields[1]) {
                point.position = *parameters.fields[1];
                if (parameters.fields[4]) {
                    if (count < 3) throw std::runtime_error("Directed vacuum endpoint requires three path points.");
                    point.previous = *parameters.fields[4];
                    constrainPoint(point, points[i - 1], &points[i - 2], parameters, parameters.fields[2], length, 0);
                } else point.previous.fill(0);
            } else {
                const float progress = static_cast<float>(i - 1) / static_cast<float>(count - 2);
                constrainPoint(point, points[i - 1], i < 2 ? nullptr : &points[i - 2], parameters,
                    parameters.fields[2], length, progress);
            }
        }
        if (parameters.field1D) {
            for (unsigned i = count; i-- > 0;) {
                auto& point = points[i];
                if (i == count - 1) {
                    if (parameters.fields[1] && !parameters.fields[4]) {
                        point.position = *parameters.fields[1];
                        point.previous.fill(0);
                    }
                } else if (!i && parameters.fields[0]) {
                    point.position = *parameters.fields[0];
                    point.previous.fill(0);
                } else {
                    const float progress = static_cast<float>(count - 2 - i) / static_cast<float>(count - 2);
                    constrainPoint(point, points[i + 1], i < count - 2 ? &points[i + 2] : nullptr, parameters,
                        parameters.fields[3], length, progress);
                }
            }
        }
        integrate(parameters, collision);
    }
}


void playermodels::VacuumPath::integrate(const PathParameters& parameters, const PathCollision& collision) {
    for (unsigned i = 0; i < count; ++i) {
        auto& point = points[i];
        PathVector next;
        for (unsigned axis = 0; axis < 3; ++axis) next[axis] = point.position[axis] + point.previous[axis];
        point.field18 = 0;
        if (parameters.field1C) {
            const auto contact = collision(next, vectors[i]);
            point.field18 = contact.surface;
            if (contact.surface) {
                const float inward = (point.previous[0] * contact.normal[0] + point.previous[1] * contact.normal[1]) +
                    point.previous[2] * contact.normal[2];
                for (unsigned axis = 0; axis < 3; ++axis) {
                    if (inward < 0) point.previous[axis] += -inward * contact.normal[axis];
                    next[axis] += contact.correction[axis];
                }
            }
        }
        point.position = next;
    }
}

void playermodels::VacuumModel::updateMotion(const VacuumParameters& value, const actormodel::InitialPose& pose,
    const PathCollision& collision, const VacuumFeedback& feedback) {
    if (value.field60 > 0 && (!feedback.roomState || !feedback.random || !feedback.impact))
        throw std::runtime_error("Vacuum collision feedback requires the original room, random and particle services.");
    parameters = value;
    field9C = parameters.field20 * interval;
    constexpr float angleRange = 65536, halfAngle = 0.5f, fullTurn = 360;
    const auto direction = [&](float degrees) {
        const float angle = ((angleRange * halfAngle) * degrees) / fullTurn;
        if (!std::isfinite(angle) || angle < -2147483648.0f || angle >= 2147483648.0f)
            throw std::runtime_error("Vacuum cone angle exceeds the recovered conversion range.");
        return pose.horizontalDirection(static_cast<std::uint16_t>(static_cast<std::int32_t>(angle)));
    };
    const auto cone = direction(parameters.field54);
    fieldA0 = cone[0];
    fieldA4 = cone[2];
    fieldA8 = fieldA0 / fieldA4;
    fieldAC = parameters.field20 * fieldA8;
    fieldB0 = field9C * fieldA8;
    const auto capture = direction(parameters.field58);
    fieldB4 = capture[2];
    fieldB8 = (field9C * capture[0]) / fieldB4;
    VacuumPath::update(0, parameters, nullptr, collision);
    if (!parameters.field4C) updateSegments();
    if (parameters.field60 > 0 && feedback.roomState(parameters.field44) > 0) {
        const int room = feedback.roomState(parameters.field44);
        constexpr float randomUnit = 2.3283064365386963e-10f;
        constexpr float randomSpeed = 2.3283064365386963e-9f;
        for (unsigned i = 0; i < count; ++i) {
            const auto& point = points[i];
            if (!point.field18) continue;
            const float probability = static_cast<float>(static_cast<double>(randomUnit) * static_cast<float>(feedback.random()));
            if (probability < parameters.field60) {
                const float speed = static_cast<float>(10.0 + static_cast<float>(
                    static_cast<double>(randomSpeed) * static_cast<float>(feedback.random())));
                feedback.impact(point, speed, fieldB0 * static_cast<float>(i), room);
            }
        }
    }
    fieldBC += parameters.field5C;
    if (static_cast<float>(textureAnimation->frameCount) <= fieldBC) fieldBC -= static_cast<float>(textureAnimation->frameCount);
}
