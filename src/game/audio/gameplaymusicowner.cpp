#include "game/audio/gameplaymusicowner.h"

#include <stdexcept>

gameplaymusic::Owner::Owner(gameassets::Package& package, const jaudio::Resources& resources,
    const gameplayaudio::Context& context, const std::int32_t& playerHealth) :
    package(package), resources(resources), context(context), health(playerHealth) {}

void gameplaymusic::Owner::refreshContext() {
    roomParameters = context.parameters(room, soundPolygon);
    state.room = room & 255;
    state.playerHealth = static_cast<std::uint32_t>(health);
}

void gameplaymusic::Owner::setRoom(unsigned value, std::int16_t polygon) {
    room = value;
    soundPolygon = polygon;
    refreshContext();
}

void gameplaymusic::Owner::start() {
    if (soundState) throw std::runtime_error("Mansion music replacement requires the original stop lifecycle.");
    if (preparedSound != 0x80000007) {
        std::string error;
        if (!sequence.loadMansion(package, resources, error)) throw std::runtime_error(error);
        preparedSound = 0x80000007;
    }
    refreshContext();
    sequence.start();
    activeSound = 0x80000007;
    soundState = 3;
    sequence.setRootPort(0, static_cast<std::uint16_t>(lastSection < 14 ? lastSection + 1 : 3));
    sequence.setTrackPort(15, 15, 0xffff);
    sequence.setTempoScale(0.5f, 1);
    state.selectedTrack = static_cast<std::uint8_t>(state.selectTrack(*this));
    state.prepareTrack(state.selectedTrack, 0, *this);
    if (state.cutsceneActive) state.setCutsceneActive(true, *this);
    else state.updateTracks(*this);
    state.updateRoomVolumes(*this);
    soundState = 4;
}

void gameplaymusic::Owner::stop() {
    sequence.stop();
    soundState = 0;
    activeSound = 0;
}

bool gameplaymusic::Owner::requestDeath(std::uint32_t audioFrame) {
    if (lastRequestFrame == audioFrame) return false;
    const auto* info = jaudio::soundInfo(resources, 0x80000012);
    if (!info || (*info)[0] || (*info)[1] || (*info)[2] || (*info)[3])
        throw std::runtime_error("Unsupported original death sequence routing metadata.");
    stop();
    std::string error;
    if (!sequence.loadDeath(package, resources, error)) {
        preparedSound = 0;
        throw std::runtime_error(error);
    }
    preparedSound = activeSound = 0x80000012;
    sequence.start();
    soundState = 4;
    lastRequestFrame = audioFrame;
    return true;
}

void gameplaymusic::Owner::setCutsceneActive(bool active) {
    refreshContext();
    state.setCutsceneActive(active, *this);
}

void gameplaymusic::Owner::updateSequenceControls() { sequence.updateSequenceControls(); }

void gameplaymusic::Owner::retainSection() {
    if (hasMansionSequence() && soundState > 3) lastSection = sequence.rootPort(1);
}

bool gameplaymusic::Owner::mix(jaudio::DspMixer& destination, std::string& error) {
    if (!soundState) return true;
    if (sequence.mix(destination, error)) {
        if (sequence.voicesFinished()) {
            soundState = 0;
            activeSound = 0;
        }
        return true;
    }
    soundState = 0;
    activeSound = 0;
    return false;
}

const gameplaymusic::State& gameplaymusic::Owner::routing() const { return state; }
const jaudio::Playback& gameplaymusic::Owner::playback() const { return sequence; }
unsigned gameplaymusic::Owner::currentSound() const { return activeSound; }
bool gameplaymusic::Owner::hasMansionSequence() const { return soundState != 0 && activeSound == 0x80000007; }
unsigned gameplaymusic::Owner::sequenceState() const { return soundState; }
short gameplaymusic::Owner::roomSize() { return static_cast<short>(roomParameters.size()); }
short gameplaymusic::Owner::roomMusicCode() { return static_cast<short>(roomParameters.musicCode()); }
void gameplaymusic::Owner::setTrackMuted(unsigned track, bool muted) { sequence.setTrackMuted(track, muted); }
void gameplaymusic::Owner::setTrackVolume(unsigned track, float volume, unsigned frames) { sequence.setTrackVolume(track, volume, frames); }
