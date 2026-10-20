#include "leafscatterupdate.h"
#include "billscattermodels.h"

#include <cmath>
#include <stdexcept>

void leafscatter::writeColor(j3dmodel::ModelInstance& model, const std::array<std::uint16_t, 4>& color) {
    for (auto& material : model.model->materials.entries) {
        if (!(material->identityFlags & 0xc0000000)) {
            if (!material->original) throw std::runtime_error("Original leaf material has no source.");
            material->identityFlags |= 0x80000000;
            material->mode = material->original->mode;
            material->fieldE = material->original->fieldE;
            material->animation.reset();
        }
        if (material->tev && material->tev->capacity) material->tev->colors[0] = color;
    }
}

bool leafscatter::advance(billscatter::Pool& pool, billscatter::Request& request, billscatter::Particle& particle,
    const std::array<std::uint16_t, 3>& color, float flutterStrength, std::uint32_t& particleRandom,
    std::uint32_t& gameplayRandom, const actormodel::InitialPose& pose, UpdateServices& services) {
    int suction = 0;
    if (request.suctionReady && particle.visible) suction = services.suction(particle);
    const bool ordinaryMotion = suction != 1;
    const float maximumScale = (0.75f * particle.sizeRatio) / 255.0f;
    if (suction == 1) {
        billscatter::scaleSuction(particle, services.suctionPosition(), maximumScale, 0.3f);
        particle.motion = 0;
    } else if (suction == 2) return false;
    else if (suction == 0)
        particle.lifetime = static_cast<std::int16_t>(static_cast<std::uint16_t>(particle.lifetime) - 1u);
    const billscatter::MotionParameters parameters{maximumScale, 0.35f, 0.945f, 4.5f, 0.4f, 1.75f, 3};
    billscatter::move(particle, request, ordinaryMotion, flutterStrength, 3, particleRandom, gameplayRandom, pose, services, parameters);
    billscatter::advanceOrientation(particle, ordinaryMotion, pose, 2000);
    const auto player = services.playerPosition();
    const float x = particle.position[0] - player[0], z = particle.position[2] - player[2];
    if (std::sqrt(x * x + z * z) <= 60 && player[1] <= particle.position[1] && particle.position[1] <= services.playerTop())
        particle.lifetime = 0;
    const bool alive = particle.lifetime > 0;
    if (alive) {
        writeColor(*particle.model, {color[0], color[1], color[2], 255});
        billscatter::updateModel(particle);
    } else pool.recycle(request, particle);
    return alive;
}
