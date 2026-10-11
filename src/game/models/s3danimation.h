#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace s3danimation {
    struct Pose {
        std::array<float, 3> scale{1, 1, 1};
        std::array<std::int16_t, 3> rotation{};
        std::array<float, 3> translation{};
    };

    class Animation {
    public:
        bool read(const std::vector<std::uint8_t>& data, std::string& error);
        std::vector<Pose> sample(float frame) const;
        unsigned frames = 0;
        unsigned flags = 0;

    private:
        struct Channel {
            bool splitTangents = false;
            std::vector<float> keys;
        };
        std::vector<std::array<Channel, 9>> channels;
        static float value(const Channel& channel, float frame, unsigned component);
    };
}
