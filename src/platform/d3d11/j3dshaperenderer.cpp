#include "platform/d3d11/j3dshaperenderer.h"

#include <limits>
#include <stdexcept>
#include <utility>

void j3drenderer::ShapeGeometry::load(ID3D11Device* device, const j3dmodel::Geometry& geometry,
    const j3dmodel::Shape& shape, bool nbtEnabled) {
    if (!device) throw std::runtime_error("J3D shape upload requires a Direct3D device.");
    ShapeGeometry result;
    result.owner = device;
    for (const auto& group : shape.groups) {
        auto& output = result.groups.emplace_back();
        for (const auto& primitive : j3dmodel::decodeShapeGroup(geometry, shape, group, nbtEnabled)) {
            std::vector<j3dmodel::ShapeVertex> vertices;
            for (const auto& vertex : primitive.vertices) if (!vertex.skipped) vertices.push_back(vertex);
            std::vector<unsigned> indices;
            D3D11_PRIMITIVE_TOPOLOGY topology;
            const auto count = static_cast<unsigned>(vertices.size());
            switch (primitive.command) {
            case 0x80:
                topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
                for (unsigned i = 0; i + 3 < count; i += 4) indices.insert(indices.end(), {i, i + 1, i + 2, i, i + 2, i + 3});
                if (count % 4 == 3) indices.insert(indices.end(), {count - 3, count - 2, count - 1});
                break;
            case 0xa0:
                topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
                for (unsigned i = 1; i + 1 < count; ++i) indices.insert(indices.end(), {0, i, i + 1});
                break;
            default:
                switch (primitive.command) {
                case 0x90: topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
                case 0x98: topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
                case 0xa8: topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST; break;
                case 0xb0: topology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
                case 0xb8: topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST; break;
                default: throw std::runtime_error("Unknown J3D primitive topology.");
                }
                for (unsigned i = 0; i < count; ++i) indices.push_back(i);
            }
            if (indices.empty()) continue;
            if (vertices.size() > std::numeric_limits<unsigned>::max() / sizeof(j3dmodel::ShapeVertex) ||
                indices.size() > std::numeric_limits<unsigned>::max() / sizeof(unsigned))
                throw std::runtime_error("J3D vertex buffers exceed Direct3D allocation limits.");
            Primitive uploaded;
            uploaded.topology = topology;
            uploaded.count = static_cast<unsigned>(indices.size());
            D3D11_BUFFER_DESC description{};
            description.Usage = D3D11_USAGE_IMMUTABLE;
            description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            description.ByteWidth = static_cast<unsigned>(vertices.size() * sizeof(j3dmodel::ShapeVertex));
            D3D11_SUBRESOURCE_DATA data{vertices.data(), 0, 0};
            if (FAILED(device->CreateBuffer(&description, &data, uploaded.vertices.GetAddressOf())))
                throw std::runtime_error("Cannot upload J3D shape vertices.");
            description.BindFlags = D3D11_BIND_INDEX_BUFFER;
            description.ByteWidth = static_cast<unsigned>(indices.size() * sizeof(unsigned));
            data.pSysMem = indices.data();
            if (FAILED(device->CreateBuffer(&description, &data, uploaded.indices.GetAddressOf())))
                throw std::runtime_error("Cannot upload J3D shape indices.");
            output.push_back(std::move(uploaded));
        }
    }
    *this = std::move(result);
}

void j3drenderer::ShapeGeometry::drawGroup(ID3D11DeviceContext* context, unsigned group) const {
    if (!context || !owner || group >= groups.size()) throw std::runtime_error("J3D shape group is not ready for drawing.");
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());
    if (device.Get() != owner.Get()) throw std::runtime_error("J3D shape context belongs to another device.");
    for (const auto& primitive : groups[group]) {
        const unsigned stride = sizeof(j3dmodel::ShapeVertex), offset = 0;
        auto* buffer = primitive.vertices.Get();
        context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
        context->IASetIndexBuffer(primitive.indices.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->IASetPrimitiveTopology(primitive.topology);
        context->DrawIndexed(primitive.count, 0, 0);
    }
}

std::size_t j3drenderer::ShapeGeometry::groupCount() const { return groups.size(); }
