#pragma once

#include "jlib/j3d/j3dmodelgeometry.h"
#include "jlib/j3d/j3dmaterialfactory.h"

namespace j3dmodel {
    using Matrix = std::array<float, 12>;
    using NormalMatrix = std::array<float, 9>;

    struct MatrixCalculator {
        explicit MatrixCalculator(unsigned kind);
        unsigned kind;
        std::unique_ptr<float[]> workspace;
        std::shared_ptr<Animation> animation;
        void* field5C = nullptr;
        float weight = 1, previousWeight = 1;
    };

    struct ModelData {
        Geometry geometry;
        Materials materials;
        std::unique_ptr<MatrixCalculator> calculator;
        std::vector<std::unique_ptr<MatrixCalculator>> jointCalculators;
        bool read(const std::vector<std::uint8_t>& bytes, std::uint32_t flags, const MaterialDefaults& defaults,
            std::string& error);
    };

    class PacketBuffer {
    public:
        PacketBuffer() = default;
        explicit PacketBuffer(std::size_t size);
        std::uint8_t* data() const;
        std::size_t size() const;

    private:
        struct Release {
            void operator()(std::uint8_t* data) const;
        };
        std::unique_ptr<std::uint8_t, Release> bytes;
        std::size_t length = 0;
    };

    struct ShapePacket {
        ShapePacket* next = nullptr;
        void* child = nullptr;
        Shape* shape = nullptr;
        Matrix* drawMatrices = nullptr;
        NormalMatrix* normalMatrices = nullptr;
        void* field18 = nullptr;
        std::uint32_t field1C = 0;
    };

    struct MaterialPacket {
        MaterialPacket* next = nullptr;
        void* child = nullptr;
        std::uint32_t flags = 0;
        std::array<PacketBuffer, 2> displayLists;
        std::uint32_t displayListSize = 0, displayListCapacity = 0;
        void* field20 = nullptr;
        void* field24 = nullptr;
        void* field28 = nullptr;
        ShapePacket* shapes = nullptr;
        Material* material = nullptr;
        std::uint32_t field34 = 0xffffffff;
        const std::vector<unsigned>* textures = nullptr;
        void addShape(ShapePacket& shape);
    };

    class ModelInstance {
    public:
        ModelInstance(std::shared_ptr<ModelData> model, std::uint32_t flags);
        ModelInstance(const ModelInstance&) = delete;
        ModelInstance& operator=(const ModelInstance&) = delete;
        std::shared_ptr<ModelData> model;
        std::uint32_t field0 = 0, field8 = 0;
        void* deform = nullptr;
        void* skin = nullptr;
        std::array<float, 3> scale{1, 1, 1};
        Matrix matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        std::unique_ptr<std::uint8_t[]> jointScaleFlags, envelopeScaleFlags;
        std::unique_ptr<Matrix[]> jointMatrices, envelopeMatrices;
        PacketBuffer drawMatrices, normalMatrices;
        std::vector<ShapePacket> shapes;
        std::vector<MaterialPacket> materials;
        std::array<unsigned, 3> positions{}, normals{};
        std::array<void*, 4> vertexWorkspace{};
    };
}
