#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <functional>
#include <vector>
#include "game/actors/actorcollisionowner.h"

namespace roomlighting { class Pool; }
namespace actormodel { class InitialPose; }
namespace mapcollision { struct SphereContact; }

namespace playerresources {
    struct PointRecord {
        std::array<float, 3> position{}, velocity{};
        float lifetime = 0;
    };

    struct PointSettings {
        static PointSettings read(const std::vector<std::uint8_t>& executable);
        float gravity = 0, damping = 0, bounce = 0;
        std::int32_t lifetime = 0;
        bool collision = false;
    };

    struct PointLine {
        std::array<float, 3> start{}, end{};
        std::array<std::uint8_t, 4> color{};
        unsigned width = 12;
    };

    class PointPool {
    public:
        explicit PointPool(unsigned count);
        void emit(unsigned requested, float speed, const std::array<float, 3>& position,
            const std::array<float, 3>* velocity, const PointSettings& settings,
            const actormodel::InitialPose& pose, const std::function<std::uint32_t()>& random);
        void update(const PointSettings& settings,
            const std::function<mapcollision::SphereContact(const std::array<float, 3>&,
                const std::array<float, 3>&)>& collision);
        void draw(const PointSettings& settings, const std::function<void(const PointLine&)>& line) const;
        unsigned count;
        std::unique_ptr<PointRecord[]> records;
        PointRecord* last;
    };

    struct EffectRecord80 {
        EffectRecord80();
        std::array<std::uint8_t, 0x7c> fields;
        float field7C;
    };

    struct EffectRecord20 {
        EffectRecord20();
        std::array<std::uint8_t, 0x18> fields;
        float field18;
        std::array<std::uint8_t, 4> field1C;
    };

    class EffectPool80 {
    public:
        explicit EffectPool80(unsigned count);
        unsigned count;
        std::unique_ptr<EffectRecord80[]> records;
    };

    class EffectPool20 {
    public:
        explicit EffectPool20(unsigned count);
        unsigned count;
        std::unique_ptr<EffectRecord20[]> records;
    };

    class RecordPool44 {
    public:
        struct Record {
            actorcollision::Volume volume;
            std::array<float, 3> position;
        };
        explicit RecordPool44(unsigned count);
        bool append(const actorcollision::Volume& volume, const std::array<float, 3>& position);
        bool contact(std::array<float, 3>& displacement, const actorcollision::Volume& volume,
            const std::array<float, 3>& position) const;
        void finishContacts();
        unsigned count;
        std::unique_ptr<Record[]> records;
        std::uint32_t field8 = 0, fieldC = 0;
    };

    class CurvePoints {
    public:
        explicit CurvePoints(unsigned count);
        std::vector<std::array<float, 3>> points;
    };

    class CurveOwner {
    public:
        CurveOwner();
        ~CurveOwner();
        std::unique_ptr<CurvePoints> primary;
        std::array<std::unique_ptr<CurvePoints>, 3> secondary;
        std::uint8_t field10 = 0;
        void* field14 = nullptr;
        std::array<std::uint8_t, 0x30> fields;
        float field48 = 0;
    };

    struct LightPulse {
        void start(const std::array<float, 3>& origin, std::uint32_t rgba, std::uint16_t range,
            std::uint16_t intensity, std::uint32_t frames);
        void update(roomlighting::Pool& lights);
        std::array<float, 3> position{};
        std::uint32_t color = 0;
        std::uint16_t distance = 0, brightness = 0;
        float remaining = 0, duration = 0;
    };
}
