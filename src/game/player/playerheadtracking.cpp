#include "game/player/playerheadtracking.h"
#include "game/player/playerorientationupdate.h"
#include "game/actors/moveactor.h"
#include "game/world/dooranimationpose.h"
#include "resources/dolresources.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    using Vector = std::array<float, 3>;
    using Matrix = actormodel::Matrix;

    Vector normalize(Vector value) {
        const float length = std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
        if (length >= 0.0001f) for (auto& component : value) component *= 1 / length;
        else value.fill(0);
        return value;
    }

    Matrix transposeBasis(const Matrix& source) {
        Matrix result{};
        for (unsigned column = 0; column < 3; ++column) {
            const auto value = normalize(normalize({source[column], source[column + 4], source[column + 8]}));
            for (unsigned row = 0; row < 3; ++row) result[column * 4 + row] = value[row];
        }
        return result;
    }

    Vector transform(const Matrix& matrix, const Vector& value) {
        Vector result;
        for (unsigned row = 0; row < 3; ++row) result[row] =
            (matrix[row * 4] * value[0] + matrix[row * 4 + 1] * value[1]) + matrix[row * 4 + 2] * value[2];
        return result;
    }

    float number(std::uint32_t bits) {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    Matrix remap(const Matrix& source, const std::array<std::uint32_t, 10>& parameters, unsigned offset) {
        auto result = source;
        const auto byte = [&](unsigned index) { return (parameters.at(index / 4) >> (24 - (index % 4) * 8)) & 255; };
        for (unsigned column = 0; column < 3; ++column) {
            const auto letter = byte(offset + column * 2 + 1);
            const unsigned axis = letter == 'y' || letter == 'Y' ? 1 : letter == 'z' || letter == 'Z' ? 2 : 0;
            const float sign = byte(offset + column * 2) == '-' ? -1.0f : 1.0f;
            for (unsigned row = 0; row < 3; ++row) result[row * 4 + column] = sign * source[row * 4 + axis];
        }
        return result;
    }

    float degrees(const actormodel::InitialPose& pose, float y, float x) {
        const float value = float(pose.angle(x, y)) * (360.0f / 65536.0f);
        return value >= 180 ? value - 360 : value;
    }

    float constant(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        return number((unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3]);
    }
}

void playercomponents::AttachedSingle::update(unsigned mode, const std::array<float, 3>& target, double blend,
    double verticalScale, double horizontalScale, const actormodel::InitialPose& pose) {
    if (!mode) {
        if (field28) {
            effect.flags &= ~8u;
            field28 = 0;
        }
        return;
    }
    if (!actor) throw std::runtime_error("Head tracking requires its original actor owner.");
    if (!field28) {
        effect.flags |= 8;
        field28 = 1;
    }
    playerorientation::Quaternion retained{field2C, field30, field34, field38};
    if (actor->model) {
        const auto& joint = actor->model->matrix(0, parameters[1]);
        const auto world = dooranimation::concatenate(actor->matrix, joint);
        const auto direction = normalize({target[0] - world[3], target[1] - world[7], target[2] - world[11]});
        auto desired = playerorientation::fromMatrix(dooranimation::concatenate(transposeBasis(actor->matrix),
            playerorientation::headingMatrix(direction, false)));
        if (mode != 4) {
            const auto basis = remap(joint, parameters, 15);
            const auto local = transform(transposeBasis(dooranimation::concatenate(actor->matrix, basis)), direction);
            double vertical = degrees(pose, local[1], std::sqrt(local[0] * local[0] + local[2] * local[2]));
            double horizontal = degrees(pose, local[0], local[2]);
            const double minimumVertical = float(number(parameters[6]) * verticalScale);
            const double maximumVertical = float(number(parameters[7]) * verticalScale);
            const double minimumHorizontal = float(number(parameters[8]) * horizontalScale);
            const double maximumHorizontal = float(number(parameters[9]) * horizontalScale);
            const bool within = minimumVertical <= vertical && vertical <= maximumVertical &&
                minimumHorizontal <= horizontal && horizontal <= maximumHorizontal;
            if (mode == 1 || (mode == 2 && !within)) desired = playerorientation::fromMatrix(basis);
            else if (mode == 3 && !within) {
                vertical = std::clamp(vertical, minimumVertical, maximumVertical);
                horizontal = std::clamp(horizontal, minimumHorizontal, maximumHorizontal);
                const float x = float(0.017453292f * -vertical), y = float(0.017453292f * horizontal);
                const float sx = std::sin(x), cx = std::cos(x), sy = std::sin(y), cy = std::cos(y);
                desired = playerorientation::fromMatrix({cy, sy * sx, sy * cx, 0, 0, cx, -sx, 0, -sy, cy * sx, cy * cx, 0});
            }
        }
        if (!field3C) {
            retained = desired;
            field3C = 1;
        } else retained = playerorientation::interpolate(std::min(blend, 1.0), retained, desired);
        field2C = retained[0];
        field30 = retained[1];
        field34 = retained[2];
        field38 = retained[3];
    }
    const auto result = playerorientation::fromMatrix(remap(playerorientation::toMatrix(retained), parameters, 8));
    effect.field34 = result[0];
    effect.field38 = result[1];
    effect.field3C = result[2];
    effect.field40 = result[3];
}

