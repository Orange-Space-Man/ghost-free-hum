#pragma once

#include "playersheetlayout.h"

namespace playersheets {
    struct MotionParameters {
        unsigned iterations = 1;
        float attachedDamping, detachedDamping, gravity, positionResponse, velocityResponse;
        bool reciprocalConstraints = false;
        explicit MotionParameters(const std::vector<std::uint8_t>& executable);
    };

    class Motion {
    public:
        Motion(Grid& grid, const std::vector<std::uint8_t>& executable);
        void retract(bool hasSuctionOwner);
        bool crop();
        void damp(const MotionParameters& parameters);
        void applyGravity(const MotionParameters& parameters);
        void constrainAnchors(const MotionParameters& parameters);
        void constrainNeighbors(const MotionParameters& parameters, const playerscanner::Vector* suctionPosition);
        void integrate();
        void updateNormals();
        const Plane* collisionPlane(unsigned point) const;
        float collide(Point& point, const Plane& plane, bool elastic) const;
        std::array<float, 4> drops;
        std::array<float, 2> pullDirection{};
        bool pulling = false, retracting = false, halfway = false, detached = false;
        std::array<float, 4> cropBounds{0, 0, 1, 1};

    private:
        Grid& grid;
        float width, height, anchorBase, anchorWeight, anchorSnap, bounce;
    };
}
