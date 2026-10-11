#pragma once

#include "game/world/doorresources.h"
#include "game/models/modeltexturestages.h"
#include "game/models/modellighting.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <optional>

namespace modelrenderer {
    struct TextureCoordinate {
        std::array<unsigned, 4> source{};
        dooranimation::Matrix matrix{};
    };

    struct TextureCoordinates {
        std::array<TextureCoordinate, 8> coordinates{};
        std::array<float, 4> lightPosition{};
    };

    TextureCoordinates prepareCoordinates(const modelmaterial::StageProgram& program,
        const std::map<int, dooranimation::Matrix>& matrices,
        const std::optional<std::array<float, 3>>& lightPosition = std::nullopt);

    struct VertexMatrices {
        dooranimation::Matrix modelView{}, normal{};
        std::array<float, 16> projection{};
    };

    class Geometry {
    public:
        void load(ID3D11Device* device, const doors::ModelResource& model);
        void draw(ID3D11DeviceContext* context, std::int16_t batch, const VertexMatrices& matrices,
            const TextureCoordinates& coordinates, const modellighting::Inputs& lighting, bool cullAllTriangles = false) const;

    private:
        struct Primitive {
            Microsoft::WRL::ComPtr<ID3D11Buffer> vertices, indices;
            D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
            unsigned count = 0;
        };
        Microsoft::WRL::ComPtr<ID3D11Device> owner;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
        Microsoft::WRL::ComPtr<ID3D11Buffer> constants;
        Microsoft::WRL::ComPtr<ID3D11Buffer> textureConstants;
        Microsoft::WRL::ComPtr<ID3D11Buffer> lightingConstants;
        std::map<std::int16_t, std::vector<Primitive>> batches;
    };
}
