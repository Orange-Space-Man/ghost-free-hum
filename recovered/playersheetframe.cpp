#include "playersheetframe.h"
#include "playersheettextures.h"

#include <cstring>
#include <stdexcept>

namespace {
    playersheets::Grid initializeGrid(unsigned capacity, const playersheets::Layout& layout) {
        playersheets::Grid grid(capacity);
        grid.initialize(layout);
        return grid;
    }

    class BoundServices final : public playersheets::SheetServices {
    public:
        BoundServices(playersheets::FrameServices& services, const playersheets::Layout& layout,
            playermodels::GridModels& models, const playermodels::SharedModels& shared) :
            services(services), layout(layout), models(models), shared(shared) {}
        void sound(unsigned id, const playerscanner::Vector& position) override { services.sound(id, position); }
        void completion(const playerevents::Payload& payload) override { services.completion(payload); }
        void completionEffect(unsigned channel, const playerscanner::Vector& position, const playerscanner::Vector& direction) override {
            services.completionEffect(channel, position, direction);
        }
        void impact(float radius, const playerscanner::Vector& position, unsigned count) override { services.impact(radius, position, count); }
        void secondaryTexture() override {
            auto replacement = layout;
            replacement.texture = layout.secondaryTexture;
            playersheets::bindTextures(replacement, shared, models);
        }
        void rustle(unsigned id, const playerscanner::Vector& position, float volume) override { services.rustle(id, position, volume); }
        bool requestPull(playersheets::Grid& sheet, const playerscanner::Vector& center, float range, float elasticity) override {
            return services.requestPull(sheet, center, range, elasticity);
        }
        bool elasticReaction(const playerscanner::Vector& center, unsigned reaction, unsigned action) override {
            return services.elasticReaction(center, reaction, action);
        }
        void releasePull(playersheets::Grid& sheet) override { services.releasePull(sheet); }

    private:
        playersheets::FrameServices& services;
        const playersheets::Layout& layout;
        playermodels::GridModels& models;
        const playermodels::SharedModels& shared;
    };
}

playersheets::Frame::Frame(const Layout& layout, playermodels::GridModels& models,
    const playermodels::SharedModels& shared, const std::vector<std::uint8_t>& executable) :
    grid(initializeGrid(models.count, layout)), motion(grid, executable), suction(grid, motion, executable), models(models), shared(shared) {
    bindTextures(layout, shared, models);
    auto& parameters = models.parameters;
    parameters.width = models.width = layout.width;
    parameters.height = models.height = layout.height;
    parameters.field8 = layout.corners[0]; parameters.field14 = layout.corners[1];
    parameters.field20 = layout.corners[2]; parameters.field2C = layout.corners[3];
    for (unsigned i = 0; i < 4; ++i) parameters.fields38[i] = static_cast<std::uint16_t>(layout.border[i]);
    parameters.fields40 = layout.drops;
    parameters.field58 = layout.response; parameters.field5C = layout.lifetime;
    parameters.fields60 = layout.offsets;
    parameters.field70 = layout.range; parameters.field74 = layout.elasticity;
    parameters.field78 = layout.mode; parameters.field7C = layout.reversePlanes;
    parameters.field7D = layout.anchored; parameters.fieldF0 = layout.splitRows;
    models.field148 = -1;
    models.suctionVolume = nullptr;
    models.suctionPosition = nullptr;
    publish();
}

