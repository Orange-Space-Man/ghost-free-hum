#include "game/player/playerscareresponse.h"
#include "game/actors/actorregistry.h"
#include "resources/dolresources.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    float number(std::uint32_t bits) {
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    std::array<float, 3> transform(const actormodel::Matrix& matrix, const std::array<float, 3>& value) {
        std::array<float, 3> result;
        for (unsigned row = 0; row < 3; ++row) {
            const unsigned offset = row * 4;
            const float xy = std::fma(matrix[offset], value[0], matrix[offset + 1] * value[1]);
            result[row] = xy + std::fma(matrix[offset + 2], value[2], matrix[offset + 3]);
        }
        return result;
    }
}

playerevents::ScareResponse::ScareResponse(const std::vector<std::uint8_t>& executable,
    const actormodel::Catalog& models) : collisions(executable, models), pose(executable) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, 0x804d2bf8, 4, bytes, error)) throw std::runtime_error(error);
    maximumDistance = number((unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) |
        (unsigned(bytes[2]) << 8) | bytes[3]);
}

bool playerevents::ScareResponse::visible(const gameplaycamera::CameraOwner& camera,
    const std::array<float, 3>& position) {
    std::array<float, 4> projected;
    for (unsigned row = 0; row < 4; ++row) {
        const auto& matrix = camera.matrix;
        projected[row] = ((matrix[row * 4 + 3] + matrix[row * 4 + 2] * position[2]) +
            matrix[row * 4] * position[0]) + matrix[row * 4 + 1] * position[1];
    }
    if (projected[3] == 0) return false;
    const float inverse = 1 / projected[3];
    for (unsigned i = 0; i < 3; ++i) projected[i] *= inverse;
    return !(projected[0] <= -1 || projected[0] >= 1 || projected[1] <= -1 || projected[1] >= 1 || projected[2] >= 0);
}

bool playerevents::ScareResponse::visible(const gameplaycamera::CameraOwner& camera,
    const actors::MoveActor& actor) const {
    for (const auto& record : collisions.type(actor.type()).records) {
        std::array<float, 3> center{number(record[2]), number(record[3]), number(record[4])};
        const auto joint = static_cast<std::int32_t>(record[0]);
        if (joint < 0) {
            auto matrix = pose.rotation(actor.rotation);
            for (unsigned i = 0; i < 3; ++i) matrix[i * 4 + 3] = actor.position[i];
            center = transform(matrix, center);
        } else if (actor.model) {
            center = transform(actor.model->matrix(0, static_cast<unsigned>(joint)), center);
            center = transform(actor.matrix, center);
        } else center.fill(0);
        if (visible(camera, center)) return true;
    }
    return false;
}

unsigned playerevents::ScareResponse::run(playeractor::Player& player, const Payload& payload,
    const actors::Registry& actors) const {
    auto& actions = *player.components->actions;
    if (actions.field310 == 2 && actions.field314 == 0x20) return 0;
    if (actions.field310 == 2 && actions.field35C == 4 &&
        (actions.field314 == 0xe || actions.field314 == 0xc)) return 0;
    if (!(player.state.field1114 <= 0) || static_cast<std::int16_t>(player.life) <= 0) return 0;
    const auto id = static_cast<std::int32_t>(payload.word(8));
    const actors::MoveActor* source = nullptr;
    const auto* position = payload.scarePosition;
    if (id >= 0) {
        source = actors.actor(id);
        if (!source) throw std::runtime_error("Player scare source is absent from the actor registry.");
        position = &source->position;
    } else if (payload.word(4)) {
        throw std::runtime_error("Player scare requires a native source position.");
    }
    if (position) {
        const float x = (*position)[0] - player.position[0], y = (*position)[1] - player.position[1],
            z = (*position)[2] - player.position[2];
        if (!(std::sqrt((x * x + y * y) + z * z) < maximumDistance)) return 0;
        if (!player.state.camera) throw std::runtime_error("Player scare requires its original camera owner.");
        if (source ? !visible(*player.state.camera, *source) : !visible(*player.state.camera, *position)) return 0;
    }
    std::array<std::uint32_t, 12> request{};
    request[0] = 5;
    actions.request(2, &request);
    return 1;
}
