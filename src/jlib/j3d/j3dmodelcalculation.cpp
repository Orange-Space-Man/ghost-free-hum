#include "jlib/j3d/j3dmodelcalculation.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <cstring>
#include <stdexcept>

namespace {
    j3dmodel::Matrix concatenate(const j3dmodel::Matrix& parent, const j3dmodel::Matrix& local) {
        j3dmodel::Matrix result;
        for (unsigned row = 0; row < 3; ++row) {
            for (unsigned column = 0; column < 4; ++column) {
                result[row * 4 + column] = (parent[row * 4] * local[column] + parent[row * 4 + 1] * local[4 + column]) +
                    parent[row * 4 + 2] * local[8 + column];
                if (column == 3) result[row * 4 + column] += parent[row * 4 + 3];
            }
        }
        return result;
    }
}

j3dmodel::TransformTables::TransformTables(const std::vector<std::uint8_t>& executable) {
    const auto load = [&](unsigned address, auto& values) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, values.size() * 4, bytes, error)) throw std::runtime_error(error);
        for (unsigned i = 0; i < values.size(); ++i) {
            std::uint32_t bits = 0;
            for (unsigned j = 0; j < 4; ++j) bits = (bits << 8) | bytes[i * 4 + j];
            std::memcpy(&values[i], &bits, sizeof(bits));
        }
    };
    load(glme01::sineTable, sine);
    load(glme01::cosineTable, cosine);
}

j3dmodel::Matrix j3dmodel::TransformTables::transform(const Joint& joint) const {
    const auto x = static_cast<std::uint16_t>(joint.rotation[0]) >> 4;
    const auto y = static_cast<std::uint16_t>(joint.rotation[1]) >> 4;
    const auto z = static_cast<std::uint16_t>(joint.rotation[2]) >> 4;
    const float sx = sine[x], sy = sine[y], sz = sine[z];
    const float cx = cosine[x], cy = cosine[y], cz = cosine[z];
    return {cz * cy, -sz * cx + (sx * cz) * sy, sz * sx + (cx * cz) * sy, joint.translation[0],
        sz * cy, cz * cx + (sx * sz) * sy, -cz * sx + (cx * sz) * sy, joint.translation[1],
        -sy, cy * sx, cy * cx, joint.translation[2]};
}

void j3dmodel::calculateBasic(ModelInstance& instance, const TransformTables& tables) {
    const auto& data = *instance.model;
    const auto& geometry = data.geometry;
    if (!data.calculator || data.calculator->kind != 0 || data.calculator->animation || instance.deform || instance.skin)
        throw std::runtime_error("Basic J3D calculation requires its original unanimated calculator without deformation.");
    for (const auto& calculator : data.jointCalculators)
        if (calculator) throw std::runtime_error("Joint calculator overrides require their original runtime path.");
    if (!geometry.envelopes.empty()) throw std::runtime_error("Basic J3D calculation requires weighted-envelope calculation for this model.");
    if (geometry.hierarchy.joints.size() != geometry.joints.size()) throw std::runtime_error("Invalid J3D joint hierarchy storage.");
    struct Visit {
        int index;
        Matrix parent;
        std::array<float, 3> scale;
    };
    auto base = instance.matrix;
    for (unsigned row = 0; row < 3; ++row)
        for (unsigned column = 0; column < 3; ++column) base[row * 4 + column] *= instance.scale[column];
    std::vector<Visit> pending;
    if (geometry.hierarchy.root >= 0) pending.push_back({geometry.hierarchy.root, base, instance.scale});
    std::vector<bool> seen(geometry.joints.size());
    while (!pending.empty()) {
        const auto visit = pending.back();
        pending.pop_back();
        if (visit.index < 0 || static_cast<unsigned>(visit.index) >= seen.size() || seen[visit.index])
            throw std::runtime_error("Invalid J3D joint traversal.");
        seen[visit.index] = true;
        const auto& joint = geometry.joints[visit.index];
        const auto& links = geometry.hierarchy.joints[visit.index];
        if (joint.index >= geometry.joints.size()) throw std::runtime_error("Invalid J3D joint matrix index.");
        std::array<float, 3> scale;
        for (unsigned axis = 0; axis < 3; ++axis) scale[axis] = visit.scale[axis] * joint.scale[axis];
        const bool unitScale = scale == std::array<float, 3>{1, 1, 1};
        instance.jointScaleFlags[joint.index] = unitScale ? 1 : 0;
        auto local = tables.transform(joint);
        if (!unitScale)
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 3; ++column) local[row * 4 + column] *= joint.scale[column];
        const auto matrix = concatenate(visit.parent, local);
        instance.jointMatrices[joint.index] = matrix;
        if (links.sibling >= 0) pending.push_back({links.sibling, visit.parent, visit.scale});
        if (links.child >= 0) pending.push_back({links.child, matrix, scale});
    }
}

bool j3dmodel::inverseTranspose(const Matrix& m, NormalMatrix& output) {
    const float positiveA = (m[9] * m[2]) * m[4];
    const float positiveB = (m[10] * m[0]) * m[5];
    const float positiveC = (m[8] * m[1]) * m[6];
    const float determinant = ((((positiveA + positiveB) + positiveC) - (m[2] * m[8]) * m[5]) -
        (m[10] * m[4]) * m[1]) - (m[6] * m[0]) * m[9];
    if (determinant == 0) return false;
    const float inverse = 1 / determinant;
    output = {inverse * (m[5] * m[10] - m[9] * m[6]), inverse * -(m[4] * m[10] - m[8] * m[6]),
        inverse * (m[4] * m[9] - m[8] * m[5]), inverse * -(m[1] * m[10] - m[9] * m[2]),
        inverse * (m[0] * m[10] - m[8] * m[2]), inverse * -(m[0] * m[9] - m[8] * m[1]),
        inverse * (m[1] * m[6] - m[5] * m[2]), inverse * -(m[0] * m[6] - m[4] * m[2]),
        inverse * (m[0] * m[5] - m[4] * m[1])};
    return true;
}

void j3dmodel::calculateViewMatrices(ModelInstance& instance, const Matrix& camera) {
    const auto& geometry = instance.model->geometry;
    auto* draws = reinterpret_cast<Matrix*>(instance.drawMatrices.data());
    auto* normals = reinterpret_cast<NormalMatrix*>(instance.normalMatrices.data());
    for (unsigned i = 0; i < geometry.drawMatrices.size(); ++i) {
        const auto& source = geometry.drawMatrices[i];
        if (source.weighted > 1 || source.index >= (source.weighted ? geometry.envelopes.size() : geometry.joints.size()))
            throw std::runtime_error("Invalid J3D draw matrix source.");
        const auto& matrix = source.weighted ? instance.envelopeMatrices[source.index] : instance.jointMatrices[source.index];
        draws[i] = instance.field8 & 1 ? matrix : concatenate(camera, matrix);
    }
    for (unsigned i = 0; i < geometry.drawMatrices.size(); ++i) {
        const auto& source = geometry.drawMatrices[i];
        const auto scaleFlag = source.weighted ? instance.envelopeScaleFlags[source.index] : instance.jointScaleFlags[source.index];
        if (scaleFlag == 1) {
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 3; ++column) normals[i][row * 3 + column] = draws[i][row * 4 + column];
        } else inverseTranspose(draws[i], normals[i]);
    }
}
