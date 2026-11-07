#include "game/player/playerlightmodel.h"
#include "jlib/j3d/j3dmaterialcalculation.h"
#include "jlib/j3d/j3dmaterialdisplaylist.h"
#include "jlib/j3d/j3dtextureanimation.h"
#include "jlib/j3d/j3ddrawbuffer.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
    float number(const std::array<std::uint8_t, 64>& bytes, unsigned index) {
        std::uint32_t bits = 0;
        for (unsigned i = 0; i < 4; ++i) bits = (bits << 8) | bytes[index * 4 + i];
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }

    void store(std::array<std::uint8_t, 100>& bytes, unsigned offset, float value) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, 4);
        for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(bits >> (24 - i * 8));
    }
}

playermodels::LightModel::LightModel(const SharedModels& shared, gameplaycamera::CameraOwner& camera) : camera(&camera) {
    instance = std::make_unique<j3dmodel::ModelInstance>(shared.model(SharedModel::Light), 0x10000);
    unsigned count = 0;
    for (auto& material : instance->model->materials.entries) {
        for (auto& matrix : material->texture->matrices) {
            if (count == 3) break;
            if (!matrix || matrix->info[1] != 3) continue;
            materials[count] = material.get();
            matrices[count] = matrix.get();
            std::copy_n(matrix->info.begin() + 0x24, 64, originalMatrices[count].begin());
            ++count;
        }
        if (count == 3) break;
    }
}

void playermodels::LightModel::prepare(const j3dmodel::Matrix& view, float projectionDepth,
    const j3dmodel::TransformTables& tables, SharedModels& shared) {
    if (!state) return;
    std::array<float, 3> offset{}, translated{};
    for (unsigned row = 0; row < 3; ++row)
        offset[row] = view[row] * view[3] + view[4 + row] * view[7] + view[8 + row] * view[11];
    offset[2] = projectionDepth;
    for (unsigned row = 0; row < 3; ++row)
        translated[row] = view[row * 4] * offset[0] + view[row * 4 + 1] * offset[1] + view[row * 4 + 2] * offset[2];
    for (unsigned i = 0; i < materials.size(); ++i) {
        if (!materials[i]) continue;
        auto& material = *materials[i];
        if (!(material.identityFlags & 0xc0000000) && material.original) {
            material.identityFlags |= 0x80000000;
            material.mode = material.original->mode;
            material.fieldE = material.original->fieldE;
            material.animation.reset();
        }
        const auto& original = originalMatrices[i];
        auto& info = matrices[i]->info;
        std::copy(original.begin(), original.end(), info.begin() + 36);
        for (unsigned row = 0; row < 3; ++row) {
            const float shift = number(original, row) * translated[0] + number(original, 4 + row) * translated[1] +
                number(original, 8 + row) * translated[2];
            store(info, 36 + (row * 4 + 3) * 4, number(original, row * 4 + 3) - shift);
        }
    }
    j3dmodel::calculateBasic(*instance, tables);
    const auto& geometry = instance->model->geometry;
    std::vector<int> pending;
    if (geometry.hierarchy.root >= 0) pending.push_back(geometry.hierarchy.root);
    j3dmodel::DrawBuffer buffer(1);
    while (!pending.empty()) {
        const auto joint = pending.back();
        pending.pop_back();
        const auto& links = geometry.hierarchy.joints[joint];
        if (links.sibling >= 0) pending.push_back(links.sibling);
        if (links.child >= 0) pending.push_back(links.child);
        const auto& matrix = instance->jointMatrices[geometry.joints[joint].index];
        for (int index = geometry.hierarchy.joints[joint].material; index >= 0;
            index = geometry.hierarchy.materials[index].next) {
            auto& packet = instance->materials.at(index);
            auto& material = *packet.material;
            j3dmodel::animateTextureMatrices(material);
            j3dmodel::calculateMaterial(material, matrix, view, instance->field8 & 12, tables);
            auto& target = shared.buffer((material.mode & 3) ? 4 : 5);
            if (target.sort != 0) throw std::runtime_error("Flashlight requires its original material-sort buffer.");
            buffer.packets.swap(target.entries);
            packet.materialIdentity = material.identity;
            packet.identityFlags = material.identityFlags;
            const bool admitted = buffer.entryMaterial(packet, geometry.textureOffset);
            buffer.packets.swap(target.entries);
            if (admitted) j3dmodel::updateMaterialPacket(packet);
        }
    }
    j3dmodel::calculateViewMatrices(*instance, view);
}
