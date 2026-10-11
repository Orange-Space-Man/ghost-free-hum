#pragma once

#include "game/player/playersharedmodels.h"

namespace playermodels {
    class VacuumVolume {
    public:
        virtual ~VacuumVolume() = default;
        virtual bool intersects(double radius, std::array<float, 3>& direction, const std::array<float, 3>& position) const = 0;
    };

    struct VacuumPoint {
        std::array<float, 3> position{}, previous{};
        std::uint32_t field18 = 0;
    };

    struct VacuumSegment {
        std::array<float, 3> position{};
        float fieldC = 0, field10 = 0, field14 = 1, field18 = 0, field1C = 0;
    };

    struct PathParameters {
        std::array<const std::array<float, 3>*, 5> fields{};
        std::uint32_t field14 = 1;
        float field18;
        std::uint8_t field1C = 0, field1D = 0;
        float field20;
        std::array<float, 3> field24;
        float field30, field34, field38 = 1;
        PathParameters();
        void reset();
    };

    struct VacuumParameters : PathParameters {
        std::uint32_t field3C = 0;
        std::uint16_t field40 = 0;
        std::uint32_t field44 = 0;
        float field48 = 0;
        std::uint8_t field4C = 0;
        float field50 = 1, field54 = 1, field58 = 1, field5C = 0, field60 = 0;
        std::uint8_t field64 = 255;
        VacuumParameters();
        void reset();
    };

    class VacuumPath {
    public:
        explicit VacuumPath(unsigned count);
        virtual ~VacuumPath();
        VacuumPath(const VacuumPath&) = delete;
        VacuumPath& operator=(const VacuumPath&) = delete;
        void initialize(float blend, const PathParameters& parameters, const PathParameters* target);
        unsigned count;
        float interval;
        std::unique_ptr<VacuumPoint[]> points;
        std::unique_ptr<std::array<float, 3>[]> vectors;
    };

    class VacuumModel : public VacuumPath, public VacuumVolume {
    public:
        VacuumModel(const SharedModels& shared, unsigned count, unsigned kind);
        ~VacuumModel() override;
        VacuumModel(const VacuumModel&) = delete;
        VacuumModel& operator=(const VacuumModel&) = delete;
        void updateSegments();
        bool intersects(double radius, std::array<float, 3>& direction, const std::array<float, 3>& position) const override;
        bool intersectsSegment(double radius, std::array<float, 3>& direction,
            const std::array<float, 3>& position, unsigned index) const;
        VacuumParameters parameters;
        std::unique_ptr<VacuumSegment[]> segments;
        std::shared_ptr<j3dmodel::ModelData> data;
        std::shared_ptr<j3dmodel::Animation> textureAnimation, colorAnimation;
        std::unique_ptr<j3dmodel::ModelInstance> instance;
        std::uint16_t joint = 0;
        float field9C = 0, fieldA0 = 0, fieldA4 = 1, fieldA8 = 0, fieldAC = 0;
        float fieldB0 = 0, fieldB4 = 1, fieldB8 = 0, fieldBC = 0;
    };
}
