#pragma once

#include "game/scene/gamearchive.h"
#include "jlib/j3d/j3dmodelinstance.h"

namespace playermodels {
    enum class SharedModel {
        Vacuum,
        Light,
        Pipe,
        Coin,
        Bill,
        Bill2,
        Bill3,
        Leaf,
        Pile,
        Bed,
        BedCover,
        Scanner,
        Warp,
        Koopa
    };

    struct DrawBuffer {
        explicit DrawBuffer(unsigned capacity);
        void clear();
        std::vector<j3dmodel::MaterialPacket*> entries;
        std::uint32_t sort = 0, draw = 0;
        float near = 1, far = 10000, interval = 0;
        void* field1C = nullptr;
        void* field20 = nullptr;
    };

    class SharedModels {
    public:
        SharedModels(const gamearchive::Archive& common, const j3dmodel::MaterialDefaults& defaults,
            unsigned mapMode, const gamearchive::Archive* mapArchive);
        const std::shared_ptr<j3dmodel::ModelData>& model(SharedModel kind) const;
        const std::shared_ptr<j3dmodel::Animation>& animation(unsigned index) const;
        const std::shared_ptr<const std::vector<std::uint8_t>>& texture(unsigned index) const;
        DrawBuffer& buffer(unsigned index);

    private:
        std::array<std::unique_ptr<DrawBuffer>, 12> buffers;
        std::array<std::shared_ptr<j3dmodel::ModelData>, 14> models;
        std::array<std::shared_ptr<j3dmodel::Animation>, 11> animations;
        std::array<std::shared_ptr<const std::vector<std::uint8_t>>, 16> textures;
    };
}
