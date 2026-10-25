#pragma once

#include "jlib/j3d/j3dmaterialloads.h"
#include "jlib/j3d/j3dpixelstate.h"

namespace materialtests {
    struct Target : j3dmodel::MaterialLoadTarget {
        std::vector<std::string> calls;
        std::vector<j3dmodel::TevCommand> commands;
        std::array<j3dmodel::ChannelControl, 4> channels{};
        std::array<j3dmodel::MaterialLight, 8> lights{};
        std::array<float, 6> indirect{};
        int exponent = 0;
        std::array<std::array<unsigned, 5>, 8> coordinates{};
        std::array<float, 12> matrix{};
        unsigned matrixIndex = 0, projection = 0;
        j3dmodel::PixelState pixels;
        const std::array<std::uint8_t, 16>* scale = nullptr;
        void tev(const j3dmodel::TevCommand& command) override { calls.push_back("tev"); commands.push_back(command); }
        void indirectCount(unsigned count) override { calls.push_back("indirect:" + std::to_string(count)); }
        void indirectMatrix(unsigned index, const std::array<float, 6>& value, int power) override {
            calls.push_back("indirectMatrix:" + std::to_string(index)); indirect = value; exponent = power;
        }
        void indirectScale(unsigned index, const std::array<std::uint8_t, 2>&) override { calls.push_back("indirectScale:" + std::to_string(index)); }
        void indirectOrder(unsigned index, const std::array<std::uint8_t, 2>&) override { calls.push_back("indirectOrder:" + std::to_string(index)); }
        void pixel(const j3dmodel::MaterialPixelBlock& block) override { calls.push_back("pixel"); j3dmodel::loadPixelState(block, pixels); }
        void materialColor(unsigned index, const std::array<std::uint8_t, 4>&) override { calls.push_back("material:" + std::to_string(index)); }
        void ambientColor(unsigned index, const std::array<std::uint8_t, 4>&) override { calls.push_back("ambient:" + std::to_string(index)); }
        void channelCount(unsigned count) override { calls.push_back("channels:" + std::to_string(count)); }
        void channel(unsigned index, const j3dmodel::ChannelControl& control) override {
            calls.push_back("channel:" + std::to_string(index)); channels.at(index) = control;
        }
        void light(unsigned index, const j3dmodel::MaterialLight& value) override {
            calls.push_back("light:" + std::to_string(index)); lights.at(index) = value;
        }
        void cull(unsigned mode) override { calls.push_back("cull:" + std::to_string(mode)); }
        void textureCount(unsigned count) override { calls.push_back("textures:" + std::to_string(count)); }
        void textureCoordinate(unsigned index, const std::array<std::uint8_t, 3>& info, unsigned normalize, unsigned post) override {
            calls.push_back("coordinate:" + std::to_string(index)); coordinates.at(index) = {info[0], info[1], info[2], normalize, post};
        }
        void textureMatrix(unsigned index, const std::array<float, 12>& value, unsigned type) override {
            calls.push_back("matrix:" + std::to_string(index)); matrixIndex = index; matrix = value; projection = type;
        }
        void materialMode(std::uint32_t mode) override { calls.push_back("mode:" + std::to_string(mode)); }
        void normalScale(const std::array<std::uint8_t, 16>* value) override { calls.push_back("nbt"); scale = value; }
    };
}
