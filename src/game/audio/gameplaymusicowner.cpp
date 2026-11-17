#include "game/audio/gameplaymusicowner.h"

#include <stdexcept>

gameplaymusic::Owner::Owner(gameassets::Package& package, const jaudio::Resources& resources,
    const gameplayaudio::Context& context, const std::int32_t& playerHealth) :
    package(package), resources(resources), context(context), health(playerHealth) {}

void gameplaymusic::Owner::refreshContext() {
    roomParameters = context.parameters(room, soundPolygon);
    state.playerHealth = static_cast<std::uint32_t>(health);
}

void gameplaymusic::Owner::setRoom(unsigned value, std::int16_t polygon) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    room = value;
    soundPolygon = polygon;
    refreshContext();
}

void gameplaymusic::Owner::notifyRoom(unsigned packedRoom, unsigned mapMode) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    roomAudio.enter(packedRoom, mapMode, state.roomVariant != 0, {
        [this] { return currentSound(); },
        []() -> std::uint32_t { throw std::runtime_error("Room transition requires auxiliary sequence ownership."); },
        [] { throw std::runtime_error("Room transition requires mansion pause and delayed-resume ownership."); },
        [] { throw std::runtime_error("Room transition requires auxiliary sequence release."); },
        [](unsigned) { throw std::runtime_error("Special-room audio requires its original sequence services."); },
        [](unsigned) { throw std::runtime_error("Room transition requires category-four sound-port publication."); },
        [this](unsigned notifiedRoom) {
            refreshContext();
            state.room = notifiedRoom;
            state.updateRoomVolumes(*this);
        }});
}

void gameplaymusic::Owner::start() {
    std::lock_guard<std::recursive_mutex> lock(mutex);
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
    std::lock_guard<std::recursive_mutex> lock(mutex);
    sequence.stop();
    soundState = 0;
    activeSound = 0;
}

bool gameplaymusic::Owner::requestDeath(std::uint32_t audioFrame) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (deferRequest(0x80000012)) return true;
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
    std::lock_guard<std::recursive_mutex> lock(mutex);
    refreshContext();
    state.setCutsceneActive(active, *this);
}

bool gameplaymusic::Owner::requestEntrance(std::uint32_t audioFrame) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (deferRequest(0x80000019)) return true;
    if (lastRequestFrame == audioFrame) return false;
    const auto* info = jaudio::soundInfo(resources, 0x80000019);
    if (!info || (*info)[0] || (*info)[1] || (*info)[2] || (*info)[3])
        throw std::runtime_error("Unsupported original entrance sequence routing metadata.");
    stop();
    std::string error;
    if (!sequence.loadEntrance(package, resources, error)) {
        preparedSound = 0;
        throw std::runtime_error(error);
    }
    preparedSound = activeSound = 0x80000019;
    sequence.start();
    soundState = 4;
    lastRequestFrame = audioFrame;
    return true;
}

void gameplaymusic::Owner::updateSequenceControls() {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    sequence.updateSequenceControls();
    sequenceVolume.update();
    if (fanfareActive) fanfare.updateSequenceControls();
}

bool gameplaymusic::Owner::requestItem(std::uint32_t audioFrame) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (deferRequest(0x8000000e)) return true;
    if (lastRequestFrame == audioFrame) return false;
    const auto* info = jaudio::soundInfo(resources, 0x8000000e);
    if (!info || (*info)[0] != 0x40 || (*info)[1] || (*info)[2] || (*info)[3])
        throw std::runtime_error("Unsupported original item fanfare routing metadata.");
    std::string error;
    if (soundState && activeSound != 0x80000007) {
        if (!fanfare.loadItemFanfare(package, resources, true, error)) throw std::runtime_error(error);
        fanfare.start();
        fanfareActive = true;
        sequenceVolume.set(0, 5);
    } else {
        resumeMansion = hasMansionSequence();
        if (resumeMansion) retainSection();
        stop();
        if (!sequence.loadItemFanfare(package, resources, false, error)) throw std::runtime_error(error);
        preparedSound = activeSound = 0x8000000e;
        sequence.start();
        soundState = 4;
    }
    lastRequestFrame = audioFrame;
    return true;
}

bool gameplaymusic::Owner::requestFirstKey(std::uint32_t audioFrame) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (deferRequest(0x8000001b)) return true;
    if (lastRequestFrame == audioFrame) return false;
    const auto* info = jaudio::soundInfo(resources, 0x8000001b);
    if (!info || (*info)[0] || (*info)[1] || (*info)[2] || (*info)[3])
        throw std::runtime_error("Unsupported original first-key sequence routing metadata.");
    stop();
    std::string error;
    if (!sequence.loadFirstKey(package, resources, error)) {
        preparedSound = 0;
        throw std::runtime_error(error);
    }
    preparedSound = activeSound = 0x8000001b;
    sequence.start();
    soundState = 4;
    lastRequestFrame = audioFrame;
    return true;
}

bool gameplaymusic::Owner::deferRequest(unsigned id) {
    if (!soundState || activeSound != 0x8000001a) return false;
    pendingSound = id;
    return true;
}

bool gameplaymusic::Owner::requestParlor(std::uint32_t audioFrame) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (deferRequest(0x8000001a)) return true;
    if (lastRequestFrame == audioFrame) return false;
    const auto* info = jaudio::soundInfo(resources, 0x8000001a);
    if (!info || (*info)[0] != 2 || (*info)[1] || (*info)[2] || (*info)[3])
        throw std::runtime_error("Unsupported original Parlor sequence routing metadata.");
    stop();
    std::string error;
    if (!sequence.loadParlor(package, resources, error)) {
        preparedSound = 0;
        throw std::runtime_error(error);
    }
    preparedSound = activeSound = 0x8000001a;
    sequence.start();
    soundState = 4;
    lastRequestFrame = audioFrame;
    return true;
}

