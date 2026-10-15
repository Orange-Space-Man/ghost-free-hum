#include "game/audio/gameplaymusicrouting.h"
#include "game/startup/keypresentationstate.h"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    struct Music : gameplaymusic::Services {
        bool present = true;
        unsigned state = 0, writes = 0, lastFrames = 0;
        short category = 0, musicCategory = 0;
        std::array<bool, 12> muted{};
        std::array<float, 12> volumes{};
        bool hasMansionSequence() const override { return present; }
        unsigned sequenceState() const override { return state; }
        short roomSize() override { return category; }
        short roomMusicCode() override { return musicCategory; }
        void setTrackMuted(unsigned track, bool value) override { muted.at(track) = value; ++writes; }
        void setTrackVolume(unsigned track, float value, unsigned frames) override {
            volumes.at(track) = value;
            lastFrames = frames;
            ++writes;
        }
    };
}

int main() {
    try {
        gameplaymusic::State routing;
        Music music;
        for (unsigned level : {0u, 40u, 41u, 70u, 71u, 100u}) {
            routing.playerHealth = level;
            require(routing.selectTrack(music) == (level < 41 ? 6 : level < 71 ? 5 : 4), "Track selection boundaries changed.");
        }
        routing.alternateSelection = 1;
        require(routing.selectTrack(music) == 7, "Alternate track missing.");
        music.category = 2;
        require(routing.selectTrack(music) == 4, "Category two must retain level selection.");
        routing.alternateSelection = 2;
        music.category = 0;
        require(routing.selectTrack(music) == 4, "Alternate selection requires exactly one.");

        music.present = false;
        routing.setCutsceneActive(true, music);
        require(routing.cutsceneActive && music.writes == 0, "Other music must remain untouched.");
        music.present = true;
        unsigned scene = 0, transition = 0;
        bool gameplay = true, flag = false;
        keypresentation::Services services{scene, transition, gameplay, flag,
            [&](bool active) { routing.setCutsceneActive(active, music); }};
        keypresentation::State movie;
        require(movie.request(false, false, 1, 0, services), "Key movie admission failed.");
        for (unsigned track = 4; track < 8; ++track) require(music.muted[track], "Cutscene must mute all four selection tracks.");
        require(!music.muted[0] && !music.muted[8], "Cutscene must not globally mute music.");
        movie.release(services);
        require(!routing.cutsceneActive && !music.muted[4] && music.muted[5] && music.muted[6] && music.muted[7], "Movie release must restore selection.");
        require(music.volumes[8] == 1 && music.volumes[10] == 1 && music.volumes[9] == 0, "Default room layers changed.");
        music.category = 2;
        music.state = 4;
        routing.updateRoomVolumes(music);
        require(music.volumes[9] == 1 && music.volumes[8] == 0 && music.lastFrames == 60, "Room category or transition duration changed.");
        music.musicCategory = 7;
        routing.updateRoomVolumes(music);
        require(music.volumes[0] == 1 && music.volumes[11] == 1 && music.volumes[9] == 0, "Special room layers changed.");
        routing.room = 0x13;
        routing.updateRoomVolumes(music);
        for (unsigned track : {0u, 1u, 2u, 3u, 8u, 9u, 10u, 11u}) require(music.volumes[track] == 0, "Silent room retained a layer.");
        routing.pendingUpdates = 2;
        const unsigned before = music.writes;
        routing.updateTracks(music);
        require(music.writes == before, "Track delay completed early.");
        routing.updateTracks(music);
        require(music.writes == before + 4, "Track delay failed to commit.");
        routing.updateTracks(music);
        require(music.writes == before + 4, "Completed track update repeated.");
        routing.prepareTrack(4, 256, music);
        require(routing.pendingUpdates == 257, "Original track delay must retain all 32 bits.");
        routing.prepareTrack(4, 0xffffffffu, music);
        require(routing.pendingUpdates == 0, "Original delay overflow changed.");
        routing.room = 0x23;
        music.musicCategory = 0;
        music.category = 0;
        routing.updateRoomVolumes(music);
        require(music.volumes[0] == 1 && music.volumes[8] == 0, "Room variant zero lost its special layers.");
        routing.roomVariant = 1;
        routing.updateRoomVolumes(music);
        require(music.volumes[0] == 0 && music.volumes[8] == 1, "Room variant did not restore ordinary layers.");
        routing.alternateSelection = 1;
        routing.setCutsceneActive(false, music);
        require(routing.selectedTrack == 7 && !music.muted[7], "Alternate selection was not restored.");
        for (unsigned track : {0u, 1u, 2u, 3u, 8u, 9u, 10u, 11u}) require(music.volumes[track] == 0, "Alternate selection retained room layers.");
        routing.alternateSelection = 0;
        routing.playerHealth = 40;
        routing.updateSelection(music);
        require(routing.selectedTrack == 6 && routing.pendingUpdates == 30 && !music.muted[7], "Selection transition lost its original delay.");
        for (unsigned track : {0u, 1u, 2u, 3u, 8u, 9u, 10u, 11u}) require(music.volumes[track] == 0, "Leaving alternate selection changed original room-update ordering.");
        for (unsigned frame = 0; frame < 29; ++frame) routing.updateSelection(music);
        require(routing.pendingUpdates == 1 && music.muted[6], "Selection committed before its final frame.");
        routing.updateSelection(music);
        require(!music.muted[6] && music.muted[7] && routing.pendingUpdates == 0, "Delayed selection did not commit.");
        routing.playerHealth = 71;
        routing.updateSelection(music);
        routing.playerHealth = 41;
        routing.updateSelection(music);
        require(routing.selectedTrack == 5 && routing.pendingUpdates == 30, "New health selection did not restart the delay.");
        routing.setCutsceneActive(true, music);
        routing.updateSelection(music);
        require(routing.selectedTrack == 0xff && routing.pendingUpdates == 0, "Recurring selection escaped cutscene suppression.");

        gameplaymusic::Tempo tempo;
        require(tempo.update(4, 100, 0, 0, 0) == 1 && tempo.transitionFrames == 0, "Ordinary tempo initialization changed.");
        require(tempo.update(6, 0, 0, 0, 0) == 0.6f, "Low-health tempo changed.");
        const float fast = tempo.update(7, 100, 110, 100, 103);
        require(std::abs(fast - 1.5f * 0.8333f) < 0.000001f && tempo.interval == 8 && tempo.transitionFrames == 10, "Short event interval changed.");
        tempo.update(7, 100, 115, 100, 105);
        require(tempo.interval == 10, "Event interval doubling changed.");
        tempo.update(7, 100, 115, 100, 108);
        require(tempo.interval == 16, "Eight-frame event interval changed.");
        tempo.update(7, 100, 115, 100, 109);
        require(tempo.interval == 16, "Nine-frame event boundary changed.");
        require(std::abs(tempo.update(7, 100, 116, 100, 103) - 0.9f) < 0.000001f, "Sixteen-frame expiry changed.");
        tempo.update(7, 100, 3, 0xfffffffcu, 1);
        require(tempo.interval == 10, "Original unsigned frame wrap changed.");
        tempo.update(7, 100, 99, 100, 103);
        require(tempo.interval == 16, "Future event frame must use unsigned subtraction.");
        require(tempo.update(4, 100, 0, 0, 0) == 1 && tempo.transitionFrames == 10, "Ordinary track reset must retain transition duration.");
        require(tempo.update(4, 200, 0, 0, 0) > 1, "Original health input must not be clamped.");
        std::cout << "Gameplay music routing passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
