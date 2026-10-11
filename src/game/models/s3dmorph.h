#pragma once

#include "game/models/s3dmodel.h"

namespace s3dmorph {
    class Animation {
    public:
        bool read(const std::vector<std::uint8_t>& shapes, const std::vector<std::uint8_t>& keys,
            std::size_t positions, std::string& error);
        void apply(float frame, std::vector<std::array<float, 3>>& positions) const;
        float advance(float frame, float step) const;

    private:
        struct Target {
            std::vector<std::array<float, 3>> positions;
            std::vector<float> keys;
            unsigned stride = 3;
            float sample(float frame) const;
        };
        struct Group {
            std::vector<unsigned> indices;
            std::vector<Target> targets;
        };
        std::vector<Group> groups;
        unsigned frames = 0, flags = 0;
    };
}
