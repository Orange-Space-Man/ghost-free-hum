#pragma once

#include <functional>

namespace keypresentation {
    enum class Phase : unsigned { Idle, Loading, Loaded, Preparing, Playing };
    enum class Action { None, Initialize, Update };

    struct Services {
        unsigned& sceneState;
        unsigned& doorTransition;
        bool& gameplayEnabled;
        bool& flag56;
        std::function<void(bool)> setAudioCutsceneActive;
    };

    struct State {
        Phase phase = Phase::Idle;
        unsigned cutscene = 0;
        unsigned mode = 0;
        bool owned = false, forceFinish = false, updated = false;
        bool overrideAdmission = false;

        bool request(bool missingKey, bool front, unsigned doorType, unsigned playbackMode, Services& services, bool overrideGate = false);
        void loaded();
        Action beginDraw();
        void initialized(Services& services);
        bool completeUpdate(bool timelineFinished, unsigned pressed, Services& services);
        void release(Services& services);
    };
}
