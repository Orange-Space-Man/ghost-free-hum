#include "game/actors/actortrailcontroller.h"

#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
}

int main() {
    try {
        const actortrail::Endpoints previous{{{0, 0, 0}, {0, 2, 0}}}, current{{{5, 0, 0}, {5, 2, 0}}};
        actortrail::Segment segment;
        require(!segment.create(previous, previous, 2, 0xff00c8ff, 30, 1, 0, 0.5f), "Stationary trails are rejected.");
        auto boundary = previous;
        boundary[0][0] = boundary[1][0] = 0.25f;
        require(!segment.create(previous, boundary, 2, 0xff00c8ff, 30, 1, 0, 0.5f), "The movement threshold is strict.");
        require(!segment.create(previous, current, 5, 0xff00c8ff, 30, 1, 0, 0.5f) &&
            !segment.create(previous, current, 2, 0xff00c8ff, 0, 1, 0, 0.5f), "Invalid types and zero lifetimes are rejected.");
        require(segment.create(previous, current, 2, 0xff00c8ff, 30, 1, 0, 0.5f), "The first moving segment is created.");
        require(segment.vertices[0] == previous[0] && segment.vertices[11] == current[1] &&
            segment.vertices[4] == actortrail::Vector{2, 0, 0} && segment.alphaStep == 8 &&
            segment.alphas[0] == 0 && segment.alphas[2] == 51 && segment.alphas[11] == 255,
            "The first strip spans six pairs with the original tapered alpha.");
        require(!segment.create(previous, current, 2, 0xff00c8ff, 30, 1, 0, 0.5f), "Occupied slots cannot be overwritten.");
        segment.update();
        require(segment.remaining == 30, "Undrawn strips do not age.");
        segment.flags |= 2;
        segment.update();
        require(segment.remaining == 29 && (segment.color & 255) == 247 && segment.alphas[11] == 247,
            "Drawn leading strips decrease their color alpha before rebuilding the gradient.");
        for (unsigned i = 0; i < 29; ++i) segment.update();
        require(segment.remaining == 0, "Drawn strips expire after their byte lifetime.");
        require(segment.create(previous, current, 2, 0xff00c8ff, 30, 0, 0.5f, 0.5f), "Expired slots are reusable.");
        require(segment.alphas[0] == 247 && segment.alphas[11] == 255, "Following strips taper by the integer alpha decrement.");
        segment.flags |= 2;
        segment.update();
        require(segment.vertices[0][1] == -1 && segment.vertices[1][1] == 3 && segment.alphas[0] == 239 &&
            (segment.color & 255) == 255, "Expansion separates both edges and following strips fade vertex alpha only.");
        const auto uv = actortrail::textureCoordinates();
        require(uv[0] == std::array<float, 2>{0, 0} && uv[7] == std::array<float, 2>{1, 0.6f} &&
            uv[11] == std::array<float, 2>{1, 1}, "Trail coordinates follow the original strip ordering.");
        actortrail::Pool pool(0), large(10);
        require(pool.entries().size() == 100 && large.entries().size() == 400, "Pool capacity follows the original mode.");
        actortrail::Controller controller;
        controller.initialize(nullptr, {0, 0}, previous, 2, 0xff00c8ff, 30, 0, 0);
        controller.update(&pool);
        require(controller.countdown == 1 && !pool.entries()[0].remaining && controller.previous == previous,
            "The initial update only primes endpoints.");
        controller.endpoints = current;
        controller.update(&pool);
        require(controller.countdown == 0 && pool.entries()[0].flags == 1, "The second update emits the leading segment.");
        controller.endpoints[0][0] += 5;
        controller.endpoints[1][0] += 5;
        controller.update(&pool);
        require(pool.entries()[1].remaining == 30 && !pool.entries()[1].flags, "Subsequent motion emits following segments.");
        for (unsigned i = 2; i < 100; ++i) require(pool.create(previous, current, 2, 0xff00c8ff, 30, 0, 0), "All free slots are usable.");
        require(!pool.create(previous, current, 2, 0xff00c8ff, 30, 0, 0), "Pool exhaustion declines emission.");
        controller.endpoints[0][0] += 5;
        controller.update(&pool);
        require(controller.previous == controller.endpoints, "Exhausted pools still advance controller endpoint history.");
        controller.reset();
        controller.update(nullptr);
        require(controller.countdown == 1, "An absent global pool still advances warmup.");
        controller.interval = 254;
        controller.reset();
        require(controller.countdown == 0, "Warmup retains byte overflow.");
        std::cout << "Actor trail checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
