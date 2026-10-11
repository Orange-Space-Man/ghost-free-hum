#pragma once

#include "game/world/dooranimationpose.h"

namespace doormodel {
    struct NodeVisit {
        std::int16_t index = 0, parent = -1;
    };

    class Hierarchy {
    public:
        bool read(const std::vector<std::uint8_t>& model, std::string& error);
        const std::vector<NodeVisit>& visits() const;
        std::vector<dooranimation::Matrix> staticPose(const dooranimation::PoseTables& tables,
            const dooranimation::Matrix& parent) const;
        std::vector<dooranimation::Matrix> sample(const dooranimation::Resource& animation,
            const dooranimation::PoseTables& tables, float frame, const dooranimation::Matrix& parent) const;
        void sampleInto(const dooranimation::Resource& animation, const dooranimation::PoseTables& tables,
            float frame, const dooranimation::Matrix& parent, dooranimation::Matrix* matrices, unsigned count) const;

    private:
        std::vector<NodeVisit> order;
        std::vector<std::array<float, 9>> transforms;
    };
}
