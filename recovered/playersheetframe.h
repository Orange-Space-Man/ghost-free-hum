#pragma once

#include "playersheetmaterial.h"
#include "playersheetsuction.h"

namespace playersheets {
    class FrameServices : public SheetServices {
    public:
        virtual const SuctionOwner* owner(int player) = 0;
        virtual bool holding(int player) = 0;
        virtual bool pulling(int player) = 0;
        virtual std::uint32_t roomColor(const playerscanner::Vector& position) = 0;
    };

    class Frame {
    public:
        Frame(const Layout& layout, playermodels::GridModels& models, const playermodels::SharedModels& shared,
            const std::vector<std::uint8_t>& executable);
        Frame(const Frame&) = delete;
        Frame& operator=(const Frame&) = delete;
        void update(const MotionParameters& parameters, int player, bool collision, FrameServices& services);
        void publish();
        Grid grid;
        Motion motion;
        Suction suction;

    private:
        playermodels::GridModels& models;
        const playermodels::SharedModels& shared;
    };

    class Owner {
    public:
        Owner(playermodels::GridOwner& models, const playermodels::SharedModels& shared,
            roomfurniture::Owner& furniture, const std::vector<std::uint8_t>& executable);
        void initialize(const playerscanner::SheetInput& input, bool reversePlanes);
        void update(FrameServices& services);
        Frame& sheet(unsigned slot);

    private:
        playermodels::GridOwner& models;
        const playermodels::SharedModels& shared;
        roomfurniture::Owner& furniture;
        const std::vector<std::uint8_t>& executable;
        Builder builder;
        std::array<std::unique_ptr<Frame>, 4> sheets;
        std::array<unsigned, 4> rows{};
    };
}
