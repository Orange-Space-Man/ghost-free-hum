#pragma once

#include "game/actors/moveactor.h"
#include "game/scene/gamearchive.h"

#include <functional>
#include <optional>

namespace actortrail {
    using Vector = std::array<float, 3>;
    using Endpoints = std::array<Vector, 2>;
    std::array<std::array<float, 2>, 12> textureCoordinates();

    struct Segment {
        std::uint8_t flags = 0, remaining = 0, type = 0, alphaStep = 0;
        std::uint32_t color = 0;
        float spread = 0;
        std::array<Vector, 12> vertices{};
        std::array<std::uint8_t, 12> alphas{};
        bool create(const Endpoints& previous, const Endpoints& current, std::uint8_t kind,
            std::uint32_t rgba, std::uint8_t life, std::uint8_t countdown, float expansion, float threshold);
        void update();
    };

    class Pool {
    public:
        explicit Pool(unsigned mode);
        void load(const gamearchive::Archive& archive);
        bool create(const Endpoints& previous, const Endpoints& current, std::uint8_t type,
            std::uint32_t color, std::uint8_t life, std::uint8_t countdown, float spread);
        void update();
        void draw(const std::function<void(const Segment&, const std::vector<std::uint8_t>&)>& submit);
        const std::vector<Segment>& entries() const;
        float movementThreshold = 0.5f;

    private:
        std::vector<Segment> segments;
        std::array<std::vector<std::uint8_t>, 5> textures;
    };

    class Controller {
    public:
        void initialize(actors::MoveActor* object, const std::array<std::int32_t, 2>& indices,
            const Endpoints& points, std::uint8_t kind, std::uint32_t rgba, std::uint8_t lifetime,
            std::uint8_t delay, float expansion);
        void reset();
        void update(Pool* pool);
        std::uint8_t interval = 0, countdown = 0, life = 0, type = 0;
        std::uint32_t color = 0;
        float spread = 0;
        std::array<std::int32_t, 2> joints{};
        Endpoints endpoints{};
        actors::MoveActor* actor = nullptr;
        std::optional<Endpoints> previous;
    };
}
