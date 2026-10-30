#pragma once

#include "game/player/playersharedmodels.h"

namespace playermodels {
    class VacuumVolume;
    struct GridTexture {
        std::shared_ptr<const std::vector<std::uint8_t>> primary, secondary;
        unsigned width = 0, height = 0, format = 0, imageOffset = 0;
        unsigned wrapS = 0, wrapT = 0, minFilter = 1, magFilter = 1;
        float minLod = 0, maxLod = 0, lodBias = 0;
        bool mipmap = false;
    };

    struct GridPoint {
        GridPoint();
        std::array<std::uint8_t, 5> flags;
        std::uint16_t field8, fieldA;
        float fieldC;
        std::array<std::uint32_t, 2> fields10;
        std::uint32_t linkCount;
        std::array<float, 3> field1C;
        float field28;
        std::array<float, 3> field2C, field38, field44;
        float field50, field54;
    };

    struct GridParameters {
        GridParameters();
        std::uint32_t width, height;
        std::array<float, 3> field8, field14, field20, field2C;
        std::array<std::uint16_t, 4> fields38;
        std::array<float, 4> fields40;
        std::uint32_t field50, field54;
        float field58;
        std::uint32_t field5C;
        std::array<float, 4> fields60;
        float field70, field74;
        std::uint32_t field78;
        std::uint8_t field7C, field7D;
        std::array<std::uint8_t, 0x72> fields7E;
        std::uint8_t fieldF0;
    };

    class GridModels {
    public:
        GridModels(const SharedModels& shared, unsigned width, unsigned height);
        ~GridModels();
        GridModels(const GridModels&) = delete;
        GridModels& operator=(const GridModels&) = delete;
        GridParameters parameters;
        GridTexture texture;
        std::unique_ptr<GridPoint[]> points;
        unsigned width, height, count;
        std::array<float, 4> fields118{};
        const VacuumVolume* suctionVolume = nullptr;
        const std::array<float, 3>* suctionPosition = nullptr;
        std::int32_t field148 = -1;
        std::array<std::uint8_t, 3> fields14C{};
        float field150 = 0, field154 = 0;
        std::uint8_t field158 = 0;
        float field15C = 0;
        std::uint8_t field160 = 0, field161 = 0;
        std::array<float, 4> fields130{0, 0, 1, 1};
        std::array<float, 3> field164{};
        std::uint8_t field170 = 0;
        std::array<std::unique_ptr<j3dmodel::ModelInstance>, 3> instances;
        std::array<j3dmodel::Material*, 3> materials{};
        std::array<std::uint8_t, 3> slots;
        std::array<float, 3> field1F8{1, 1, 1};
    };

    class GridOwner {
    public:
        explicit GridOwner(const SharedModels& shared);
        ~GridOwner();
        std::int32_t field0 = -1;
        std::uint32_t field1684 = 0;
        std::array<std::unique_ptr<GridModels>, 4> grids;
        std::array<std::uint8_t, 4> fields16BC{};
    };
}
