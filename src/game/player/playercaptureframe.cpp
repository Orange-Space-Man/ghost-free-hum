#include "game/player/playercaptureframe.h"
#include "game/player/playerorientationupdate.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <stdexcept>
#include <cmath>
#include <cstring>

namespace {
    constexpr std::uint32_t captureActive = 0x80000;
    constexpr unsigned captureSound = 0x850;

    std::uint32_t word(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }
}

playeractor::CaptureFrame::CaptureFrame(const std::vector<std::uint8_t>& executable) {
    concealFrames = static_cast<std::int32_t>(word(executable, glme01::playerCaptureConcealFrames));
    auto bits = word(executable, glme01::playerCaptureLaunchSpeed);
    std::memcpy(&launchSpeed, &bits, sizeof(bits));
    bits = word(executable, glme01::playerDirectionEpsilon);
    std::memcpy(&directionEpsilon, &bits, sizeof(bits));
}

bool playeractor::CaptureFrame::begin(Player& player, const std::array<float, 3>& position, std::int32_t recipient,
    actorcapture::Motion& motion, const playerevents::CollectionSound& sound) const {
    auto& state = player.state;
    if (player.components->actions->field310 == 2 || state.field11A0 || state.field1060) return false;
    if (!sound) throw std::runtime_error("Player capture requires positional playback.");
    state.flags1040[2] = 1;
    state.capturePosition = &position;
    state.field1054 = recipient;
    player.initializeCapture(motion, position, 2);
    sound(captureSound, player.position);
    return true;
}

bool playeractor::CaptureFrame::release(Player& player, const std::array<float, 3>* position,
    const std::array<float, 3>* direction, bool launched, const actormodel::InitialPose& pose) const {
    if (!position || !direction) return false;
    player.setPosition(*position);
    player.state.field105C = static_cast<float>(concealFrames);
    player.state.field1058 = 0;
    std::array<std::uint32_t, 12> payload{};
    payload[1] = 6;
    payload[3] = payload[4] = 1;
    player.components->actions->request(launched ? 17 : 3, &payload);
    std::array<float, 3> velocity{};
    if (launched) for (unsigned axis = 0; axis < 3; ++axis) velocity[axis] = launchSpeed * (*direction)[axis];
    player.setVelocity(velocity);
    std::array<float, 3> facing{-(*direction)[0], 0, -(*direction)[2]};
    const float magnitude = std::sqrt((facing[0] * facing[0] + facing[1] * facing[1]) + facing[2] * facing[2]);
    if (magnitude >= directionEpsilon) {
        const float inverse = 1 / magnitude;
        for (auto& value : facing) value *= inverse;
    } else facing.fill(0);
    player.setRotation(pose.angles(playerorientation::headingMatrix(facing, false)));
    player.refreshView();
    return true;
}

void playeractor::CaptureFrame::updateOpacity(Player& player) const {
    const auto& state = player.state;
    const auto alpha = static_cast<std::uint8_t>(state.field1058 || state.field105C == static_cast<float>(concealFrames) ?
        0 : state.fieldF5C);
    player.visibility(alpha, alpha);
    for (auto* actor : {state.field7A4, state.field7A8, state.field7AC}) if (actor) actor->visibility(alpha, alpha);
}

void playeractor::CaptureFrame::run(Player& player, const playerevents::Dispatcher& events,
    particles::EffectFactory& effects) const {
    auto& state = player.state;
    if (!state.flags1040[2]) return;
    if (!state.capturePosition) throw std::runtime_error("Player capture lost its retained target position.");
    if (player.flags & captureActive) {
        player.field100 = *state.capturePosition;
        state.field1048 = {};
        if (player.model && player.model->model) {
            const auto& root = player.model->matrix(0, 0);
            for (unsigned axis = 0; axis < 3; ++axis) state.field1048[axis] =
                ((player.matrix[axis * 4] * root[3] + player.matrix[axis * 4 + 1] * root[7]) +
                    player.matrix[axis * 4 + 2] * root[11]) + player.matrix[axis * 4 + 3];
        }
        return;
    }
    state.flags1040[2] = 0;
    if (state.field1054 < 0) {
        player.setPosition(*state.capturePosition);
        state.field1058 = 1;
        updateOpacity(player);
    } else {
        auto* recipient = events.player(state.field1054);
        if (!recipient || !recipient->view.field8A4 || !recipient->view.field8A8)
            throw std::runtime_error("Player capture recipient has no bound weapon view.");
        playerevents::Payload payload;
        payload.word(0x60, 3);
        payload.word(0x68, player.actorType);
        events.send(state.field1054, 0x13, &payload);
        effects.createCapture(1, *recipient->view.field8A4, *recipient->view.field8A8);
    }
    state.field1048 = *state.capturePosition;
}
