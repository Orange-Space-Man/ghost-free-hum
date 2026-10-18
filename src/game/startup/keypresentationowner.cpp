#include "game/startup/keypresentationowner.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <stdexcept>

keypresentation::Owner::Owner(const std::filesystem::path& package, const std::vector<std::uint8_t>& executable,
    SceneControl& control, gameflags::State& flags, gameplaymusic::Owner& music) :
    package(package), control(control), flags(flags), music(music) {
    std::vector<std::uint8_t> definitions;
    std::string error;
    if (!dolresources::read(executable, glme01::cutsceneDefinitions + 3 * 24, 56 * 24, definitions, error))
        throw std::runtime_error(error);
    for (unsigned index = 0; index < modes.size(); ++index) {
        const auto offset = index * 24 + 16;
        modes[index] = (unsigned(definitions[offset]) << 24) | (unsigned(definitions[offset + 1]) << 16) |
            (unsigned(definitions[offset + 2]) << 8) | definitions[offset + 3];
    }
}

keypresentation::Owner::~Owner() { release(); }

keypresentation::Services keypresentation::Owner::services() {
    flag56 = flags.value(0x56);
    return {control.state, control.doorTransition, control.gameplayEnabled, flag56,
        [this](bool active) { music.setCutsceneActive(active); }};
}

bool keypresentation::Owner::request(bool missingKey, bool front, unsigned doorType) {
    if (retained.phase != Phase::Idle || retained.owned) return false;
    auto bindings = services();
    const auto cutscene = openingscene::doorCutscene(missingKey, front, doorType);
    if (!retained.request(missingKey, front, doorType, modes.at(cutscene - 3), bindings)) return false;
    flags.setOff(0x56);
    auto loaded = std::make_unique<openingscene::Scene>();
    std::string error;
    if (!loaded->load(package, cutscene, error)) {
        release();
        throw std::runtime_error(error);
    }
    movie = std::move(loaded);
    retained.loaded();
    return true;
}

void keypresentation::Owner::advanceDraw(unsigned pressed) {
    auto bindings = services();
    switch (retained.beginDraw()) {
    case Action::Initialize:
        movie->reset();
        retained.initialized(bindings);
        break;
    case Action::Update:
        movie->update(flags.value(0x55));
        if (movie->clearsEventFlag()) flags.setOff(0x55);
        if (retained.completeUpdate(movie->finished(), pressed, bindings)) release();
        break;
    case Action::None:
        break;
    }
}

void keypresentation::Owner::draw(bool present) {
    if (movie && retained.phase == Phase::Playing && retained.updated) movie->draw(present);
}

void keypresentation::Owner::release() {
    if (!retained.owned) return;
    movie.reset();
    auto bindings = services();
    retained.release(bindings);
}

const keypresentation::State& keypresentation::Owner::state() const { return retained; }
