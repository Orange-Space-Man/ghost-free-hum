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

void playermodels::PipeModel::updatePulses() {
    constexpr float baseScale = 1, pulseWeight = 0.3333333432674408f, pulseWidth = 3;
    for (unsigned i = 0; i < count; ++i) values[i] = baseScale;
    for (auto& section : sections) {
        if (!section.state) continue;
        auto& position = section.field4[0];
        const float speed = section.field4[1], amount = section.field4[2];
        position += speed;
        if ((speed > 0 && position >= static_cast<float>(count)) || (speed < 0 && position < 0)) {
            section.state = 0;
            continue;
        }
        const int center = static_cast<int>(position);
        for (int offset = -2; offset <= 2; ++offset) {
            const int index = center + offset;
            if (index >= 0 && static_cast<unsigned>(index) < count)
                values[index] += pulseWeight * amount * (pulseWidth - static_cast<float>(std::abs(offset)));
        }
    }
}

namespace {
    using PipeVector = std::array<float, 3>;

    float pipeDot(const PipeVector& first, const PipeVector& second) {
        return (first[0] * second[0] + first[1] * second[1]) + first[2] * second[2];
    }

    void normalizePipeVector(PipeVector& value) {
        const float length = std::sqrt(pipeDot(value, value));
        if (length >= 0.0001f) {
            const float inverse = 1.0f / length;
            for (auto& component : value) component *= inverse;
        } else value.fill(0);
    }
}

void playermodels::PipeModel::updateMotion(const PipeParameters& parameters, const PathCollision& collision) {
    if (!parameters.fields[0]) throw std::runtime_error("Pipe update requires its original connection position.");
    if (parameters.boundary && parameters.boundary->plane >= 3)
        throw std::runtime_error("Pipe body constraint references an invalid plane.");
    PipeVector difference;
    for (unsigned axis = 0; axis < 3; ++axis) difference[axis] = (*parameters.fields[0])[axis] - points[0].position[axis];
    constexpr float resetDistance = 160;
    if (std::sqrt(pipeDot(difference, difference)) > resetDistance) initialize(0, parameters, nullptr);
    startNormal = parameters.startNormal;
    startFraction = parameters.field44;
    hasBoundary = parameters.boundary != nullptr;
    if (hasBoundary) {
        const auto& boundary = *parameters.boundary;
        boundaryPlane = boundary.plane;
        for (unsigned row = 0; row < 3; ++row) {
            const auto offset = row * 4;
            positions[0][row] = ((boundary.matrix[offset] * boundary.position[0] +
                boundary.matrix[offset + 1] * boundary.position[1]) + boundary.matrix[offset + 2] * boundary.position[2]) +
                boundary.matrix[offset + 3];
        }
        for (unsigned column = 0; column < 3; ++column) {
            auto& normal = positions[column + 1];
            normal = {boundary.matrix[column], boundary.matrix[column + 4], boundary.matrix[column + 8]};
            normalizePipeVector(normal);
            for (auto& value : normal) value *= boundary.signs[column];
        }
    }
    VacuumPath::update(0, parameters, nullptr, collision);
    updatePulses();
}

void playermodels::PipeModel::integrate(const PathParameters& parameters, const PathCollision& collision) {
    if (startNormal && startFraction > 0) {
        for (unsigned i = 1; i < count; ++i) {
            if (!(static_cast<float>(i) * interval < startFraction)) continue;
            auto& point = points[i];
            PipeVector difference;
            for (unsigned axis = 0; axis < 3; ++axis) difference[axis] = point.position[axis] - points[0].position[axis];
            const float distance = pipeDot(difference, *startNormal);
            if (distance >= 0) continue;
            const float velocity = pipeDot(point.previous, *startNormal);
            for (unsigned axis = 0; axis < 3; ++axis) {
                point.position[axis] -= distance * (*startNormal)[axis];
                if (velocity < 0) point.previous[axis] -= velocity * (*startNormal)[axis];
            }
        }
    }
    VacuumPath::integrate(parameters, collision);
    if (!hasBoundary) return;
    for (unsigned i = 0; i < count; ++i) {
        auto& point = points[i];
        PipeVector difference, distances;
        for (unsigned axis = 0; axis < 3; ++axis) difference[axis] = point.position[axis] - positions[0][axis];
        for (unsigned axis = 0; axis < 3; ++axis) distances[axis] = pipeDot(difference, positions[axis + 1]);
        if (!(distances[0] > 0 && distances[1] > 0 && distances[2] > 0)) continue;
        const auto& normal = positions[boundaryPlane + 1];
        const float velocity = pipeDot(point.previous, normal);
        for (unsigned axis = 0; axis < 3; ++axis) {
            point.position[axis] -= distances[boundaryPlane] * normal[axis];
            if (velocity > 0) point.previous[axis] -= velocity * normal[axis];
        }
    }
}

void playermodels::PipeModel::publishJoints(double scale, const std::array<float, 3>& frameDirection) {
    const auto jointCount = instance->model->geometry.joints.size();
    if (count < 2 || joint > jointCount || count > jointCount - joint)
        throw std::runtime_error("Pipe point chain exceeds its original model joint range.");
    instance->scale = {1, 1, 1};
    instance->matrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    j3dmodel::Matrix matrix{};
    const auto cross = [](const PipeVector& first, const PipeVector& second) {
        return PipeVector{first[1] * second[2] - first[2] * second[1],
            first[2] * second[0] - first[0] * second[2], first[0] * second[1] - first[1] * second[0]};
    };
    for (unsigned i = 0; i < count; ++i) {
        if (i < count - 1) {
            PipeVector forward;
            for (unsigned axis = 0; axis < 3; ++axis) forward[axis] = points[i + 1].position[axis] - points[i].position[axis];
            normalizePipeVector(forward);
            auto side = cross(frameDirection, forward);
            auto up = cross(forward, side);
            normalizePipeVector(forward);
            normalizePipeVector(side);
            normalizePipeVector(up);
            const float width = static_cast<float>(scale * static_cast<double>(values[i]));
            for (unsigned row = 0; row < 3; ++row) {
                matrix[row * 4] = forward[row] * width;
                matrix[row * 4 + 1] = side[row] * width;
                matrix[row * 4 + 2] = up[row] * width;
            }
        }
        for (unsigned row = 0; row < 3; ++row) matrix[row * 4 + 3] = points[i].position[row];
        instance->jointMatrices[joint + i] = matrix;
    }
    for (unsigned i = joint + count; i < jointCount; ++i) instance->jointMatrices[i] = matrix;
}
