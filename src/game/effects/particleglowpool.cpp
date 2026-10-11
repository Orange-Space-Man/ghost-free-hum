#include "game/effects/particleglowpool.h"

particles::Glow* particles::GlowPool::create(const std::array<float, 3>& position, float scale, std::int32_t height,
    const std::array<std::uint8_t, 4>& color) {
    for (auto& glow : values) {
        if (glow.state) continue;
        glow.position = &position;
        glow.color = color;
        glow.intensity = 0;
        glow.scale = scale;
        glow.height = height;
        glow.state = 2;
        return &glow;
    }
    return nullptr;
}

bool particles::GlowPool::release(Glow* glow) {
    if (!glow) return false;
    glow->state = 3;
    return true;
}

void particles::GlowPool::update() {
    for (auto& glow : values) {
        if (glow.state == 2) {
            glow.intensity += 0.1f;
            if (glow.intensity >= 1.0f) glow.state = 1;
        } else if (glow.state == 3) {
            glow.intensity -= 0.1f;
            if (glow.intensity <= 0) {
                glow.position = nullptr;
                glow.intensity = 0;
                glow.state = 0;
            }
        }
    }
}

const std::array<particles::Glow, 45>& particles::GlowPool::entries() const { return values; }
