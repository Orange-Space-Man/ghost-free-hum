#include "game/player/playerweaponowner.h"
#include "game/actors/moveactor.h"
#include "game/world/dooranimationpose.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    unsigned axis(unsigned letter) {
        if (letter == 'y' || letter == 'Y') return 1;
        if (letter == 'z' || letter == 'Z') return 2;
        return 0;
    }

    float asFloat(std::uint32_t bits) {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    void endpoint(const actors::MoveActor& actor, const playerweapons::Part& part, unsigned index,
        std::array<float, 3>& position, std::array<float, 3>& direction, std::uint8_t* retained) {
        auto matrix = actor.matrix;
        if (actor.model) matrix = dooranimation::concatenate(matrix, actor.model->matrix(0, part.parameters[index]));
        std::memcpy(retained, matrix.data(), sizeof(matrix));
        const unsigned positionAxis = axis(part.parameters[index + 1] >> 24);
        const unsigned directionAxis = axis((part.parameters[index + 3] >> 16) & 255);
        const float offset = asFloat(part.parameters[index + 2]);
        for (unsigned row = 0; row < 3; ++row) {
            position[row] = matrix[row * 4 + 3] + matrix[row * 4 + positionAxis] * offset;
            direction[row] = matrix[row * 4 + directionAxis];
        }
        const float length = std::sqrt((direction[0] * direction[0] + direction[1] * direction[1]) + direction[2] * direction[2]);
        if (length >= 0.0001f) {
            const float inverse = 1 / length;
            for (auto& value : direction) value *= inverse;
        } else direction.fill(0);
        if ((part.parameters[index + 3] >> 24) == '-') for (auto& value : direction) value = -value;
    }
}

playerweapons::Owner::Owner(const std::vector<std::uint8_t>& executable, const playermodels::SharedModels& shared,
    playermodels::PipeParameters& parameters) {
    first = std::make_unique<Part>(executable, actorType, firstType);
    second = std::make_unique<Part>(executable, actorType, secondType);
    pipe = std::make_unique<playermodels::PipeModel>(shared, 15);
    effect.flags = 0;
    parameters.field30 = 0;
    effect.field40 = 0;
    effect.field34 = 1;
    effect.field38 = effect.field3C = 0;
    parameters.field1D = 1;
    parameters.field18 = -1.6f;
    parameters.field24 = {0.95f, 0.8f, 0.8f};
    parameters.field34 = 0.5f;
    parameters.field38 = 0.88f;
    parameters.field1C = 1;
    parameters.field44 = 0.4f;
    parameters.field20 = 150;
    playermodels::PipeParameters initial;
    initial.fields[0] = &second->position;
    initial.fields[1] = &first->position;
    initial.field20 = 150;
    pipe->initialize(0, initial, nullptr);
    fieldC = 2;
}

playerweapons::Owner::~Owner() {
    pipe.reset();
    first.reset();
    second.reset();
}

