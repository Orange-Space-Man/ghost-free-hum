#include "scatterattraction.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
    using Vector = billscatter::Vector;
    float length(const Vector& v) { return std::sqrt((v[0] * v[0] + v[1] * v[1]) + v[2] * v[2]); }
    void normalize(Vector& v) { const float inverse = 1.0f / length(v); for (auto& c : v) c *= inverse; }
}

int scattraction::apply(billscatter::Particle& particle, unsigned mode, const particles::SceneContext& context,
    float range, std::uint32_t& gameplayRandom) {
    if (mode != 1) return 0;
    if (!context.suction.position || !context.suction.direction)
        throw std::runtime_error("Original scatter attraction requires refreshed suction position and direction.");
    Vector delta;
    for (unsigned i = 0; i < 3; ++i) delta[i] = particle.position[i] - (*context.suction.position)[i];
    const float distance = length(delta);
    if (distance <= 20) return 2;
    auto outward = delta;
    normalize(outward);
    const auto& axis = *context.suction.direction;
    const float dot = (outward[0] * axis[0] + outward[1] * axis[1]) + outward[2] * axis[2];
    if (!(distance <= range && dot > -0.5f)) return 0;
    Vector inward;
    for (unsigned i = 0; i < 3; ++i) inward[i] = -outward[i];
    const float projection = (delta[0] * axis[0] + delta[1] * axis[1]) + delta[2] * axis[2];
    Vector transverse;
    for (unsigned i = 0; i < 3; ++i) transverse[i] = axis[i] * projection - delta[i];
    gameplayRandom = gameplayRandom * 0x19660du + 0x3c6ef35fu;
    const float threshold = 15.0f * (1.0f + static_cast<float>(gameplayRandom) / 4294967296.0f);
    Vector force = inward;
    if (length(transverse) > threshold) { normalize(transverse); force = transverse; }
    const float strength = static_cast<float>(double(0.8f) * context.suction.strength);
    const float acceleration = static_cast<float>(double(strength / distance) * 3000.0);
    const float speed = std::max(static_cast<float>(double(strength / (distance * distance)) * 150000.0), 3.5f);
    const float damping = 0.72f + (1.0f - 0.72f) * std::min(distance / 250.0f, 1.0f);
    for (unsigned i = 0; i < 3; ++i) {
        particle.acceleration[i] = (particle.acceleration[i] + force[i] * acceleration) * damping;
        particle.velocity[i] = inward[i] * speed;
    }
    return 1;
}
