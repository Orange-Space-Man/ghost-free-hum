#pragma once

#include "jlib/j3d/j3dtevcommands.h"

namespace j3dmodel {
    struct ChannelControl {
        unsigned enabled, ambientSource, materialSource, lights, diffuse, attenuation;
    };

    class MaterialLoadTarget {
    public:
        virtual ~MaterialLoadTarget() = default;
        virtual void tev(const TevCommand& command) = 0;
        virtual void indirectCount(unsigned count) = 0;
        virtual void indirectMatrix(unsigned index, const std::array<float, 6>& matrix, int exponent) = 0;
        virtual void indirectScale(unsigned index, const std::array<std::uint8_t, 2>& scale) = 0;
        virtual void indirectOrder(unsigned index, const std::array<std::uint8_t, 2>& order) = 0;
        virtual void pixel(const MaterialPixelBlock& block) = 0;
        virtual void materialColor(unsigned index, const std::array<std::uint8_t, 4>& color) = 0;
        virtual void ambientColor(unsigned index, const std::array<std::uint8_t, 4>& color) = 0;
        virtual void channelCount(unsigned count) = 0;
        virtual void channel(unsigned index, const ChannelControl& control) = 0;
        virtual void light(unsigned index, const MaterialLight& light) = 0;
        virtual void cull(unsigned mode) = 0;
        virtual void textureCount(unsigned count) = 0;
        virtual void textureCoordinate(unsigned index, const std::array<std::uint8_t, 3>& info,
            unsigned normalize, unsigned postMatrix) = 0;
        virtual void textureMatrix(unsigned index, const std::array<float, 12>& matrix, unsigned projection) = 0;
        virtual void materialMode(std::uint32_t mode) = 0;
        virtual void normalScale(const std::array<std::uint8_t, 16>* scale) = 0;
    };

    void loadColorBlock(const MaterialColorBlock& block, MaterialLoadTarget& target);
    void loadTextureBlock(const MaterialTextureBlock& block, MaterialLoadTarget& target);
    void loadIndirectBlock(const MaterialIndirectBlock& block, MaterialLoadTarget& target);
    void loadMaterialBlocks(const Material& material, MaterialLoadTarget& target);
    void loadMaterialDirect(const Material& material, std::uint32_t systemFlags, MaterialLoadTarget& target);
}
