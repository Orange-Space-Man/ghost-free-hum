#include "game/player/playeraudioframe.h"
#include "game/actors/actortimingsampler.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    float scalar(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) |
            (unsigned(bytes[2]) << 8) | bytes[3];
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
}

playeractor::AudioFrame::AudioFrame(const std::vector<std::uint8_t>& executable) : actions(executable),
    landingPulse(scalar(executable, glme01::playerLandingFeedbackAmplitude)),
    slipPulse(scalar(executable, glme01::playerSlipFeedbackAmplitude)) {
    for (unsigned i = 0; i < movement.size(); ++i) movement[i] = scalar(executable, glme01::playerMovementSoundParameters + i * 4);
    for (unsigned i = 0; i < scraping.size(); ++i) scraping[i] = scalar(executable, glme01::playerScrapingSoundPitch + i * 4);
}

void playeractor::AudioFrame::run(Player& player, const AudioFrameServices& services) {
    auto& state = player.state;
    const auto& action = *player.components->actions;
    const auto reacting = [&](unsigned kind) { return action.field310 == 2 && action.field314 == kind; };
    const bool grounded = (player.fieldA0 >= 0 || player.fieldB2 >= 0) && !reacting(0x20) && !reacting(0x21);
    const bool ladder = services.collision.ladderContact(static_cast<std::uint16_t>(state.field10E0), player.position);
    const auto& velocity = player.velocity;
    const float speed = std::sqrt((velocity[0] * velocity[0] + velocity[1] * velocity[1]) + velocity[2] * velocity[2]);
    const auto& type = services.catalog.type(player.actorType);
    const auto timing = services.models.timing(type.model, type.animations.at(player.animationState)[0]);
    const float previous = static_cast<float>(double(player.animationFrame) - double(player.animationFrameStep));
    const auto crossing = [&](const char* name) {
        return actoranimation::timingCrossing(timing.get(), name, previous, player.animationFrame);
    };
    auto& playback = services.playback;
    const auto controls = playback.animationServices(player.position);
    state.field1118 = grounded || reacting(0x20) || ladder ? crossing("Lfoot_timing") : 0;
    const auto movementParameters = [&](jaudio::PositionalSound& sound, unsigned offset) {
        movementSpeed = speed;
        const float ratio = std::min(speed, movement[0]) / movement[0];
        controls.pitch(sound, movement[offset] + movement[offset + 1] * ratio);
        controls.volume(sound, movement[offset + 3] + movement[offset + 4] * ratio);
    };
    if (state.field1118 > 0) {
        jaudio::PositionalSound* sound = nullptr;
        playback.play(0x828, player.position, &sound);
        if (sound) {
            playback.setPort(*sound, 8, !(state.flags792 & 8));
            footsteps[1] = footsteps[0];
            footsteps[0] = services.audioFrame;
            movementParameters(*sound, 1);
            sound->clearBinding();
        }
    }
    if (state.flags134C[1] && speed > 0 && grounded && (!state.fieldFB8 || state.fieldFB8->value == 0))
        playback.play(0x3b, player.position, &state.slidingSound);
    else if (state.slidingSound) state.slidingSound->clearBinding();
    if (state.slidingSound) {
        movementSpeed = speed;
        if (speed < movement[6]) state.slidingSound->stop(1);
        else {
            const float ratio = std::min(speed, movement[0]) / movement[0];
            controls.pitch(*state.slidingSound, movement[7] + movement[8] * ratio);
            controls.volume(*state.slidingSound, movement[9] + movement[10] * ratio);
        }
    }
    if (state.flags134C[0]) playback.play(0x43, player.position, &state.scrapingSound);
    else if (state.scrapingSound) state.scrapingSound->clearBinding();
    if (state.scrapingSound) {
        const float ratio = std::min(speed / actions.maximumSpeed(player), movement[6]);
        if (ratio == movement[3]) state.scrapingSound->stop(1);
        else {
            controls.pitch(*state.scrapingSound, scraping[0] + scraping[1] * ratio);
            controls.volume(*state.scrapingSound, ratio);
        }
    }
    if (crossing("LAND_ON_REAR") > 0) {
        playback.playGlobal(0x801);
        player.pulseFeedback(landingPulse, 0, 10);
    }
    if (actoranimation::sampleTiming(timing.get(), "HIP_SLIP", player.animationFrame) > 0) {
        playback.playGlobal(2);
        player.pulseFeedback(slipPulse, 1, 0);
    }
    if (crossing("vhead_timing") > 0) playback.play(0x805, player.position);
    if (player.view.field8C0 > 0) playback.play(9, player.position);
    if (state.field111C > 0) {
        state.field111C -= 1;
        if (state.field111C <= 0) playback.playGlobal(0x288a);
    }
    if (state.field1120 > 0) {
        playback.play(0x1028, player.position);
        state.field1120 -= 1;
    }
    const auto ladderPolygon = static_cast<std::int16_t>((unsigned(player.fieldsB8[0]) << 8) | player.fieldsB8[1]);
    services.roomContext(player.fieldB4, ladder ? ladderPolygon : player.fieldB2);
    if (!services.sequenceActive(0x8000003e)) state.flags1388[0] = 0;
    else if (!state.flags1388[0] && state.field7A0) {
        if (services.feedbackRepeats(0x14) < 4) player.playFeedback(0x14);
        else state.flags1388[0] = 1;
    }
}
