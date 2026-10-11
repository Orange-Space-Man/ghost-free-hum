#pragma once

#include "jlib/j3d/j3dnametable.h"

#include <array>
#include <memory>

namespace j3dmodel {
    struct ModelData;

    enum class AnimationKind {
        Transform,
        Color,
        Texture,
        Tev
    };

    struct AnimationGroup {
        unsigned count = 0, table = 0;
        std::array<unsigned, 4> valueCounts{}, values{};
        std::unique_ptr<NameTable> names;
        std::vector<std::uint16_t> materials;
        std::vector<std::uint8_t> slots;
        std::vector<std::array<std::uint8_t, 12>> centers;
    };

    struct Animation {
        AnimationKind kind = AnimationKind::Transform;
        std::uint8_t attribute = 0, angleScale = 0;
        std::int16_t frameCount = 0;
        float frame = 0;
        std::shared_ptr<const std::vector<std::uint8_t>> bytes;
        std::array<AnimationGroup, 2> groups;
        bool read(const std::vector<std::uint8_t>& source, std::string& error);
        void resolveMaterials(const NameTable& names);
    };

    struct AnimationBinding {
        std::uint16_t index = 0;
        std::shared_ptr<Animation> animation;
    };

    struct MaterialAnimation {
        std::array<std::unique_ptr<AnimationBinding>, 2> colors;
        std::array<std::unique_ptr<AnimationBinding>, 10> matrices;
        std::array<std::unique_ptr<AnimationBinding>, 20> postMatrices;
        std::array<std::unique_ptr<AnimationBinding>, 8> textures;
        std::array<std::unique_ptr<AnimationBinding>, 4> tevColors, tevConstants;
    };

    unsigned bindAnimation(ModelData& model, const std::shared_ptr<Animation>& animation);
    void prepareMaterialAnimations(ModelData& model);
}
