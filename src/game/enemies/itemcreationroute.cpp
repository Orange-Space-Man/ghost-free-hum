#include "game/enemies/itemcreationroute.h"
#include "game/actors/characterowner.h"

#include <cmath>
#include <stdexcept>

namespace {
    using Vector = itemsuction::Vector;

    Vector normalized(Vector value) {
        const float squared = (value[2] * value[2] + value[0] * value[0]) + value[1] * value[1];
        if (!(squared > 0.000003814697265625f)) return {};
        float inverse = static_cast<float>(1.0 / std::sqrt(static_cast<double>(squared)));
        for (unsigned i = 0; i < 3; ++i) inverse = (0.5f * inverse) * (3.0f - squared * (inverse * inverse));
        for (auto& component : value) component *= inverse;
        return value;
    }

    Vector launch(Vector value) {
        value[0] = (value[0] + 0.8f * (0 - value[0])) * 15;
        value[1] = (value[1] + 0.8f * (1 - value[1])) * 15;
        value[2] = (value[2] + 0.8f * (0 - value[2])) * 15;
        return value;
    }

    Vector rotate(const Vector& value, float angle) {
        const float sine = std::sin(angle), cosine = std::cos(angle);
        const float x = cosine * value[0] + (sine * value[2] - 0.0f * value[1]);
        const float y = (cosine * value[1] + -0.0f * value[2]) + 0.0f * value[0];
        const float z = cosine * value[2] + (0.0f * value[1] - sine * value[0]);
        const float w = (-0.0f * value[0] - sine * value[1]) - 0.0f * value[2];
        const float first = y * -0.0f, second = w * -0.0f;
        return {second + ((x * cosine + first) - z * -sine),
            ((w * -sine + z * -0.0f) + x * 0.0f) + y * cosine,
            (second + z * cosine) + (x * -sine - first)};
    }
}

characteritems::Creation::Creation(const mission::Resources& mission, const enemytypes::Manager& parameters,
    const Admission& admission, enemies::Manager& manager, enemies::Runtime& runtime, const actors::Registry& actors,
    const roomlocation::Rooms& rooms, enemystrategy::ItemSounds& sounds) : mission(mission), parameters(parameters),
    admission(admission), manager(manager), runtime(runtime), actors(actors), rooms(rooms), sounds(sounds) {}

const playeractor::Player& characteritems::Creation::player() const {
    const auto* actor = actors.actor(manager.appearPoint(0).actor);
    if (!actor) throw std::runtime_error("Original item creation requires player zero.");
    return dynamic_cast<const playeractor::Player&>(*actor);
}

itemsuction::Vector characteritems::Creation::center(const itemsuction::Vector& position) const {
    const auto room = rooms.resolve(position) >> 24;
    const auto& bounds = rooms.entries().at(room).bounds;
    Vector result;
    for (unsigned i = 0; i < 3; ++i) result[i] = (static_cast<float>(bounds[i + 3]) + static_cast<float>(bounds[i])) * 0.5f;
    return result;
}

unsigned characteritems::Creation::create(unsigned type, const Vector& position, const Vector* velocity) {
    const auto id = manager.createActor(type, -1);
    if (static_cast<std::int32_t>(id) < 0) return 0xffffffff;
    if (admission.key(type)) sounds.play(0x1875, position, 0);
    auto* actor = actors.actor(id);
    if (!actor) throw std::runtime_error("Original item manager returned an absent actor.");
    actor->setPosition(position);
    actor->visibility(255, 255);
    if (velocity) actor->setVelocity(*velocity);
    else {
        auto direction = center(position);
        for (unsigned i = 0; i < 3; ++i) direction[i] -= position[i];
        direction = normalized(direction);
        const float random = static_cast<float>(runtime.random() % 10000) / 9999.0f;
        actor->setVelocity(launch(rotate(direction, 0.5f * (random - 0.5f))));
    }
    return id;
}

unsigned characteritems::Creation::create(const jmp::ToolDataRef& source, const Vector& position, const Vector* velocity) {
    if (admission.nothing(source) || admission.money(source) || admission.blocked(source, player().view)) return 0xffffffff;
    const auto type = admission.type(source);
    unsigned id;
    if (admission.redirected(source)) {
        auto direction = center(position);
        for (unsigned i = 0; i < 3; ++i) direction[i] -= position[i];
        const auto redirected = launch(normalized(direction));
        id = create(type, position, &redirected);
    } else id = create(type, position, velocity);
    if (static_cast<std::int32_t>(id) >= 0) {
        auto* actor = actors.actor(id);
        auto* owner = actor ? manager.activeOwner(*actor) : nullptr;
        auto* character = dynamic_cast<characterowner::Owner*>(owner);
        if (character) if (auto* item = dynamic_cast<enemystrategy::FiniteItem*>(character->ownership().strategy.get())) item->itemSource = source;
    }
    return id;
}

unsigned characteritems::Creation::drop(characterowner::Owner& owner) {
    const auto table = tableId(owner.source, owner.actor().type(), parameters);
    const auto* capture = owner.state().captureRecord;
    if (!capture) throw std::runtime_error("Original capture drop requires its owner capture record.");
    const auto source = (*capture)[3] >= 2 && table == 3 ? find(mission, "sheart") :
        select(mission, static_cast<std::int32_t>(table), static_cast<std::int16_t>(player().view.life), runtime);
    return create(source, dynamic_cast<actors::MoveActor&>(owner.actor()).position);
}
