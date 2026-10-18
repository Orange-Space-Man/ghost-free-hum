#include "billscattercollection.h"

#include <cmath>

void billscatter::scaleSuction(Particle& particle, const Vector& suctionPosition) {
    Vector delta;
    for (unsigned i = 0; i < 3; ++i) delta[i] = particle.position[i] - suctionPosition[i];
    const float distance = std::sqrt((delta[0] * delta[0] + delta[1] * delta[1]) + delta[2] * delta[2]);
    constexpr double maximumScale = 0.75f, minimumRatio = 0.1f, range = 150;
    if (range < distance) particle.scale = static_cast<float>(maximumScale);
    else {
        const float slope = static_cast<float>(1.0 - minimumRatio);
        const float product = static_cast<float>(double(slope) * distance);
        const float fraction = static_cast<float>(double(product) / range);
        const float ratio = static_cast<float>(minimumRatio + fraction);
        particle.scale = static_cast<float>(maximumScale * ratio);
    }
}

bool billscatter::collect(Particle& particle, CollectionServices& services) {
    const auto player = services.playerPosition();
    const float x = particle.position[0] - player[0], z = particle.position[2] - player[2];
    const float distance = std::sqrt(x * x + z * z);
    if (!(distance <= 60 && player[1] <= particle.position[1] && particle.position[1] <= services.playerTop())) return false;
    constexpr unsigned contactEvent = 0x27, billCountOffset = 0x98;
    playerevents::Payload payload;
    payload.word(billCountOffset, 1);
    services.playerEvent(contactEvent, payload);
    particle.lifetime = 0;
    return true;
}

void billscatter::collectSuction(Pool& pool, Request& request, Particle& particle, CollectionServices& services) {
    constexpr unsigned suctionEvent = 0x13, typeOffset = 0x60, countOffset = 0x64, billType = 6;
    playerevents::Payload payload;
    payload.word(typeOffset, billType);
    payload.word(countOffset, 1);
    pool.recycle(request, particle);
    services.playerEvent(suctionEvent, payload);
}
