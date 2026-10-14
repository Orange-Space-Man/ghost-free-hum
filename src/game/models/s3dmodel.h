#pragma once

#include "game/models/s3danimation.h"
#include "game/models/s3dtexturestage.h"
#include "game/world/roommaterials.h"

namespace s3dmodel {
    using Matrix = std::array<float, 12>;

    struct Vertex {
        std::array<float, 3> position{};
        std::array<float, 3> normal{};
        std::array<float, 2> coordinate{};
        std::array<float, 4> color{1, 1, 1, 1};
        unsigned matrix = 0;
        unsigned positionIndex = 0;
    };

    struct Material {
        std::array<float, 4> color{1, 1, 1, 1};
        unsigned texture = 0, wrapS = 0, wrapT = 0, mode = 0;
        bool vertexColor = false, vertexAlpha = false;
        bool secondaryColor = false;
        unsigned sourceMode = 0;
        std::vector<s3dtexturestage::Stage> stages;
    };

    struct Sampler {
        unsigned texture = 0, wrapS = 0, wrapT = 0, mode = 0;
    };

    unsigned renderMode(const Material& material);

    struct Batch {
        unsigned material = 0, cull = 0;
        bool lighting = false;
        std::vector<Vertex> vertices;
    };

    struct Joint {
        int child = -1, sibling = -1;
    };

    struct Weight {
        unsigned joint = 0;
        float value = 0;
    };

    class Model {
    public:
        bool read(const std::vector<std::uint8_t>& data, std::string& error, bool loadShadows = false);
        std::vector<Batch> animate(const std::vector<s3danimation::Pose>& pose, unsigned flags,
            const std::vector<float>& trigonometry, bool shadow = false) const;
        std::vector<roommaterials::Texture> textures;
        std::vector<Material> materials;
        std::vector<Sampler> samplers;
        std::vector<Batch> batches;
        std::vector<Batch> shadowBatches;
        std::vector<Joint> joints;
        std::vector<std::array<float, 3>> positions;

    private:
        std::vector<Matrix> inverseBind;
        std::vector<std::vector<Weight>> envelopes;
    };
}