void gameplaymusic::Owner::updateRequests(std::uint32_t audioFrame, bool enabled, bool paused) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (!pendingSound || soundState) return;
    const auto pending = pendingSound;
    pendingSound = 0;
    if (!enabled || paused) return;
    switch (pending) {
    case 0x80000012: requestDeath(audioFrame); break;
    case 0x8000000c: requestLab(audioFrame); break;
    case 0x80000025: requestTraining(audioFrame); break;
    case 0x80000019: requestEntrance(audioFrame); break;
    case 0x8000001a: requestParlor(audioFrame); break;
    case 0x8000001b: requestFirstKey(audioFrame); break;
    case 0x8000000e: requestItem(audioFrame); break;
    case 0x80000008: case 0x8000000f: case 0x80000026: case 0x80000027: requestSequence(pending, audioFrame); break;
    default: throw std::logic_error("Pending music request has no recovered route.");
    }
}

void gameplaymusic::Owner::retainSection() {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (hasMansionSequence() && soundState > 3) lastSection = sequence.rootPort(1);
}

bool gameplaymusic::Owner::mix(jaudio::DspMixer& destination, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (fanfareActive) {
        if (!fanfare.mix(destination, error)) return false;
        if (fanfare.voicesFinished()) {
            fanfareActive = false;
            sequenceVolume.set(1, 20);
        }
    }
    if (!soundState) return true;
    jaudio::SpatialParameters parameters;
    parameters.volume = sequenceVolume.value;
    if (sequence.mix(destination, error, parameters)) {
        if (sequence.voicesFinished()) {
            soundState = 0;
            activeSound = 0;
            if (resumeMansion) {
                resumeMansion = false;
                start();
            }
        }
        return true;
    }
    soundState = 0;
    activeSound = 0;
    return false;
}

const gameplaymusic::State& gameplaymusic::Owner::routing() const { return state; }
const jaudio::Playback& gameplaymusic::Owner::playback() const { return sequence; }
unsigned gameplaymusic::Owner::currentSound() const {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return activeSound;
}
bool gameplaymusic::Owner::hasMansionSequence() const { return soundState != 0 && activeSound == 0x80000007; }
unsigned gameplaymusic::Owner::sequenceState() const { return soundState; }
short gameplaymusic::Owner::roomSize() { return static_cast<short>(roomParameters.size()); }
short gameplaymusic::Owner::roomMusicCode() { return static_cast<short>(roomParameters.musicCode()); }
void gameplaymusic::Owner::setTrackMuted(unsigned track, bool muted) { sequence.setTrackMuted(track, muted); }
void gameplaymusic::Owner::setTrackVolume(unsigned track, float volume, unsigned frames) { sequence.setTrackVolume(track, volume, frames); }

bool gameplaymusic::Owner::requestLab(std::uint32_t audioFrame) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (deferRequest(0x8000000c)) return true;
    if (lastRequestFrame == audioFrame) return false;
    const auto* info = jaudio::soundInfo(resources, 0x8000000c);
    if (!info || (*info)[0] || (*info)[1] || (*info)[2] || (*info)[3])
        throw std::runtime_error("Unsupported original lab sequence routing metadata.");
    stop();
    std::string error;
    if (!sequence.loadLab(package, resources, error)) {
        preparedSound = 0;
        throw std::runtime_error(error);
    }
    preparedSound = activeSound = 0x8000000c;
    sequence.start();
    soundState = 4;
    lastRequestFrame = audioFrame;
    return true;
}

// Event BGM requests (8018A470 through table 8038EAB4) for sequences without a dedicated request above; same flow as 8018A470's
// 801889FC route for a plain JaiInit-routed sequence.
bool gameplaymusic::Owner::requestSequence(std::uint32_t id, std::uint32_t audioFrame) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (deferRequest(id)) return true;
    if (lastRequestFrame == audioFrame) return false;
    const auto* info = jaudio::soundInfo(resources, id);
    if (!info || (*info)[0] || (*info)[1] || (*info)[2] || (*info)[3])
        throw std::runtime_error("Unsupported original event sequence routing metadata.");
    stop();
    std::string error;
    if (!sequence.loadSequence(package, resources, id, error)) {
        preparedSound = 0;
        throw std::runtime_error(error);
    }
    preparedSound = activeSound = id;
    sequence.start();
    soundState = 4;
    lastRequestFrame = audioFrame;
    return true;
}

bool gameplaymusic::Owner::requestTraining(std::uint32_t audioFrame) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (deferRequest(0x80000025)) return true;
    if (lastRequestFrame == audioFrame) return false;
    const auto* info = jaudio::soundInfo(resources, 0x80000025);
    if (!info || (*info)[0] || (*info)[1] || (*info)[2] || (*info)[3])
        throw std::runtime_error("Unsupported original training sequence routing metadata.");
    stop();
    std::string error;
    if (!sequence.loadTraining(package, resources, error)) {
        preparedSound = 0;
        throw std::runtime_error(error);
    }
    preparedSound = activeSound = 0x80000025;
    sequence.start();
    soundState = 4;
    lastRequestFrame = audioFrame;
    return true;
}
