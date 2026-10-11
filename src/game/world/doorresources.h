#pragma once

#include "game/world/doormodelhierarchy.h"
#include "game/models/modeldrawresource.h"
#include "game/models/modelvertexstream.h"
#include "game/models/modelmaterialstate.h"
#include "game/models/modeltexturematrix.h"
#include "game/models/modeltextureimages.h"

#include <memory>

namespace doors {
    struct ModelResource {
        bool read(const std::vector<std::uint8_t>& source, const dooranimation::PoseTables& tables, std::string& error);
        std::vector<std::uint8_t> bytes;
        doormodel::Hierarchy hierarchy;
        modeldraw::Resource drawing;
        std::map<std::int16_t, std::vector<modeldraw::Primitive>> geometry;
        std::map<std::int16_t, modelmaterial::Material> materials;
        std::map<int, modelmaterial::TextureTransform> textureTransforms;
        std::map<std::int16_t, std::vector<modelmaterial::TextureImages>> textureImages;
        std::vector<dooranimation::Matrix> initialMatrices;
    };

    class Resources {
    public:
        bool load(const std::vector<std::uint8_t>& archive, const std::vector<std::uint8_t>& executable,
            std::string& error);
        const ModelResource& model(unsigned index) const;
        const dooranimation::Resource& animation(unsigned index) const;
        const dooranimation::PoseTables& poseTables() const;
        const originalmath::FloatTrig& floatTrig() const;
        bool ready() const;

    private:
        std::array<ModelResource, 15> models;
        std::array<dooranimation::Resource, 6> animations;
        dooranimation::PoseTables tables;
        originalmath::FloatTrig trig;
        bool loaded = false;
    };

    class PoseOwner {
    public:
        void bind(std::shared_ptr<const Resources> resources);
        void selectModel(unsigned index);
        void initializeMatrices();
        void update(double frame, const dooranimation::Matrix& parent, unsigned animation);
        void updateLooping(float& frame, const dooranimation::Matrix& parent, unsigned animation);
        const dooranimation::Matrix& matrix(unsigned index) const;
        unsigned matrixCount() const;
        unsigned flags() const;
        unsigned selectedModel() const;
        const ModelResource& model(unsigned index) const;
        float previousFrame() const;
        float frameDelta() const;

    private:
        struct Binding { std::shared_ptr<const Resources> owner; unsigned index = 0; };
        std::vector<Binding> models, animations;
        unsigned selected = 0, state = 0, count = 0;
        std::array<dooranimation::Matrix, 9> inlineMatrices{};
        std::unique_ptr<dooranimation::Matrix[]> heapMatrices;
        float previous = -1.234f, delta = 0;
        void sample(float frame, const dooranimation::Matrix& parent, unsigned animation);
    };
}
