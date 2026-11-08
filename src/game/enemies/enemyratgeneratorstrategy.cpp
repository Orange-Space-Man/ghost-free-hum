#include "game/enemies/enemyratgeneratorstrategy.h"
#include "game/actors/moveactor.h"

#include <cmath>
#include <stdexcept>

enemystrategy::RatGenerator::RatGenerator(enemies::Runtime& runtime, const actormodel::InitialPose& pose) :
    runtime(runtime), pose(pose) {}

charactergenerator::Generator& enemystrategy::RatGenerator::generator() const {
    if (!context) throw std::runtime_error("Original rat strategy has no generator owner.");
    return *static_cast<charactergenerator::Generator*>(context);
}

void enemystrategy::RatGenerator::initializeBehavior() {
    if (state == 2) {
        const float random = static_cast<float>(runtime.random()) / 4294967296.0f;
        const auto count = static_cast<std::int32_t>(static_cast<std::uint32_t>(generator().parameter("appear_time")) + 1u);
        appearanceFrame = static_cast<int>(static_cast<double>(static_cast<float>(count)) * random);
    }
}

void enemystrategy::RatGenerator::behavior() {
    auto& parent = generator();
    if (state == 0) {
        auto& path = parent.path();
        if (!path.attached()) throw std::runtime_error("Original rat generator requires its hole path.");
        pointCount = static_cast<int>(path.count());
        pairCount = static_cast<int>(0.5f * static_cast<float>(pointCount));
        setNextState(1);
    } else if (state == 1) {
        recycle();
        if (frames > 10) setNextState(2);
    } else if (state == 2) {
        recycle();
        if (static_cast<std::uint32_t>(parent.parameter("appear_time")) < frames) {
            setNextState(1);
        } else if (static_cast<std::uint32_t>(appearanceFrame) == frames && parent.source.table && parent.source.row >= 0) {
            auto* child = parent.create(parent.type, -1);
            if (!child || !place(*child)) return;
            auto& actor = dynamic_cast<actors::MoveActor&>(child->actor());
            actor.fieldE0 &= 0xffffff00;
            actor.fieldE4 &= 0xffffff00;
            actor.setVelocity({});
            actor.flags |= 0xc000;
        }
    }
}

void enemystrategy::RatGenerator::release(characterowner::Owner* child) {
    for (auto& entry : holes) {
        if (entry.child == child) { entry.child = nullptr; return; }
    }
}

void enemystrategy::RatGenerator::receive(charactergenerator::Event& event) {
    if (event.message == 0) release(event.child);
}

void enemystrategy::RatGenerator::recycle() {
    auto& parent = generator();
    for (int i = 0; i < parent.count; ++i) {
        auto& child = *parent.children.at(i);
        auto& strategy = *child.ownership().strategy;
        if (strategy.state == 2) {
            release(&child);
            strategy.setNextState(0);
            place(child);
        }
    }
}

std::array<float, 3> enemystrategy::RatGenerator::point(int index) {
    std::array<float, 3> result;
    const char* names[]{"pnt0_x", "pnt0_y", "pnt0_z"};
    for (unsigned axis = 0; axis < 3; ++axis) {
        if (!generator().path().getValue(index, names[axis], result[axis]))
            throw std::runtime_error("Original rat hole coordinate is missing.");
    }
    return result;
}

bool enemystrategy::RatGenerator::place(characterowner::Owner& child) {
    std::array<int, 16> available;
    for (int i = 0; i < 16; ++i) available[i] = i;
    int occupied = 0;
    for (const auto& entry : holes) {
        if (!entry.child) continue;
        available.at(entry.start) = available.at(entry.end) = -1;
        ++occupied;
    }
    std::array<int, 16> free;
    unsigned count = 0;
    for (const auto index : available) if (index != -1) free[count++] = index;
    const float remaining = static_cast<float>(pairCount - occupied);
    const float first = static_cast<float>(runtime.random()) / 4294967296.0f;
    int start = free.at(static_cast<int>(remaining * first));
    const float second = static_cast<float>(runtime.random()) / 4294967296.0f;
    int end = free.at(static_cast<int>(remaining * (1.0f + second)));
    if (static_cast<float>(runtime.random()) / 4294967296.0f <= 0.5f) std::swap(start, end);
    const auto player = runtime.playerPosition(0);
    const auto candidate = point(start);
    const float x = candidate[0] - player[0], z = candidate[2] - player[2];
    if (!(200.0f < std::sqrt(x * x + z * z))) std::swap(start, end);
    for (auto& entry : holes) {
        if (!entry.child) { entry = {&child, start, end}; break; }
    }
    auto* spawn = dynamic_cast<RatSpawn*>(child.ownership().strategy.get());
    if (!spawn) return false;
    const auto origin = point(start), target = point(end);
    spawn->setRatPath(origin, target);
    auto& actor = dynamic_cast<actors::MoveActor&>(child.actor());
    actor.setPosition(origin);
    actor.rotation = {0, static_cast<std::int16_t>(pose.angle(target[2] - origin[2], target[0] - origin[0])), 0};
    actor.previousRotation = actor.rotation;
    return true;
}
