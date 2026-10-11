#include "jlib/j3d/j3dmodelinstance.h"
#include "jlib/j3d/j3ddrawbuffer.h"

#include <new>
#include <stdexcept>
#include <algorithm>

namespace {
    unsigned displayListSize(const j3dmodel::Material& material) {
        unsigned result = material.color->lighting ? 0x280 : 0x60;
        result += material.texture->full ? 0x820 : 0x2c0;
        switch (material.tev->capacity) {
        case 1: result += 0x80; break;
        case 2: result += 0x180; break;
        case 4: result += 0x260; break;
        case 16: result += 0x780; break;
        default: throw std::runtime_error("Invalid J3D material TEV block.");
        }
        result += material.indirect->full ? 100 : 0;
        result += material.pixel->kind == j3dmodel::MaterialPixelKind::Full ? 0x60 : 0x20;
        return result;
    }
}

j3dmodel::MatrixCalculator::MatrixCalculator(unsigned kind) : kind(kind), workspace(new float[18]) {}

bool j3dmodel::ModelData::read(const std::vector<std::uint8_t>& bytes, std::uint32_t flags, const MaterialDefaults& defaults,
    std::string& error) {
    ModelData result;
    if (!readGeometry(bytes, flags, result.geometry, error)) {
        return false;
    }
    if (result.geometry.materialOffset && !readMaterials(bytes, result.geometry.materialOffset, flags, defaults, result.materials, error)) {
        return false;
    }
    const unsigned kind = result.geometry.flags & 15;
    if (kind < 3) {
        result.calculator = std::make_unique<MatrixCalculator>(kind);
    }
    result.jointCalculators.resize(result.geometry.joints.size());
    const auto& links = result.geometry.hierarchy.materials;
    if (links.size() != result.materials.entries.size()) {
        error = "J3D material ownership does not match its hierarchy.";
        return false;
    }
    for (unsigned i = 0; i < links.size(); ++i) {
        auto& material = *result.materials.entries[i];
        material.shape = links[i].shape;
        material.next = links[i].next < 0 ? nullptr : result.materials.entries.at(links[i].next).get();
    }
    *this = std::move(result);
    return true;
}

j3dmodel::PacketBuffer::PacketBuffer(std::size_t size) :
    bytes(size ? static_cast<std::uint8_t*>(::operator new(size, std::align_val_t{32})) : nullptr), length(size) {}

void j3dmodel::PacketBuffer::Release::operator()(std::uint8_t* data) const {
    ::operator delete(data, std::align_val_t{32});
}

std::uint8_t* j3dmodel::PacketBuffer::data() const {
    return bytes.get();
}

std::size_t j3dmodel::PacketBuffer::size() const {
    return length;
}

void j3dmodel::MaterialPacket::addShape(ShapePacket& shape) {
    if (shapes) {
        shape.next = shapes;
    }
    shapes = &shape;
}

j3dmodel::ModelInstance::ModelInstance(std::shared_ptr<ModelData> model, std::uint32_t flags) : model(std::move(model)) {
    if (!this->model) {
        throw std::runtime_error("J3D model construction requires owned model data.");
    }
    auto& data = *this->model;
    const auto joints = data.geometry.joints.size(), envelopes = data.geometry.envelopes.size();
    if (joints) {
        jointScaleFlags.reset(new std::uint8_t[joints]);
        if (envelopes) {
            envelopeScaleFlags.reset(new std::uint8_t[envelopes]);
        }
        jointMatrices.reset(new Matrix[joints]);
    }
    if (envelopes) {
        envelopeMatrices.reset(new Matrix[envelopes]);
    }
    const auto draws = data.geometry.drawMatrices.size();
    if (draws) {
        drawMatrices = PacketBuffer(draws * sizeof(Matrix));
        normalMatrices = PacketBuffer(draws * sizeof(NormalMatrix));
    }
    shapes.resize(data.geometry.shapes.size());
    for (unsigned i = 0; i < shapes.size(); ++i) {
        shapes[i].shape = data.geometry.shapes[i].get();
        shapes[i].drawMatrices = reinterpret_cast<Matrix*>(drawMatrices.data());
        shapes[i].normalMatrices = reinterpret_cast<NormalMatrix*>(normalMatrices.data());
    }
    materials.resize(data.materials.entries.size());
    for (unsigned i = 0; i < materials.size(); ++i) {
        auto& packet = materials[i];
        auto& material = *data.materials.entries[i];
        if (material.shape < 0 || static_cast<unsigned>(material.shape) >= shapes.size() || !shapes[material.shape].shape) {
            throw std::runtime_error("J3D material has no constructed shape packet.");
        }
        packet.material = &material;
        packet.addShape(shapes[material.shape]);
        packet.textures = data.geometry.textureOffset ? &data.geometry.textureHeaders : nullptr;
        if (flags & 0x10000) {
            for (auto& entry : data.materials.entries) {
                entry->flags |= 1;
            }
        } else {
            packet.displayListCapacity = (displayListSize(material) + 31) & ~31U;
            for (auto& list : packet.displayLists) {
                list = PacketBuffer(packet.displayListCapacity);
            }
        }
    }
    positions.fill(data.geometry.vertexOffsets[0]);
    normals.fill(data.geometry.vertexOffsets[1]);
}


j3dmodel::DrawBuffer::DrawBuffer(unsigned capacity) : packets(capacity), interval((farPlane - nearPlane) / static_cast<float>(capacity)) {}

void j3dmodel::DrawBuffer::clear() { std::fill(packets.begin(), packets.end(), nullptr); }