void playersheets::Frame::publish() {
    for (unsigned i = 0; i < grid.points().size(); ++i) {
        const auto& source = grid.points()[i];
        auto& target = models.points[i];
        target.flags = {std::uint8_t(source.active), std::uint8_t(source.fixed), std::uint8_t(source.edge),
            std::uint8_t(source.attracted), std::uint8_t(source.touched)};
        target.field8 = static_cast<std::uint16_t>(source.links[0].x);
        target.fieldA = static_cast<std::uint16_t>(source.links[0].y);
        target.fieldC = source.links[0].length;
        target.fields10[0] = (std::uint32_t(static_cast<std::uint16_t>(source.links[1].x)) << 16) |
            static_cast<std::uint16_t>(source.links[1].y);
        std::memcpy(&target.fields10[1], &source.links[1].length, 4);
        target.linkCount = source.linkCount;
        target.field1C = source.anchor; target.field28 = source.weight;
        target.field2C = source.position; target.field38 = source.velocity; target.field44 = source.normal;
        target.field50 = source.texture[0]; target.field54 = source.texture[1];
    }
    models.fields118 = motion.drops;
    models.fields14C = {std::uint8_t(suction.releaseStarted), std::uint8_t(motion.halfway), std::uint8_t(motion.pulling)};
    models.field150 = motion.pullDirection[0]; models.field154 = motion.pullDirection[1];
    models.field158 = motion.retracting; models.field15C = suction.pullFrames;
    models.field160 = motion.detached; models.field161 = suction.completed;
    models.fields130 = motion.cropBounds; models.field164 = suction.captureCenter; models.field170 = suction.impactReady;
}

void playersheets::Frame::update(const MotionParameters& parameters, int player, bool collision, FrameServices& services) {
    if (suction.completed) return;
    const auto* owner = player < 0 ? nullptr : services.owner(player);
    if (player >= 0 && !owner) throw std::runtime_error("Original sheet update requires a valid player binding.");
    models.field148 = player;
    models.suctionVolume = owner ? owner->volume : nullptr;
    models.suctionPosition = owner ? &owner->position : nullptr;
    BoundServices bound(services, grid.layout(), models, shared);
    motion.retract(owner && owner->volume);
    motion.crop();
    for (unsigned i = 0; i < parameters.iterations; ++i) {
        motion.damp(parameters);
        motion.applyGravity(parameters);
        suction.attract(owner, bound);
        motion.constrainAnchors(parameters);
        motion.constrainNeighbors(parameters, owner ? &owner->position : nullptr);
        motion.integrate();
    }
    if (collision && !motion.detached) suction.collide(bound, bool(models.texture.secondary));
    motion.updateNormals();
    updateMaterial(grid, motion, models);
    suction.updateFeedback(player >= 0, owner && owner->volume, [&]() { return services.holding(player); },
        [&]() { return services.pulling(player); }, bound);
    updateModelTransform(grid, models, shared, [&](const auto& position) { return services.roomColor(position); });
    suction.updateSound(bound);
    if (motion.detached) {
        if (!owner) throw std::runtime_error("Detached sheet audio requires the original owner position.");
        const auto& layout = grid.layout();
        const auto& border = layout.border;
        const bool full = border[0] == 0 && border[1] == 0 && int(border[2]) == int(layout.width) - 1 &&
            int(border[3]) == int(layout.height) - 1;
        bound.sound(full ? 0x20 : 0x21, owner->position);
    }
    publish();
}

playersheets::Owner::Owner(playermodels::GridOwner& models, const playermodels::SharedModels& shared,
    roomfurniture::Owner& furniture, const std::vector<std::uint8_t>& executable) :
    models(models), shared(shared), furniture(furniture), executable(executable), builder(executable) {}

void playersheets::Owner::initialize(const playerscanner::SheetInput& input, bool reversePlanes) {
    auto& model = *models.grids.at(input.slot);
    const auto layout = builder.build(input, model.count, input.texture, 13, reversePlanes);
    auto frame = std::make_unique<Frame>(layout, model, shared, executable);
    rows.at(input.slot) = input.row;
    sheets.at(input.slot) = std::move(frame);
    models.fields16BC.at(input.slot) = 1;
}

void playersheets::Owner::update(FrameServices& services) {
    for (unsigned i = 0; i < sheets.size(); ++i) {
        if (!models.fields16BC[i]) continue;
        auto& frame = sheet(i);
        if (frame.suction.completed) {
            models.fields16BC[i] = 0;
            furniture.markSheetCollected(rows[i]);
        } else {
            MotionParameters parameters(executable);
            parameters.reciprocalConstraints = true;
            frame.update(parameters, 0, true, services);
        }
    }
}

playersheets::Frame& playersheets::Owner::sheet(unsigned slot) {
    if (!sheets.at(slot)) throw std::runtime_error("Original sheet owner slot has no initialized runtime.");
    return *sheets[slot];
}