playercomponents::HeadFrame::HeadFrame(const std::vector<std::uint8_t>& executable) :
    range(constant(executable, 0x804db5d8)), targetHeight(constant(executable, 0x804db59c)),
    blend(constant(executable, 0x804d2ba4)) {}

void playercomponents::HeadFrame::run(playeractor::Player& player, const actors::Registry& actors,
    const enemies::Manager& manager, const actormodel::Catalog& catalog, const actormodel::InitialPose& pose) const {
    const auto& state = player.state;
    if (!(state.flags792 & 0x10)) return;
    auto target = player.position;
    unsigned mode = 1;
    double vertical = 1, horizontal = 1;
    const auto jointPosition = [](const actors::MoveActor& actor) {
        if (!actor.model) return Vector{};
        const auto matrix = dooranimation::concatenate(actor.matrix, actor.model->matrix(0, 0));
        return Vector{matrix[3], matrix[7], matrix[11]};
    };
    if (state.field10D1) {
        mode = 3;
        target = state.field10D4;
    } else if (state.field11A0 && state.field11A0->actor && *state.field11A0->actor) {
        mode = 3;
        target = jointPosition(**state.field11A0->actor);
        vertical = 0.7f;
        horizontal = 0.5f;
    } else {
        const auto& reaction = *player.components->reactions;
        const Vector* reactionPosition = nullptr;
        if (reaction.state != 1) {
            if (reaction.field18) reactionPosition = &reaction.fieldC;
            else if (static_cast<std::int32_t>(reaction.field8) >= 0) {
                const auto* source = actors.actor(reaction.field8);
                if (source && !(source->flags & 0xc000)) reactionPosition = &source->position;
            }
        }
        if (reactionPosition) {
            mode = 3;
            target = *reactionPosition;
        } else {
            const actors::MoveActor* closest = nullptr;
            double distance = range;
            for (const auto& slot : manager.slots()) {
                if (slot.state != 2) continue;
                const auto& actor = dynamic_cast<const actors::MoveActor&>(slot.owner->actor());
                const auto category = catalog.type(actor.actorType).field4;
                if ((actor.flags & 0xc000) || (category != 2 && category != 3 && category != 4)) continue;
                Vector delta;
                for (unsigned i = 0; i < 3; ++i) delta[i] = actor.position[i] - player.position[i];
                const double length = std::sqrt((delta[0] * delta[0] + delta[1] * delta[1]) + delta[2] * delta[2]);
                if (length < distance) {
                    closest = &actor;
                    distance = length;
                }
            }
            if (closest) {
                mode = 2;
                target = jointPosition(*closest);
                target[1] += targetHeight;
            }
        }
    }
    const auto& attached = *player.components->attached;
    if (!(state.field1324 & 4) || attached.field2C0 == 4 || attached.field2C0 == 6 || attached.field2D0 == 5 ||
        state.field1338 == 4 || state.flags1140[2] || state.flags1330[1] || state.field10D0 || !(player.flags & 0x1000)) mode = 0;
    player.components->single->update(mode, target, blend, vertical, horizontal, pose);
}
