#pragma once

#include "game/models/s3denvironment.h"
#include "game/scene/gameplaycameraowner.h"
#include "game/world/roomlightpool.h"

namespace gameevent {
    class CameraPath {
    public:
        explicit CameraPath(const std::vector<std::uint8_t>& resource);
        void sample(float frame, gameplaycamera::CameraOwner& camera) const;
        unsigned duration() const;

    private:
        unsigned frames;
        std::array<s3denvironment::Curve, 8> channels;
    };

    class LightPath {
    public:
        explicit LightPath(const std::vector<std::uint8_t>& resource);
        void sample(float frame, roomlighting::Pool& pool) const;

    private:
        struct Light {
            unsigned flags, distance, brightness;
            std::array<s3denvironment::Curve, 12> channels;
        };
        std::vector<Light> lights;
    };
}
