#pragma once

#include <array>

namespace ghostcapture {
    using Vector = std::array<float, 3>;

    struct Motion {
        Vector source{}, towardSource{}, horizontalTowardSource{};
        Vector force{}, recoilForce{};
        float damping = 0.95f, maximumSpeed = 40, forceMagnitude = 4;
        bool recoilEnabled = false;
        float recoilFrames = 0;
        bool overrideHeight = false, preserveVertical = false;
        float height = 0;

        void track(const Vector& position, const Vector* captureSource);
        bool recoil(bool contact, const Vector& normal);
        Vector velocity(const Vector& position, const Vector& previous, float floor, float typeHeight,
            const Vector* contactNormal) const;
    };
}
