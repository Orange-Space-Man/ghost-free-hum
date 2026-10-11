#pragma once

#include <array>
#include <cstdint>

namespace particles {
    struct Glow {
        const std::array<float, 3>* position = nullptr;
        std::array<std::uint8_t, 4> color{};
        float intensity = 0, scale = 0;
        std::int32_t height = 0;
        std::uint8_t state = 0;
    };

    class GlowPool {
    public:
        Glow* create(const std::array<float, 3>& position, float scale, std::int32_t height,
            const std::array<std::uint8_t, 4>& color);
        bool release(Glow* glow);
        void update();
        const std::array<Glow, 45>& entries() const;

    private:
        std::array<Glow, 45> values{};
    };
}
