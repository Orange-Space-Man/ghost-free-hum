#pragma once

#include "playerfurniturescancache.h"
#include "game/player/playergridmodels.h"

namespace playersheets {
    struct Layout {
        unsigned width = 16, height = 16;
        std::array<playerscanner::Vector, 4> corners{};
        std::array<std::int16_t, 4> border{4, 4, 11, 11};
        std::array<float, 4> drops{80, 80, 80, 80}, offsets{10, 10, 10, 10};
        unsigned mode = 0, texture = 0, secondaryTexture = 0, lifetime = 45;
        float response = 30, range = 80, elasticity = 0;
        bool reversePlanes = false, anchored = true, splitRows = false;
    };

    class Builder {
    public:
        explicit Builder(const std::vector<std::uint8_t>& executable);
        Layout build(const playerscanner::SheetInput& input, unsigned capacity, unsigned texture,
            unsigned secondaryTexture, bool reversePlanes) const;

    private:
        float diagonalScale, elasticRange, minimumRange, aspectThreshold;
    };

    struct Link {
        std::int16_t x = 0, y = 0;
        float length = 0;
    };

    struct Point {
        bool active = false, fixed = false, edge = false, attracted = false, touched = false;
        std::array<Link, 2> links{};
        unsigned linkCount = 0;
        playerscanner::Vector anchor{}, position{}, velocity{};
        playerscanner::Vector normal{0, 1, 0};
        float weight = 0;
        std::array<float, 2> texture{};
    };

    struct Plane {
        playerscanner::Vector normal{};
        float distance = 0;
    };

    class Grid {
    public:
        explicit Grid(unsigned capacity);
        void initialize(const Layout& layout);
        const Layout& layout() const;
        const std::vector<Point>& points() const;
        std::vector<Point>& points();
        const std::array<Plane, 5>& planes() const;
        const actormodel::Matrix& matrix() const;

    private:
        unsigned capacity;
        Layout parameters;
        std::vector<Point> vertices;
        std::array<Plane, 5> boundaries{};
        actormodel::Matrix basis{};
    };
}
