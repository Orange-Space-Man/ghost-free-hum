#pragma once

#include <cstdint>

namespace gameplaymusic {
    inline constexpr std::uint32_t mansionSequence = 0x80000007;

    struct Tempo {
        std::uint32_t interval = 0;
        unsigned transitionFrames = 0;
        float scale = 0;

        float update(unsigned track, unsigned health, std::uint32_t currentFrame,
            std::uint32_t previousEventFrame, std::uint32_t latestEventFrame);
    };

    struct Services {
        virtual ~Services() = default;
        virtual bool hasMansionSequence() const = 0;
        virtual unsigned sequenceState() const = 0;
        virtual short roomSize() = 0;
        virtual short roomMusicCode() = 0;
        virtual void setTrackMuted(unsigned track, bool muted) = 0;
        virtual void setTrackVolume(unsigned track, float volume, unsigned frames) = 0;
    };

    struct State {
        bool cutsceneActive = false;
        std::uint8_t alternateSelection = 0;
        std::uint8_t roomVariant = 0;
        std::uint8_t selectedTrack = 0xff;
        std::uint32_t pendingUpdates = 0;
        unsigned playerHealth = 0;
        unsigned room = 0;

        unsigned selectTrack(Services& services) const;
        void updateTracks(Services& services);
        void updateSelection(Services& services);
        void updateRoomVolumes(Services& services);
        void prepareTrack(unsigned track, unsigned delay, Services& services);
        void setCutsceneActive(bool active, Services& services);
    };
}
