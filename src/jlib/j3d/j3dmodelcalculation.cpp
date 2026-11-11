#include "jlib/j3d/j3dmodelcalculation.h"
#include "jlib/j3d/j3dkeyanimation.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <cstring>
#include <stdexcept>

j3dmodel::Matrix j3dmodel::concatenateMatrix(const Matrix& parent, const Matrix& local) {
    Matrix result;
    for (unsigned row = 0; row < 3; ++row) {
        for (unsigned column = 0; column < 4; ++column) {
            result[row * 4 + column] = (parent[row * 4] * local[column] + parent[row * 4 + 1] * local[4 + column]) +
                parent[row * 4 + 2] * local[8 + column];
            if (column == 3) result[row * 4 + column] += parent[row * 4 + 3];
        }
    }
    return result;
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

void j3dmodel::calculateAnimated(ModelInstance& instance, const TransformTables& tables) {
    const auto& data = *instance.model;
    const auto& geometry = data.geometry;
    if (!data.calculator || data.calculator->kind > 1 || data.calculator->animation || instance.deform || instance.skin)
        throw std::runtime_error("Basic J3D calculation requires its original unanimated calculator without deformation.");

    if (!geometry.envelopes.empty()) throw std::runtime_error("Basic J3D calculation requires weighted-envelope calculation for this model.");
    if (geometry.hierarchy.joints.size() != geometry.joints.size()) throw std::runtime_error("Invalid J3D joint hierarchy storage.");
    struct Visit {
        int index;
        Matrix parent;
        std::array<float, 3> scale;
        const MatrixCalculator* calculator;
    };
    auto base = instance.matrix;
    const bool softimage = data.calculator->kind == 1;
    if (!softimage)
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned column = 0; column < 3; ++column) base[row * 4 + column] *= instance.scale[column];
    std::vector<Visit> pending;
    if (geometry.hierarchy.root >= 0) pending.push_back({geometry.hierarchy.root, base, instance.scale, data.calculator.get()});
    std::vector<bool> seen(geometry.joints.size());
    while (!pending.empty()) {
        const auto visit = pending.back();
        pending.pop_back();
        if (visit.index < 0 || static_cast<unsigned>(visit.index) >= seen.size() || seen[visit.index])
            throw std::runtime_error("Invalid J3D joint traversal.");
        seen[visit.index] = true;
        auto joint = geometry.joints[visit.index];
        const auto* calculator = data.jointCalculators[visit.index] ? data.jointCalculators[visit.index].get() : visit.calculator;
        if (calculator->kind != data.calculator->kind) throw std::runtime_error("J3D animated calculator changes matrix convention.");
        if (calculator->animation) {
            const auto& animation = *calculator->animation;
            const auto& group = animation.groups[0];
            if (animation.kind != AnimationKind::Transform || joint.index >= group.count)
                throw std::runtime_error("J3D transform key does not cover its joint.");
            for (unsigned axis = 0; axis < 3; ++axis) {
                const auto descriptor = group.table + joint.index * 54 + axis * 18;
                joint.scale[axis] = sampleAnimationKey(animation, group, descriptor, 0, false, 1);
                const auto rotation = static_cast<std::uint32_t>(static_cast<std::int32_t>(
                    sampleAnimationKey(animation, group, descriptor + 6, 1, true, 0)));
                joint.rotation[axis] = static_cast<std::int16_t>(animation.angleScale < 32 ? rotation << animation.angleScale : 0);
                joint.translation[axis] = sampleAnimationKey(animation, group, descriptor + 12, 2, false, 0);
            }
        }
        const auto& links = geometry.hierarchy.joints[visit.index];
        if (joint.index >= geometry.joints.size()) throw std::runtime_error("Invalid J3D joint matrix index.");
        std::array<float, 3> scale;
        for (unsigned axis = 0; axis < 3; ++axis) scale[axis] = visit.scale[axis] * joint.scale[axis];
        const bool unitScale = scale == std::array<float, 3>{1, 1, 1};
        instance.jointScaleFlags[joint.index] = unitScale ? 1 : 0;
        auto local = tables.transform(joint);
        if (softimage)
            for (unsigned row = 0; row < 3; ++row) local[row * 4 + 3] *= visit.scale[row];
        else if (!unitScale)
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 3; ++column) local[row * 4 + column] *= joint.scale[column];
        const auto matrix = concatenateMatrix(visit.parent, local);
        instance.jointMatrices[joint.index] = matrix;
        if (softimage && !unitScale)
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 3; ++column)
                    instance.jointMatrices[joint.index][row * 4 + column] *= scale[column];
        if (links.sibling >= 0) pending.push_back({links.sibling, visit.parent, visit.scale, visit.calculator});
        if (links.child >= 0) pending.push_back({links.child, matrix, scale, calculator});
    }
}

void j3dmodel::calculateBasic(ModelInstance& instance, const TransformTables& tables) {
    for (const auto& calculator : instance.model->jointCalculators)
        if (calculator) throw std::runtime_error("Basic J3D calculation requires unanimated joints.");
    calculateAnimated(instance, tables);
}

void j3dmodel::calculateEnvelopes(ModelInstance& instance) {
    const auto& geometry = instance.model->geometry;
    if (geometry.envelopes.empty()) return;
    if (!geometry.bytes || !geometry.inverseMatrixOffset)
        throw std::runtime_error("J3D envelope calculation requires inverse joint matrices.");
    const auto& bytes = *geometry.bytes;
    for (unsigned index = 0; index < geometry.envelopes.size(); ++index) {
        const auto& envelope = geometry.envelopes[index];
        if (envelope.joints.size() != envelope.weights.size()) throw std::runtime_error("Invalid J3D envelope weights.");
        auto& output = instance.envelopeMatrices[index];
        output.fill(0);
        instance.envelopeScaleFlags[index] = 1;
        for (unsigned entry = 0; entry < envelope.joints.size(); ++entry) {
            const auto joint = envelope.joints[entry];
            const auto offset = static_cast<std::size_t>(geometry.inverseMatrixOffset) + joint * sizeof(Matrix);
            if (joint >= geometry.joints.size() || offset > bytes.size() || sizeof(Matrix) > bytes.size() - offset)
                throw std::runtime_error("J3D envelope references an invalid inverse joint matrix.");
            Matrix inverse;
            for (unsigned element = 0; element < inverse.size(); ++element) {
                const auto position = offset + element * 4;
                const std::uint32_t bits = (std::uint32_t(bytes[position]) << 24) | (std::uint32_t(bytes[position + 1]) << 16) |
                    (std::uint32_t(bytes[position + 2]) << 8) | bytes[position + 3];
                std::memcpy(&inverse[element], &bits, sizeof(float));
            }
            const auto transformed = concatenateMatrix(instance.jointMatrices[joint], inverse);
            const double weight = envelope.weights[entry];
            for (unsigned element = 0; element < output.size(); ++element)
                output[element] += static_cast<float>(static_cast<double>(transformed[element]) * weight);
            instance.envelopeScaleFlags[index] &= instance.jointScaleFlags[joint];
        }
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
        draws[i] = instance.field8 & 1 ? matrix : concatenateMatrix(camera, matrix);
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
