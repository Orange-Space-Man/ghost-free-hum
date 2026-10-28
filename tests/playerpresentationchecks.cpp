#include "../recovered/playeritempresentation.h"
#include "game/scene/gameplayresourceowner.h"
#include "resources/dolresources.h"

#include <cstring>
#include <stdexcept>
#include <string>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    float scalar(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> data;
        std::string error;
        if (!dolresources::read(executable, address, 4, data, error)) throw std::runtime_error(error);
        const std::uint32_t bits = (unsigned(data[0]) << 24) | (unsigned(data[1]) << 16) | (unsigned(data[2]) << 8) | data[3];
        float result;
        std::memcpy(&result, &bits, 4);
        return result;
    }

    class Services final : public playerpresentation::Services {
    public:
        void effect(unsigned effect, const std::array<float, 3>&) override { effects.push_back(effect); events += 'e'; }
        bool specialItem(unsigned) const override { return special; }
        void music(unsigned sequence) override { musicSequence = sequence; events += 'm'; }
        void collected(actors::MoveActor&) override { events += 'c'; }
        void setFlag(unsigned) override { events += 'f'; }
        bool equipmentOwned() const override { return false; }
        void equipmentEvent(unsigned, unsigned, unsigned) override { events += 'q'; }
        void sound(unsigned, const std::array<float, 3>&) override { events += 's'; }
        void scene(unsigned scene, std::int32_t argument) override {
            require(scene == 1 && argument == 19, "Presentation requests original scene and retained argument."); events += 'r';
        }
        std::string events;
        std::vector<unsigned> effects;
        unsigned musicSequence = 0;
        bool special = false;
    };
}

void checkItemPresentation(gameplay::ResourceOwner& owner) {
    auto& player = *owner.managers().playerEvents().player(0);
    const auto savedState = player.state;
    const auto savedPresentation = *player.components->state80;
    const auto savedActions = *player.components->actions;
    const auto savedFlags = player.flags;
    const auto savedFrame = player.animationFrame;
    const auto savedType = player.actorType;
    auto& actions = *player.components->actions;
    auto& presentation = *player.components->state80;
    playerpresentation::Frame frame(owner.executable());
    Services services;
    actions.field310 = 1;
    player.state.field1060 = 0;
    player.state.field1070 = 123;
    require(!frame.run(player, services) && player.state.field1070 == 123, "Inactive presentation leaves orientation untouched.");
    player.state.field1060 = 1;
    player.state.field1064 = nullptr;
    require(frame.run(player, services) && !presentation.field10 && !presentation.field11 && player.state.field1070 == 7,
        "Presentation admission clears its one-shot gates and publishes orientation mode.");
    actions.field310 = 2; actions.field314 = 0x22; actions.fields31C[3] = 2;
    player.animationFrame = scalar(owner.executable(), 0x804da9d8);
    require(frame.run(player, services) && presentation.field10 && presentation.field11 && presentation.field7C == 1 && services.events.empty(),
        "Null-item presentation still advances both original gates without effects or audio.");
    player.state.field1064 = &player;
    for (unsigned type = 0x57; type <= 0x5a; ++type) {
        player.actorType = type;
        presentation.field11 = 0;
        services = Services{};
        services.special = true;
        frame.run(player, services);
        const unsigned first = 0x3e + (type - 0x57) * 2;
        require(services.effects == std::vector<unsigned>{first, first + 1} && services.events == "eem" &&
            services.musicSequence == 0x80000035 && presentation.field7C == 2,
            "Special-item effect pair precedes music and sets two completion events.");
        frame.run(player, services);
        require(services.events == "eem", "Presentation effect gate prevents repeated sound and particles.");
    }
    player.actorType = 0xcc;
    presentation.field11 = 0;
    services = Services{};
    frame.run(player, services);
    require(services.effects == std::vector<unsigned>{0x59, 0x5a, 0x5b} && presentation.field7C == 3 &&
        services.musicSequence == 0x8000000e, "Equipment effects retain the separate original music-classification gate.");
    player.actorType = savedType;
    player.state.field1064 = nullptr;
    services = Services{};
    actions.fields31C[3] = 3;
    presentation.field7C = 2;
    player.flags &= ~0x20000u;
    frame.run(player, services);
    require(presentation.field7C == 2, "Presentation waits for the animation event before decrementing.");
    player.flags |= 0x20000;
    frame.run(player, services);
    require(presentation.field7C == 1, "Presentation consumes one animation event per update.");
    actions.field310 = 2; actions.field314 = 0x22; actions.fields31C[3] = 4;
    player.animationFrame = scalar(owner.executable(), 0x804da9dc);
    player.flags |= 0x40000;
    player.state.field1061 = 1; player.state.field1068 = 19;
    require(frame.run(player, services) && !presentation.field10 && !player.state.field1060 &&
        !player.state.field1061 && player.state.field1068 == -1 && services.events == "r",
        "Release boundary ends presentation and consumes the requested scene transition.");
    for (unsigned i = 0; i < player.children.size(); ++i) {
        if (!player.children[i].actor) continue;
        const auto saved = player.children[i];
        const actormodel::Matrix matrix{2, 0, 0, 7, 0, 3, 0, 8, 0, 0, 4, 9};
        require(player.updateChildAttachment(*saved.actor, 3, &matrix, 0) && player.children[i].transform == matrix &&
            player.children[i].joint == 3 && !player.children[i].orientation && saved.actor->parent == &player,
            "Attachment update changes transform without recreating ownership.");
        require(player.updateChildAttachment(*saved.actor, 4, nullptr, 1) && !player.children[i].transformed &&
            player.children[i].transform == matrix, "Null attachment transform clears its gate and preserves stored matrix.");
        require(!saved.actor->updateChildAttachment(player, 0, nullptr, 0), "Attached actors cannot update child attachments.");
        player.children[i] = saved;
        break;
    }
    player.state = savedState;
    *player.components->state80 = savedPresentation;
    actions = savedActions;
    player.flags = savedFlags;
    player.animationFrame = savedFrame;
}
