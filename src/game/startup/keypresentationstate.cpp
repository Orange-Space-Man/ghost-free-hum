#include "game/startup/keypresentationstate.h"
#include "game/startup/openingresources.h"

#include <stdexcept>

bool keypresentation::State::request(bool missingKey, bool front, unsigned doorType, unsigned playbackMode,
    Services& services, bool overrideGate) {
    if (!overrideGate && (phase != Phase::Idle || owned)) return false;
    mode = playbackMode;
    if (mode == 0 && !overrideGate) {
        if (services.sceneState != 0) return false;
        if (!services.pauseAudio) throw std::runtime_error("Key presentation requires original audio pause ownership.");
        services.sceneState = 255;
        services.pauseAudio(true);
    }
    overrideAdmission = overrideGate;
    cutscene = openingscene::doorCutscene(missingKey, front, doorType);
    phase = Phase::Loading;
    owned = true;
    forceFinish = updated = false;
    services.flag56 = false;
    return true;
}

void keypresentation::State::loaded() {
    if (phase == Phase::Loading) phase = Phase::Loaded;
}

keypresentation::Action keypresentation::State::beginDraw() {
    if (phase == Phase::Loaded) phase = Phase::Preparing;
    else if (phase == Phase::Preparing) return Action::Initialize;
    else if (phase == Phase::Playing) return Action::Update;
    return Action::None;
}

void keypresentation::State::initialized(Services& services) {
    if (phase != Phase::Preparing) throw std::runtime_error("Key presentation initialization outside preparation.");
    phase = Phase::Playing;
    services.gameplayEnabled = mode == 1;
    if (mode == 1) services.doorTransition = 3;
}

bool keypresentation::State::completeUpdate(bool timelineFinished, unsigned pressed, Services& services) {
    if (phase != Phase::Playing) throw std::runtime_error("Key presentation update outside playback.");
    services.gameplayEnabled = mode == 1;
    const bool finished = timelineFinished || forceFinish || (mode != 1 && (pressed & 0x100));
    if (!overrideAdmission && services.flag56) {
        forceFinish = true;
        if (mode == 1) services.doorTransition = 4;
    }
    updated = true;
    return finished;
}

void keypresentation::State::release(Services& services) {
    phase = Phase::Idle;
    owned = false;
    services.gameplayEnabled = true;
    if (!overrideAdmission && mode == 0) {
        if (!services.pauseAudio) throw std::runtime_error("Key presentation requires original audio resume ownership.");
        services.sceneState = 0;
        services.pauseAudio(false);
    }
}
