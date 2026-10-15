#include "game/audio/gameplaymusicrouting.h"

#include <initializer_list>

namespace {
    constexpr unsigned roomTracks[] = {0, 1, 2, 3, 8, 9, 10, 11};

    void silenceRoomTracks(gameplaymusic::Services& services, unsigned frames) {
        for (unsigned track : roomTracks) services.setTrackVolume(track, 0, frames);
    }
}

unsigned gameplaymusic::State::selectTrack(Services& services) const {
    if (alternateSelection == 1 && services.roomSize() != 2) return 7;
    if (playerHealth < 71) return playerHealth < 41 ? 6 : 5;
    return 4;
}

float gameplaymusic::Tempo::update(unsigned track, unsigned health, std::uint32_t currentFrame,
    std::uint32_t previousEventFrame, std::uint32_t latestEventFrame) {
    std::uint32_t nextInterval = 12;
    if (track == 7) {
        const std::uint32_t elapsed = currentFrame - previousEventFrame;
        const std::uint32_t eventInterval = latestEventFrame - previousEventFrame;
        nextInterval = elapsed < 16 && eventInterval < 9 ? (eventInterval < 4 ? 8 : eventInterval * 2) : 16;
    } else {
        interval = 12;
        scale = 1;
    }
    if (nextInterval != interval) {
        scale = 12.0f / static_cast<float>(nextInterval);
        if (nextInterval < 12) scale *= 0.8333f;
        else if (nextInterval > 12) scale *= 1.2f;
        transitionFrames = 10;
        interval = nextInterval;
    }
    return scale * (0.6f + (0.4f * static_cast<float>(health)) / 100.0f);
}

void gameplaymusic::State::updateSelection(Services& services) {
    const unsigned track = selectTrack(services);
    if (selectedTrack != track && selectedTrack != 0xff) {
        prepareTrack(track, 30, services);
        selectedTrack = static_cast<std::uint8_t>(track);
    }
    updateTracks(services);
}

void gameplaymusic::State::updateTracks(Services& services) {
    if (pendingUpdates == 0 || --pendingUpdates != 0) return;
    for (unsigned track = 4; track < 8; ++track) services.setTrackMuted(track, track != selectedTrack);
}

void gameplaymusic::State::updateRoomVolumes(Services& services) {
    if (!services.hasMansionSequence() || selectedTrack == 7) return;
    const unsigned frames = services.sequenceState() == 4 ? 60 : 0;
    if (room == 0x13 || room == 0x20 || room == 0x41) {
        silenceRoomTracks(services, frames);
        return;
    }
    if (services.roomMusicCode() == 7 || services.roomMusicCode() == 6 || (room == 0x23 && roomVariant == 0)) {
        if (selectTrack(services) != 7) {
            for (unsigned track : {0u, 1u, 2u, 3u, 11u}) services.setTrackVolume(track, 1, frames);
        }
        for (unsigned track : {8u, 9u, 10u}) services.setTrackVolume(track, 0, frames);
    } else if (services.roomSize() == 2) {
        if (selectTrack(services) != 7) services.setTrackVolume(9, 1, frames);
        for (unsigned track : {0u, 1u, 2u, 3u, 8u, 10u, 11u}) services.setTrackVolume(track, 0, frames);
    } else {
        if (selectTrack(services) != 7) {
            services.setTrackVolume(8, 1, frames);
            services.setTrackVolume(10, 1, frames);
        }
        for (unsigned track : {0u, 1u, 2u, 3u, 9u, 11u}) services.setTrackVolume(track, 0, frames);
    }
}

void gameplaymusic::State::prepareTrack(unsigned track, unsigned delay, Services& services) {
    if (static_cast<std::uint8_t>(track) == 7) silenceRoomTracks(services, 0);
    else updateRoomVolumes(services);
    pendingUpdates = delay + 1;
}

void gameplaymusic::State::setCutsceneActive(bool active, Services& services) {
    cutsceneActive = active;
    if (!services.hasMansionSequence()) return;
    if (active) {
        pendingUpdates = 1;
        selectedTrack = 0xff;
    } else {
        selectedTrack = static_cast<std::uint8_t>(selectTrack(services));
        prepareTrack(selectedTrack, 0, services);
    }
    updateTracks(services);
}
