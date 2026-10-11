#pragma once

#include "game/world/dooranimationsampler.h"

namespace dooranimation {
    using Matrix = std::array<float, 12>;

    class PoseTables {
    public:
        bool read(const std::vector<std::uint8_t>& executable, std::string& error);
        Matrix local(const std::array<float, 9>& channels) const;
        Matrix staticLocal(const std::array<float, 9>& channels) const;
        Matrix world(const std::array<float, 3>& position, const std::array<std::int32_t, 3>& angles,
            const std::array<float, 3>& scale) const;
        float sineAngle(std::uint32_t angle) const;

    private:
        std::array<float, 4096> sine{}, cosine{};
        double bias = 0, angleScale = 0;
        double staticBias = 0, staticAngleScale = 0;
        bool loaded = false;
        Matrix local(const std::array<float, 9>& channels, double conversionBias, double conversionScale) const;
    };

    Matrix concatenate(const Matrix& parent, const Matrix& local);
    Matrix samplePose(const Resource& resource, const PoseTables& tables, unsigned track, float frame, const Matrix& parent);
}
