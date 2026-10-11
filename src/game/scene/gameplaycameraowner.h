#pragma once

#include <array>
#include <cstdint>
#include <memory>

namespace gameplaycamera {
    struct CameraOwner {
        CameraOwner();
        void reset();
        std::uint16_t flags;
        std::uint32_t callback;
        std::array<std::uint8_t, 0x38> fields8;
        std::array<float, 16> matrix;
        std::array<float, 3> up, target, position;
        std::uint16_t fieldA4;
        float fieldA8, fieldOfView, aspect, nearPlane, farPlane;
        std::array<float, 4> orthographic;
        std::uint16_t fieldCC, fieldCE;
        std::array<std::int32_t, 4> viewport;
        std::uint32_t fieldE0;
        std::uint32_t fieldE4;
        void* subject;
    };

    class Owners {
    public:
        Owners();
        ~Owners();
        Owners(const Owners&) = delete;
        Owners& operator=(const Owners&) = delete;
        std::array<std::unique_ptr<CameraOwner>, 3> cameras;
        CameraOwner* active;
        std::uint16_t field8244 = 0;
        float field009C = 26;
    };
}
