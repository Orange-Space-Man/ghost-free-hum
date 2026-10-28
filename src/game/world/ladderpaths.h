#pragma once

#include "game/events/missionresources.h"
#include "game/actors/actorinitialpose.h"

namespace ladders {
    using Vector = std::array<float, 3>;

    class Paths {
    public:
        explicit Paths(const mission::Resources& mission);
        unsigned count() const;
        int entryCount(unsigned ladder) const;
        bool position(unsigned ladder, int index, Vector& output) const;
        bool center(unsigned ladder, int entry, Vector& output) const;
        bool normal(unsigned ladder, int entry, Vector& output) const;
        bool tangent(unsigned ladder, int entry, Vector& output) const;
        bool segmentLength(unsigned ladder, int entry, float& output) const;
        float length(unsigned ladder) const;
        bool endpoint(unsigned ladder, bool end, Vector& position, Vector& normal) const;
        bool sample(unsigned ladder, double distance, actormodel::Matrix& output) const;
        bool sampleSmoothed(unsigned ladder, double distance, double radius, actormodel::Matrix& output) const;

    private:
        const jmp::ToolData* path(unsigned ladder) const;
        const mission::Resources& mission;
    };
}