void playerweapons::Owner::update(const std::vector<std::uint8_t>& executable, actors::MoveActor& player,
    actors::MoveActor* firstActor, actors::MoveActor* secondActor, unsigned action, unsigned attachedAnimation,
    unsigned attachedState, bool alternate, double shake, const actormodel::InitialPose& pose,
    const std::function<std::uint32_t()>& random) {
    const int nextFirst = firstActor ? static_cast<int>(firstActor->actorType) : -1;
    const int nextSecond = secondActor ? static_cast<int>(secondActor->actorType) : -1;
    const bool changed = actorType != static_cast<int>(player.actorType);
    actorType = static_cast<int>(player.actorType);
    if (changed || firstType != nextFirst) {
        firstType = nextFirst;
        first->reset(executable, actorType, firstType);
    }
    if (changed || secondType != nextSecond) {
        secondType = nextSecond;
        if (secondActor && field1C) {
            field1C = 0;
            effect.flags &= ~0x100u;
        }
        second->reset(executable, actorType, secondType);
    }
    if (secondActor && !field1C) {
        effect.flags |= 0x100;
        field1C = 1;
    }
    const auto select = [&](actors::MoveActor* actor, const Part& part) {
        if (!actor) return;
        const auto animation = part.animation(action, attachedAnimation, attachedState, alternate);
        if (animation != actor->animationState) actor->selectAnimation(animation);
    };
    select(firstActor, *first);
    select(secondActor, *second);
    if (firstActor) player.updateChildAttachment(*firstActor, first->joint(action, attachedAnimation, attachedState, alternate), nullptr, 1);
    actormodel::Matrix rotation{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    if (shake <= 0) effect.fields44 = rotation;
    else {
        if (!random) throw std::runtime_error("Weapon shake requires the original random stream.");
        const auto elevation = pose.horizontalDirection(static_cast<std::uint16_t>(static_cast<unsigned>(float(random()) / 65536)));
        const auto azimuth = pose.horizontalDirection(static_cast<std::uint16_t>(static_cast<unsigned>(float(random()) / 65536)));
        std::array<float, 3> axis{elevation[2] * azimuth[2], elevation[0], elevation[2] * azimuth[0]};
        const float inverseLength = 1 / std::sqrt((axis[0] * axis[0] + axis[1] * axis[1]) + axis[2] * axis[2]);
        for (auto& value : axis) value *= inverseLength;
        const float angle = 0.017453292f * float(15.0 * shake);
        const float sine = std::sin(angle), cosine = std::cos(angle), remainder = 1 - cosine;
        for (unsigned row = 0; row < 3; ++row) for (unsigned column = 0; column < 3; ++column)
            rotation[row * 4 + column] = axis[row] * axis[column] * remainder + (row == column ? cosine : 0);
        rotation[1] -= axis[2] * sine;
        rotation[2] += axis[1] * sine;
        rotation[4] += axis[2] * sine;
        rotation[6] -= axis[0] * sine;
        rotation[8] -= axis[1] * sine;
        rotation[9] += axis[0] * sine;
    }
    if (secondActor) player.updateChildAttachment(*secondActor,
        second->joint(action, attachedAnimation, attachedState, alternate), shake <= 0 ? nullptr : &rotation, 1);
    fieldC = secondActor && (secondActor->flags & 8) ? 5 : 2;
}

void playerweapons::Owner::updateEndpoints(const actors::MoveActor& firstActor, const actors::MoveActor& secondActor) {
    endpoint(firstActor, *first, 10, first->position, first->field610, first->fields634.data());
    endpoint(firstActor, *first, 14, first->field61C, first->field628, first->fields634.data() + 48);
    endpoint(secondActor, *second, 10, second->position, second->field610, second->fields634.data());
    endpoint(secondActor, *second, 14, second->field61C, second->field628, second->fields634.data() + 48);
}

void playerweapons::Owner::updatePipe(const playermodels::PipeParameters& globals, const actors::MoveActor& firstActor,
    const actors::MoveActor& secondActor, double scale, bool collisionEnabled, bool bodyConstraint,
    bool rigid, const playermodels::PathCollision& collision) {
    updateEndpoints(firstActor, secondActor);
    if ((firstActor.flags & 0xc000) || (secondActor.flags & 0xc000)) return;
    auto parameters = globals;
    parameters.fields[0] = &second->position;
    parameters.fields[2] = &second->field610;
    parameters.fields[1] = &first->field61C;
    parameters.fields[3] = &first->field628;
    parameters.field1C = collisionEnabled;
    auto frameDirection = second->field628;
    for (auto& component : frameDirection) component = -component;
    parameters.frameDirection = &frameDirection;
    parameters.startNormal = &second->field628;
    parameters.field4C = static_cast<float>(scale);
    parameters.field20 = static_cast<float>(static_cast<double>(parameters.field20) * scale);
    playermodels::PipeBoundary boundary;
    if (bodyConstraint && secondActor.model) {
        constexpr float angle = 0.5235987901687622f;
        const float sine = std::sin(angle), cosine = std::cos(angle);
        const actormodel::Matrix rotation{1, 0, 0, 0, 0, cosine, -sine, 0, 0, sine, cosine, 0};
        boundary.matrix = dooranimation::concatenate(
            dooranimation::concatenate(secondActor.matrix, secondActor.model->matrix(0, 1)), rotation);
        boundary.position = {0, 19, -25};
        boundary.signs = {-1, -1, 1};
        boundary.plane = 1;
        parameters.boundary = &boundary;
    }
    if (rigid) parameters.field24[0] = 0;
    pipe->updateMotion(parameters, collision);
    pipe->publishJoints(scale, frameDirection);
    pipe->startNormal = nullptr;
}
