#pragma once

#include "game/actors/actorinitialpose.h"

#include <functional>

namespace playermodels { class VacuumPath; }
namespace actors { class MoveActor; }

namespace actorcapture {
    using Vector = std::array<float, 3>;
    bool targetDirection(const Vector& target, const Vector& axis, const Vector& position, Vector& output);
    bool invert(const actormodel::Matrix& matrix, actormodel::Matrix& output);

    class State {
    public:
        explicit State(const std::array<std::uint8_t, 0x430>& retained);
        std::uint8_t byte(unsigned offset) const;
        std::uint16_t half(unsigned offset) const;
        std::uint32_t word(unsigned offset) const;
        float number(unsigned offset) const;
        Vector vector(unsigned offset) const;
        actormodel::Matrix matrix(unsigned offset) const;
        void byte(unsigned offset, std::uint8_t value);
        void half(unsigned offset, std::uint16_t value);
        void word(unsigned offset, std::uint32_t value);
        void number(unsigned offset, float value);
        void vector(unsigned offset, const Vector& value);
        void matrix(unsigned offset, const actormodel::Matrix& value);
        std::array<std::uint8_t, 0x430> bytes;
    };

    class Motion {
    public:
        Motion(const std::vector<std::uint8_t>& executable, const actormodel::InitialPose& pose,
            std::function<std::uint32_t()> random);
        void initialize(State& state, const Vector& target, const Vector& position, std::uint16_t type);
        void endpoints(State& state, const Vector& position, const Vector& target);
        void specialEndpoints(State& state, const Vector& target, const Vector& position);
        void advanceOrdinary(State& state, const Vector& target, const Vector& position,
            const Vector* segmentDirection, const Vector* suctionDirection);
        void advanceSpecial(State& state, bool alternate);
        void advance(State& state, const Vector& target, const Vector& position, const Vector& scale,
            const actormodel::Matrix& matrix, const playermodels::VacuumPath* path,
            const Vector* suctionDirection, const Vector* originalScratch = nullptr);
        static float scale(const State& state);
        static void follow(State& state, const Vector& axis);
        void jointParameters(State& state, const Vector& position, bool vertical = false) const;
        actormodel::Matrix basicJointTransform(State& state, const Vector& position) const;
        bool ordinaryJointTransform(State& state, const Vector& position, actormodel::Matrix& output,
            const float* originalRegister = nullptr) const;
        bool jointTransform(State& state, double amount, const Vector& position, actormodel::Matrix& output,
            const float* originalRegister = nullptr) const;
        bool deform(State& state, double amount, const Vector& position, actormodel::Matrix& joint,
            actormodel::Matrix& retainedTransform, const float* originalRegister = nullptr) const;
        Vector field8B28{}, field8B34{};

    private:
        const std::vector<std::uint8_t>& executable;
        const actormodel::InitialPose& pose;
        std::function<std::uint32_t()> random;
        float sample();
        void copy(State& state, unsigned offset, unsigned address, unsigned size);
        void parameters(State& state);
        void ordinary(State& state);
        void special(State& state, bool alternate);
        void segments(State& state);
    };

    void initializeActor(Motion& motion, State& state, actors::MoveActor& actor, const Vector& target, std::uint16_t type);
    void updateActor(Motion& motion, State& state, actors::MoveActor& actor, const playermodels::VacuumPath* path,
        const Vector* suctionDirection, actormodel::Matrix& retainedTransform,
        const Vector* originalScratch = nullptr, const float* originalRegister = nullptr,
        const bool* emptyCompletion = nullptr);
}
