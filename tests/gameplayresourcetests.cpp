#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/scene/gameplayresourceowner.h"
#include "game/startup/startupresources.h"
#include "game/player/playeractioncontext.h"
#include "game/player/playerinputrouting.h"
#include "game/player/playervelocitycontext.h"
#include "game/player/playerorientationframe.h"
#include "game/player/playervelocityframe.h"
#include "game/player/playerdoorinteraction.h"
#include "game/player/playerdoorframe.h"
#include "resources/roomarchivereader.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const std::string& message) {
        if (!condition) throw std::runtime_error(message);
    }

    void checkDoorInteraction(gameplay::ResourceOwner& owner) {
        const auto& executable = owner.executable();
        auto& manager = owner.managers();
        auto& doors = manager.doors();
        std::string error;
        actormodel::Catalog catalog;
        require(catalog.read(executable, error), error);
        actormodel::Resources models(catalog);
        std::vector<std::uint8_t> compressed, archiveBytes;
        gamearchive::Archive archive;
        require(owner.package().read("files/Game/game_usa.szp", compressed, error) &&
            roomarchive::decompress(compressed, archiveBytes, error) && archive.read(archiveBytes, error), error);
        models.initializeEmbedded(archive);
        std::array<std::uint32_t, 256> rooms{};
        for (unsigned i = 0; i < owner.rooms().entries().size(); ++i) rooms[i] = owner.rooms().entries()[i].id;
        std::array<std::uint8_t, 32> keys{};
        doors::SavedFlags saved{};
        playerinteraction::DoorServices services{doors, saved, keys, rooms, manager.enemies(), catalog, models};
        playerinteraction::DoorInteraction interaction(executable);
        playercomponents::DoorState state;
        playeractions::Actions actions(executable, 0);
        playerinteraction::DoorInput input;
        bool found = false;
        for (unsigned i = 0; i < doors.size(); ++i) {
            const auto& leaf = doors.leaf(i);
            if (!leaf.collision || !leaf.collisionActive) continue;
            input.position = leaf.collision->center;
            for (unsigned axis = 0; axis < 3; ++axis) for (unsigned component = 0; component < 3; ++component)
                input.position[component] += leaf.collision->axes[axis][component] * (leaf.collision->extents[axis] * 0.5f);
            input.room = rooms[leaf.rooms[0]];
            input.radius = 0;
            if (interaction.select(state, input, services)) { found = true; break; }
        }
        require(found && state.door >= 0, "Original selection finds a real map-two door before fake actors.");
        const auto selected = state.door;
        const auto originalFlags = doors.leaf(selected).flags;
        const auto savedId = doors.leaf(selected).savedId;
        input.direction = state.facing;
        for (auto& value : input.direction) value = -value;
        require(interaction.select(state, input, services) && state.door == selected,
            "Original minus-one facing threshold accepts a direction facing directly away.");
        playerinteraction::DoorOutput output;
        output.damage = 99;
        require(!interaction.run(state, actions, input, output, services) && output.damage == 99,
            "An unpressed interaction leaves the caller output untouched.");
        input.eligible = input.pressed = input.primary = true;
        input.speed = 2;
        input.reaction = 1;
        require(interaction.run(state, actions, input, output, services) && actions.fields31C[0] == 1,
            "Selection enters the real protected door action through its payload.");
        input.position = state.approachPosition;
        input.contact = true;
        doors.setEventBlocked(savedId, true);
        require(interaction.run(state, actions, input, output, services) && output.blocked && actions.field310 == 1 && !state.unlocked,
            "Arrival processes original door barriers before opening and publishes the blocked reaction.");
        doors.setEventBlocked(savedId, false);
        input.contact = false;
        state.flags = 1;
        state.door = selected;
        unsigned presentations = 0;
        bool missingKey = false;
        services.presentKey = [&](bool missing, bool front, std::uint8_t type) {
            ++presentations;
            missingKey = missing;
            require(front == (state.side == 1) && type == doors.leaf(selected).type, "Key presentation receives the original side and door type.");
        };
        const auto start = [&] {
            actions.field310 = 1;
            std::array<std::uint32_t, 12> payload{};
            payload[8] = payload[9] = 1;
            actions.request(12, &payload);
            state.approachFrames = 0;
            state.pairedDoor = -1;
            input.position = state.approachPosition;
        };
        doors.setSavedFlag(savedId, true, saved);
        start();
        require(interaction.run(state, actions, input, output, services) && presentations == 1 && missingKey &&
            actions.fields31C[0] == 2 && !state.unlocked && (doors.leaf(selected).flags & 0x8000),
            "Missing keys retain the lock, mark the door used, and enter presentation wait.");
        services.keyPresentationActive = true;
        interaction.run(state, actions, input, output, services);
        require(actions.fields31C[0] == 2 && presentations == 1, "Key presentation is not restarted while active.");
        services.keyPresentationActive = false;
        interaction.run(state, actions, input, output, services);
        require(actions.field310 == 1 && !output.blocked, "Missing-key rejection differs from an event barrier.");
        keys[savedId >> 3] |= 1u << (savedId & 7);
        start();
        interaction.run(state, actions, input, output, services);
        require(presentations == 2 && !missingKey && state.unlocked && !(doors.leaf(selected).flags & 0x4400) &&
            !(saved[savedId >> 5] & (0x80000000u >> (savedId & 31))), "A held key clears the manager and saved lock flags before presentation.");
        interaction.run(state, actions, input, output, services);
        require(actions.fields31C[0] == 3 && (doors.leaf(selected).flags & 0x20), "Completed key presentation starts the original door animation.");
        std::uint32_t destination;
        doors.destinationRoom(selected, input.room, rooms, destination);
        require(output.room == destination, "Door opening publishes the original destination room.");
        input.animation = 0;
        input.previousFrame = input.frame = 0;
        actions.fields31C[0] = 3;
        state.pairedDoor = selected;
        input.animationEnded = true;
        interaction.run(state, actions, input, output, services);
        require(state.pairedDoor == -1 && state.unlocked && output.position == &state.approachPosition &&
            actions.fields31C[0] == 8, "Paired-leaf completion publishes the retained approach position and advances the existing protected action.");
        input.animationEnded = false;
        actions.fields31C[0] = 8;
        interaction.run(state, actions, input, output, services);
        require(output.room == state.sourceRoom, "Return stage restores the retained source room.");
        actions.fields31C[0] = 9;
        interaction.run(state, actions, input, output, services);
        require(output.instantTurn && output.reverseFacing && output.motion.facing[0] == -state.facing[0],
            "Fake-door turn stage publishes both original turn flags.");
        actions.fields31C[0] = 10;
        input.animationLooped = false;
        interaction.run(state, actions, input, output, services);
        require(actions.fields31C[0] == 10 && output.reverseFacing && !output.instantTurn, "Reverse stage waits for the animation loop.");
        input.animationLooped = true;
        interaction.run(state, actions, input, output, services);
        require(actions.fields31C[0] == 11, "The loop event advances the protected action once.");
        actions.fields31C[0] = 5;
        input.close = input.reverse = true;
        interaction.run(state, actions, input, output, services);
        require(actions.fields31C[0] == 6 && (doors.leaf(selected).flags & 0x1000), "Close input wins when both close and reverse are pressed.");
        actions.fields31C[0] = 5;
        input.close = false;
        interaction.run(state, actions, input, output, services);
        require(actions.fields31C[0] == 7 && (doors.leaf(selected).flags & 0x80) && output.room == destination,
            "Reverse input uses the original close-back command and destination publication.");
        for (unsigned i = 0; i < 100; ++i) doors.advanceAnimation(selected, [](unsigned, const auto&) {});
        doors.setSavedFlag(savedId, (originalFlags & 0x400) != 0, saved);
        doors.refreshPose(selected);
        auto& player = *dynamic_cast<playeractor::Player*>(manager.actors().actor(manager.enemies().appearPoint(0).actor));
        const auto retainedState = player.state;
        const auto retainedActions = *player.components->actions;
        const auto retainedDoor = *player.components->door;
        const auto retainedControls = *player.components->controls;
        const auto retainedPosition = player.position;
        const auto retainedRoom = player.fieldB4;
        const auto retainedLife = player.life;
        auto& reactions = *player.components->reactions;
        const auto retainedReaction = reactions.state;
        const auto retainedReactionTime = reactions.field4;
        const auto retainedReactionValue = reactions.field8;
        const auto retainedReactionPosition = reactions.fieldC;
        const auto retainedReactionFlag = reactions.field18;
        std::int32_t audioLife = 100;
        std::vector<unsigned> sounds;
        playerevents::LifeEnvironment life{audioLife, manager.particles().effects(),
            [&](unsigned id, const auto&) { sounds.push_back(id); }, [](const auto&) {}, {}};
        gameflags::State flags;
        playerinteraction::DoorFrame frame(executable);
        playerinteraction::DoorFrameServices frameServices{services, manager.actors(), flags, life};
        player.state.flags792 |= 1;
        player.state.field1130 = player.state.field1170 = 0;
        player.state.field1338 = 1;
        player.state.field1180 = 0;
        player.state.field1184 = 1;
        player.state.flags1140[2] = player.state.flags1330[1] = player.state.field1060 = 0;
        player.state.field10E4 = 1;
        player.state.flags10E8[0] = 1;
        reactions.state = 1;
        player.components->controls->flagsC[6] = player.components->controls->flagsC[7] = 1;
        player.components->actions->field310 = 1;
        require(!frame.run(player, frameServices) && player.state.field10E4 == 0 && !player.state.flags10E8[0],
            "Door eligibility is sampled before decrementing the retry delay, and the contact latch resets every frame.");
        const auto controls = frame.input(player);
        require(controls.eligible && controls.pressed && controls.primary && controls.radius == player.state.fieldF54 &&
            controls.previousFrame == player.animationFrame - player.animationFrameStep,
            "The original frame adapter reads player capabilities, button bytes and retained animation timing.");
        player.position = input.position;
        player.fieldB4 = input.room;
        require(frame.run(player, frameServices) && player.state.fieldF70 && player.state.field1070 == 7 &&
            player.state.flags10E8[0] && player.state.field10E4 > 0,
            "Manager-owned player input reaches the door dispatcher and publishes forced motion and retry state.");
        player.position = player.components->door->approachPosition;
        player.components->door->flags = 0;
        for (unsigned flag = 0; flag < 256; ++flag) flags.setOn(static_cast<std::uint8_t>(flag));
        require(frame.run(player, frameServices), "The active protected door action continues during the retry delay.");
        if (player.fieldB4 != input.room) for (unsigned flag = 0; flag < 256; ++flag)
            require(flags.value(static_cast<std::uint8_t>(flag)) == (flag < 0x55 || flag >= 0xaa),
                "Door room changes clear only the original transient event-flag interval.");

        playerevents::DamageFrame damage(executable);
        playerevents::Payload payload;
        payload.word(0x18, 4);
        payload.word(0x20, 0);
        player.components->actions->field310 = 1;
        player.state.field10C4 = player.state.field1150 = player.state.field1170 = 0;
        player.state.field1114 = 0;
        player.state.flags1040[2] = player.state.field1060 = 0;
        player.state.field11A0 = nullptr;
        player.state.fields12F4[9] = 0;
        player.life = 100;
        require(damage.run(player, payload, false, manager.actors(), catalog, false, life) && player.life == 96 &&
            player.state.field10C4 > 0 && audioLife == 96 && sounds.back() == 0x839,
            "General damage reaches original life-loss effects and audio, retaining the ordinary invulnerability delay.");
        require(!damage.run(player, payload, false, manager.actors(), catalog, false, life) && player.life == 96,
            "Invulnerability rejects repeated damage.");
        player.state.field10C4 = 0;
        player.state.field1170 = 1;
        player.state.field1178 = 1;
        payload.word(0x18, 5);
        require(damage.run(player, payload, false, manager.actors(), catalog, true, life) && player.life == 92,
            "Power scaling truncates before the original doubled-damage mode is applied.");
        player.state.field10C4 = player.state.field1170 = 0;
        payload.word(0x20, 7);
        const auto previousSounds = sounds.size();
        require(damage.run(player, payload, false, manager.actors(), catalog, false, life) && player.life == 92 &&
            player.state.field112C == 5 && sounds.size() == previousSounds,
            "Deferred damage publishes its pending amount without immediate life loss or sound.");
        player.components->actions->field310 = 2;
        player.components->actions->field314 = 0x20;
        player.state.field112C = 0;
        require(!damage.run(player, payload, false, manager.actors(), catalog, false, life),
            "Protected door actions reject damage unless a deferred amount is already pending.");
        player.state = retainedState;
        *player.components->actions = retainedActions;
        *player.components->door = retainedDoor;
        *player.components->controls = retainedControls;
        player.position = retainedPosition;
        player.fieldB4 = retainedRoom;
        player.life = retainedLife;
        reactions.state = retainedReaction;
        reactions.field4 = retainedReactionTime;
        reactions.field8 = retainedReactionValue;
        reactions.fieldC = retainedReactionPosition;
        reactions.field18 = retainedReactionFlag;
        std::cout << "Original door selection, key/barrier and protected interaction states verified.\n";
    }

    roomportal::Matrix projection(const s3denvironment::Camera& camera) {
        const auto normalize = [](std::array<float, 3> value) {
            const float length = std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
            for (auto& component : value) component /= length;
            return value;
        };
        const auto backward = normalize({camera.position[0] - camera.target[0], camera.position[1] - camera.target[1],
            camera.position[2] - camera.target[2]});
        const auto right = normalize({backward[2], 0, -backward[0]});
        const std::array<float, 3> up{backward[1] * right[2] - backward[2] * right[1],
            backward[2] * right[0] - backward[0] * right[2], backward[0] * right[1] - backward[1] * right[0]};
        const std::array<std::array<float, 3>, 3> axes{right, up, backward};
        roomportal::Matrix view{}, perspective{}, result{};
        view[15] = 1;
        for (unsigned i = 0; i < 3; ++i) for (unsigned j = 0; j < 3; ++j) {
            view[i * 4 + j] = axes[i][j];
            view[i * 4 + 3] -= axes[i][j] * camera.position[j];
        }
        const float cotangent = 1 / std::tan(camera.fieldOfView * 0.00872664626f);
        perspective[0] = cotangent / camera.aspect;
        perspective[5] = cotangent;
        perspective[10] = -camera.nearPlane / (camera.farPlane - camera.nearPlane);
        perspective[11] = -(camera.farPlane * camera.nearPlane) / (camera.farPlane - camera.nearPlane);
        perspective[14] = -1;
        for (unsigned i = 0; i < 4; ++i) for (unsigned j = 0; j < 4; ++j)
            for (unsigned k = 0; k < 4; ++k) result[i * 4 + j] += perspective[i * 4 + k] * view[k * 4 + j];
        return result;
    }
}

int main(int argc, char** argv) {
    try {
        gameentry::State absent;
        bool rejected = false;
        try { gameplay::ResourceOwner owner("missing.ppak", absent); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "A scene cannot initialize without a selected save.");
        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> executable, initialFlags;
            std::string error;
            require(package.open(argv[1], error) && package.read("sys/main.dol", executable, error) &&
                dolresources::read(executable, glme01::initialSaveFlags, 31, initialFlags, error), error);
            gamesave::State saves;
            require(gamesave::initialize(initialFlags, saves, error), error);
            const auto saved = saves.working;
            for (unsigned cycle = 0; cycle < 2; ++cycle) {
                gameentry::State entry;
                require(gameentry::selectNewFile(saves, 0, entry, error), error);
                if (cycle) entry.inventory[0x1a0 + 207 / 8] |= 0x80 >> (207 & 7);
                gameplay::ResourceOwner owner(argv[1], entry);
                const auto& residents = owner.residentRooms().slots();
                require(residents[0].room == 2 && residents[1].room == 30,
                    "Original portal order loads the foyer and its open neighbor.");
                for (unsigned i = 2; i < residents.size(); ++i) require(residents[i].room == 0xffffffff,
                    "Actor dependency rooms must not become resident room archives.");
                const std::array<std::vector<unsigned>, 2> expectedRows{{{99, 100, 101, 207, 208, 270, 295, 296}, {335, 336, 377}}};
                for (unsigned i = 0; i < expectedRows.size(); ++i) {
                    const auto& furniture = residents[i].furniture;
                    require(furniture.placements().size() == expectedRows[i].size(), "Original admitted furniture count.");
                    require(furniture.models().size() == (i ? 2 : 5) && furniture.animations().size() == (i ? 2 : 4),
                        "Original shared models and animation directory contents.");
                    for (unsigned j = 0; j < expectedRows[i].size(); ++j) require(furniture.placements()[j].row == expectedRows[i][j],
                        "Furniture admission preserves original table order.");
                }
                auto& furniture = owner.furniture();
                require(bool(furniture.rowState(207).flags & 8) == bool(cycle),
                    "Furniture collected state restores the original big-endian inventory bit order.");
                require((furniture.rowState(208).flags & 0x10) && (furniture.rowState(295).flags & 0x200) &&
                    furniture.rowState(270).height == 2 && furniture.rowState(335).height == 0 &&
                    furniture.rowState(207).occupant == -1, "Original behavior flags, height overrides and occupant sentinel.");
                require(furniture.object(99)->model == furniture.object(100)->model &&
                    furniture.object(335)->model == furniture.object(336)->model, "Instances share room model resources.");
                require(!furniture.object(295)->model && !furniture.object(296)->model,
                    "Model-less furniture retains object ownership.");
                require(furniture.object(207)->animations.size() == 2, "Chest receives both original animation bindings.");
                furniture.initializeObjects();
                for (const auto& object : furniture.slots()) {
                    if (!(object.flags & 2)) continue;
                    require((object.flags & 7) == 7 && object.updates == 1, "First furniture dispatch runs original initialization.");
                    if (!object.model) continue;
                    require(object.matrices.size() == object.model->hierarchy.visits().size(), "Object pose has one matrix per visited node.");
                    for (const auto& matrix : object.matrices) for (float value : matrix)
                        require(std::isfinite(value), "Furniture pose contains finite transformed matrices.");
                }
                furniture.initializeObjects();
                require(furniture.object(207)->updates == 1, "Resource initialization must not stand in for behavior updates.");
                for (const auto& room : owner.residentRooms().slots()) {
                    if (room.room == 0xffffffff) continue;
                    require(!room.drawing.geometry.empty(), "Resident rooms decode the original display-list geometry.");
                    std::cout << "Resident room " << room.room << ": " << room.path << '\n';
                    std::cout << "  Furniture: " << room.furniture.placements().size() << " placements, " <<
                        room.furniture.models().size() << " models, " << room.furniture.animations().size() << " animations.\n";
                }
                auto& managers = owner.managers();
                auto* player = dynamic_cast<playeractor::Player*>(managers.actors().actor(managers.enemies().appearPoint(0).actor));
                require(player && player->state.record == &entry.player && player->room() == 0x02000102,
                    "Scene resource ownership preserves the selected record and original foyer spawn.");
                {
                    auto* pad = static_cast<playerinput::Pad*>(player->state.field794);
                    playerinput::PadStatus sample;
                    sample.buttons = 0x100;
                    sample.stickX = 87;
                    managers.sampleInput(sample, 0);
                    require(pad && pad == player->state.field7A0->pad && pad->port == 0 &&
                        (pad->pressed & 0x100) && pad->stickMagnitude == 1,
                        "Production sampling updates the original shared controller pad through PADClamp.");
                    managers.sampleInput(sample, 0);
                    require(!pad->pressed, "Holding host input does not replay pressed edges on the next player tick.");
                    managers.sampleInput({}, -1);
                    require(!pad->held && pad->released && pad->port == -1,
                        "Focus and controller release reach the same manager-owned pad.");
                    managers.sampleInput({}, -1);
                }
                {
                    const auto retained = player->state;
                    const auto actorFlags = player->flags;
                    const auto life = player->life;
                    playeractions::Context context(owner.executable());
                    auto input = context.build(*player, {false, false, -1});
                    const auto boundInput = managers.preparePlayerAction(0);
                    require(boundInput.actor == player && boundInput.controller == input.controller &&
                        boundInput.maximumSpeed == input.maximumSpeed && boundInput.modes == input.modes,
                        "Manager action preparation resolves original collision and timing resources through the registered player.");
                    require(input.actor == player && input.attached == player->components->attached.get() &&
                        input.controller == player->state.field7A0 && input.maximumSpeed > 0 &&
                        input.direction == player->state.direction && input.velocity == player->velocity,
                        "Action input retains the production player, equipment, controller and original reaction speed.");
                    require(input.modes[0] == 1 && input.mode == 0,
                        "Initial action input selects ordinary locomotion and normal mode without forcing a movement amount.");
                    playercontrols::Motion motion{0.85f, {1, 0, 0}, {1, 0, 0}};
                    player->state.fieldFB8 = &motion;
                    player->flags |= 0x61000;
                    input = context.build(*player, {true, true, 1});
                    require(input.useMotion && input.flags[0] && input.motion == 0.85f && input.animationFlags[0] &&
                        input.animationFlags[1] && input.animationFlags[2] && input.animationFlags[3] &&
                        input.animationFlags[4] && input.animationFlags[5] && !(player->flags & 0x1000),
                        "Action construction consumes original ground/marker signals, inclusive thresholds, and clears the frame flag.");
                    player->state.flags134C[0] = 1;
                    require(context.build(*player, {false, false, -1}).mode == 9, "Original special-state mode has priority over lower action modes.");
                    player->life = 0;
                    require(context.build(*player, {false, false, -1}).mode == 4, "Death takes priority over the special-state mode.");
                    player->life = life;
                    player->state = retained;
                    player->state.field1070 = 3;
                    playercontrols::Motion turn{0.5f, {}, {1, 0, 0}};
                    player->state.fieldFBC = &turn;
                    player->state.fieldF68 = 1;
                    input = context.build(*player, {false, false, -1});
                    require(input.modes[0] == 2 && input.turn == 0.5f && player->state.fieldF68 == 2,
                        "Stationary turning preserves its original motion channel and animation-speed multiplier.");
                    player->state = retained;
                    player->flags = actorFlags;
                }
                require(std::isfinite(owner.camera().position[2]) && owner.camera().fieldOfView == 26,
                    "Camera initialization uses the actual manager-owned player.");
                {
                    const auto retained = player->state;
                    const auto controls = *player->components->controls;
                    const auto actions = *player->components->actions;
                    const auto life = player->life;
                    auto& input = *player->components->controls;
                    playerinput::Decoder decoder(owner.executable());
                    playerinput::Pad pad;
                    pad.stick = {1, 0};
                    gameplaycamera::CameraOwner camera;
                    camera.flags = 0;
                    camera.position = {0, 0, 10};
                    camera.target = {};
                    unsigned transitions = 0;
                    playerinput::RouteServices services{camera, true, 2, [&](playeractor::Player& target, bool enabled) {
                        require(&target == player && enabled != bool(target.state.flags1140[2]),
                            "Scanner side effects are dispatched before the retained scanner state changes.");
                        ++transitions;
                    }};
                    player->state.field794 = &pad;
                    player->state.flags792 &= ~8;
                    input.settings.fields[37] = 1;
                    input.settings.fields[38] = 0;
                    input.settings.fields[39] = 0;
                    input.settings.flags = 4;
                    playerinput::route(*player, decoder, services);
                    require(player->state.fieldFB8 == &input.motions[0] && player->state.fieldFBC == &input.motions[2] &&
                        input.motions[0].value > 0 && player->state.fieldF9C.value == input.motions[0].value,
                        "Player input routes the decoded pad into retained movement channels without replacing their ownership.");
                    player->life = 0;
                    playerinput::route(*player, decoder, services);
                    require(input.motions[0].value == 0, "The original dead-player gate suppresses pad decoding.");
                    player->life = life;
                    player->state.flags792 |= 8;
                    player->components->actions->field310 = 1;
                    input.settings.fields[7] = 0x800;
                    input.settings.fields[8] = 0;
                    input.settings.fields[2] = 0x200;
                    pad.pressed = pad.held = 0x800;
                    playerinput::route(*player, decoder, services);
                    require(transitions == 1 && player->state.flags1140[2] && !player->state.fieldFB8,
                        "Scanner entry routes its transition and suppresses player translation.");
                    pad.pressed = 0;
                    playerinput::route(*player, decoder, services);
                    require(transitions == 1 && player->state.flags1140[2], "Held input does not repeat the scanner edge transition.");
                    pad.pressed = pad.held = 0x200;
                    playerinput::route(*player, decoder, services);
                    require(transitions == 2 && !player->state.flags1140[2], "The original cancel edge leaves scanner mode.");
                    player->state.flags1140[3] = 1;
                    pad.pressed = pad.held = 0;
                    playerinput::route(*player, decoder, services);
                    require(!player->state.fieldFBC, "The original turn lock clears the retained turning channel.");
                    const auto standingActor = player->fieldA0;
                    const auto wall = player->fieldB0;
                    playermotion::VelocityContext velocityContext(owner.executable());
                    player->fieldA0 = 0;
                    player->fieldB0 = -1;
                    player->state.flagsFC0[0] = 0;
                    player->state.flags134C.fill(0);
                    player->components->actions->field310 = 1;
                    player->components->actions->fields31C[4] = 0;
                    playercontrols::Motion motion{0.2f, {}, player->state.direction};
                    player->state.fieldFB8 = &motion;
                    auto velocity = velocityContext.build(*player, managers.collision());
                    require(velocity.floor == 0xfffe && velocity.motion == &motion && velocity.direction == &player->state.direction &&
                        velocity.applyMotion && velocity.threshold == 0.1f && velocity.gravity == -3 && velocity.maximumSpeed > 0,
                        "Production movement input uses retained contact, direction, original constants and reaction records.");
                    for (auto& component : motion.second) component = -component;
                    velocity = velocityContext.build(*player, managers.collision());
                    require(!velocity.applyMotion && velocity.threshold == 0.35f,
                        "Original small input only accelerates when aligned with the player's retained direction.");
                    player->components->actions->field310 = 2;
                    player->components->actions->field314 = 0x20;
                    velocity = velocityContext.build(*player, managers.collision());
                    require(!velocity.applyFriction && !velocity.applyGravity && !velocity.applyLadderGravity && velocity.applyReflection,
                        "Protected reactions suppress friction and gravity without suppressing the original contact response.");
                    player->fieldA0 = standingActor;
                    player->fieldB0 = wall;
                    player->state = retained;
                    *player->components->controls = controls;
                    *player->components->actions = actions;
                    player->life = life;
                }
                {
                    actormodel::InitialPose pose(owner.executable());
                    playerorientation::Frame frame(owner.executable(), pose);
                    const auto retained = player->state;
                    const auto actions = *player->components->actions;
                    const auto rotation = player->rotation, previous = player->previousRotation;
                    const auto quaternion = *player->components->quaternion;
                    const auto support = player->fieldA0;
                    const auto floor = player->fieldB2;
                    playercontrols::Motion motion{0.2f, {}, {1, 0, 0}};
                    player->fieldA0 = 0;
                    player->state.fieldFB8 = &motion;
                    player->state.fieldFBC = nullptr;
                    player->state.field1070 = 0;
                    player->state.direction = {0, 0, 1};
                    player->state.flagsFC0[0] = 0;
                    player->state.flags134C.fill(0);
                    player->state.flags1140[2] = 0;
                    player->components->actions->field310 = 1;
                    player->components->actions->fields31C[4] = 0;
                    const playerorientation::FrameSignals signals{false, 0.5f, 0.75f, 0};
                    auto input = frame.build(*player, signals);
                    require(input.mode == 2 && input.enabled && input.blend == 0.18f && input.turnLimit == 50 && input.maximumSpeed == 60,
                        "Small off-axis input selects the original limited turn with imported parameters.");
                    player->components->actions->field310 = 2;
                    player->components->actions->field314 = 0x20;
                    input = frame.build(*player, signals);
                    require(!input.enabled, "Protected reactions cannot turn even with retained floor support.");
                    player->components->actions->field310 = 1;
                    player->fieldA0 = player->fieldB2 = -1;
                    input = frame.build(*player, signals);
                    require(input.mode == 7 && !input.enabled && input.targetDirection == playerorientation::Vector{0, 0, 1},
                        "Airborne turning retains the original flattened direction target.");
                    player->state.quaternion = {0, 0, 0, 1};
                    player->components->quaternion->quaternion = player->state.quaternion;
                    player->components->quaternion->field10 = 0.0f;
                    frame.run(*player, signals);
                    require(player->state.field1070 == 0 && player->rotation == player->previousRotation,
                        "Completed direction locks restore ordinary orientation and publish matching previous rotation.");

                    playercomponents::QuaternionState turn({0, 0, 0, 1});
                    playerorientation::Quaternion output{0, 0, 0, 1};
                    input = {};
                    bool unresolved = false;
                    try { playerorientation::update(turn, output, output, input, pose); }
                    catch (const std::runtime_error&) { unresolved = true; }
                    require(unresolved, "An originally unwritten angular-velocity slot must not acquire an invented native default.");
                    turn.field10 = 0.0f;
                    input.mode = 1;
                    input.enabled = true;
                    input.blend = 1;
                    input.movement = &motion;
                    const float alignment = playerorientation::update(turn, output, output, input, pose);
                    require(playerorientation::direction(output)[0] > 0.999f && alignment > 2.999f,
                        "Direct movement turns toward the original world-space stick direction with alias-safe output.");
                    input.reverse = true;
                    playerorientation::update(turn, output, output, input, pose);
                    require(playerorientation::direction(output)[0] < -0.999f, "Reaction reversal flips the original right and forward axes.");
                    input.mode = 2;
                    input.reverse = false;
                    input.turnLimit = 50;
                    output = turn.quaternion = {0, 0, 0, 1};
                    playerorientation::update(turn, output, output, input, pose);
                    const auto limited = playerorientation::direction(output);
                    require(limited[0] > 0 && limited[0] < 0.3f && limited[2] > 0.95f,
                        "Small-stick turning blends by the original table angle instead of snapping to its target.");
                    input.mode = 3;
                    input.turning = &motion;
                    input.acceleration = 30;
                    input.maximumSpeed = 60;
                    input.damping = 0.5f;
                    turn.field10 = 1.0f;
                    playerorientation::update(turn, output, output, input, pose);
                    require(turn.field10 == -6, "Sub-unit angular velocity is cleared before turn input acceleration.");
                    input.mode = 1;
                    input.enabled = false;
                    const auto before = output;
                    playerorientation::update(turn, output, output, input, pose);
                    require(turn.field10 == 0 && output != before, "Residual turning runs once before ordinary modes clear angular velocity.");
                    input.mode = 4;
                    input.target = {0, 0, 0, -1};
                    output = {0, 0, 0, 1};
                    playerorientation::update(turn, output, output, input, pose);
                    require(output[3] == 1, "Equivalent opposite quaternion signs use the original shortest blend.");
                    input.mode = 5;
                    input.targetPosition = {1, 0, 0};
                    turn.quaternion = output = {0, 0, 0, 1};
                    playerorientation::update(turn, output, output, input, pose);
                    require(output[3] == 1, "Look-at ignores a target exactly one unit away.");
                    input.mode = 6;
                    playerorientation::update(turn, output, output, input, pose);
                    require(playerorientation::direction(output)[0] < -0.999f, "Look-away retains its distinct near-target behavior.");
                    player->state = retained;
                    *player->components->actions = actions;
                    *player->components->quaternion = quaternion;
                    player->rotation = rotation;
                    player->previousRotation = previous;
                    player->fieldA0 = support;
                    player->fieldB2 = floor;
                }
                {
                    const auto state = player->state;
                    const auto position = player->position, retainedPosition = player->field5C;
                    const auto velocity = player->velocity, direction = player->direction;
                    const auto speed = player->speed;
                    const auto quaternion = *player->components->quaternion;
                    actormodel::InitialPose pose(owner.executable());
                    playermotion::VelocityFrame frame(owner.executable(), pose);
                    std::array<playermotion::PlatformMotion, 2> platforms{};
                    player->state.field10B0 = 0;
                    player->state.fieldF70 = 1;
                    player->state.fieldF74 = {3, 4, 0};
                    frame.run(*player, managers.collision(), platforms, {});
                    require(player->velocity == std::array<float, 3>{3, 4, 0} && player->speed == 5 && !player->state.fieldF70 &&
                        player->state.fieldF74 == std::array<float, 3>{}, "Root-motion velocity consumes its retained displacement exactly once.");
                    player->position = {0, 0, 0};
                    player->state.field10B0 = 1;
                    player->state.field10B4 = {10, 90, 0};
                    player->state.field10C0 = 1;
                    frame.run(*player, managers.collision(), platforms, {});
                    require(player->velocity == std::array<float, 3>{10, 0, 0} && !player->state.field10B0 &&
                        player->state.field10C0 == 0 && player->state.field1070 == 7 && player->state.field109C == 0.18f &&
                        player->state.field1090 == std::array<float, 3>{1, 0, 0},
                        "Scripted travel consumes its final frame and retains the horizontal orientation target.");
                    player->position = {10, 3, 0};
                    platforms[0] = {{0, 100, 0}, 10, 0x4000};
                    player->state.fieldF70 = 1;
                    frame.run(*player, managers.collision(), platforms, {});
                    require(!player->state.flagsFC0[3] && player->position == std::array<float, 3>{10, 3, 0},
                        "Platform transport excludes the radius boundary and ignores vertical separation.");
                    platforms[0].radius = 11;
                    platforms[1] = {{0, 100, 0}, 100, -0x4000};
                    player->state.fieldF70 = 1;
                    frame.run(*player, managers.collision(), platforms, {});
                    require(player->state.flagsFC0[3] && player->field5C == player->position && player->position[1] == 3 &&
                        player->position[2] < -9.99f && std::abs(player->position[0] - 20) < 0.01f,
                        "Transport preserves the original center-minus-position displacement and first matching platform priority.");
                    player->position = {30, 5, 0};
                    player->setVelocity({0, 2, 0});
                    player->state.flags134C[2] = 0;
                    frame.constrain(*player);
                    require(player->velocity == std::array<float, 3>{0, 2, 0}, "Inactive radial constraint leaves velocity untouched.");
                    player->state.flags134C[2] = 1;
                    player->state.field1350 = {0, 100, 0};
                    player->state.field1360 = 10;
                    player->state.field1364 = 10;
                    frame.constrain(*player);
                    require(player->velocity == std::array<float, 3>{-40, 2, 0} && player->position == std::array<float, 3>{30, 5, 0} &&
                        player->state.field1070 == 5 && player->state.field1074 == std::array<float, 3>{0, 7, 0} && player->state.field109C == 0.18f,
                        "Radial constraint accumulates hard and soft corrections and targets the predicted horizontal plane before contact integration.");
                    player->position = {200, 0, 0};
                    player->setVelocity({0, 0, 0});
                    frame.constrain(*player);
                    require(player->velocity == std::array<float, 3>{-50, 0, 0} && player->speed == 50,
                        "Each radial correction retains the original full-vector fifty-unit speed cap.");
                    player->position = {10, 0, 0};
                    player->setVelocity({0, 0, 0});
                    player->state.field1070 = 0;
                    frame.constrain(*player);
                    require(player->velocity == std::array<float, 3>{} && player->state.field1070 == 0,
                        "Exact radius and turn-distance equality do not activate either constraint branch.");
                    player->state = state;
                    player->position = position;
                    player->field5C = retainedPosition;
                    player->velocity = velocity;
                    player->direction = direction;
                    player->speed = speed;
                    *player->components->quaternion = quaternion;
                }
                managers.updatePlacement(0);
                if (!cycle) checkDoorInteraction(owner);
                require(managers.placements().size() == 46, "Independent production resource ownership admits the full fresh mission scan.");
                const auto submissions = owner.prepareVisibility(projection(owner.camera()), {0, 0, 640, 480});
                require(submissions.size() == 2 && submissions[0].room == player->room() && (submissions[1].room & 255) == 30 &&
                    owner.visibility().marked(player->room() >> 24, 0, 0) && owner.visibility().marked(submissions[1].room >> 24, 0, 0),
                    "First-area traversal uses real manager-owned player and door state.");
                unsigned visibleDoors = 0;
                for (unsigned i = 0; i < managers.doors().size(); ++i) if (managers.doors().leaf(i).flags & 1) ++visibleDoors;
                require(visibleDoors != 0, "Room traversal supplies the original door presentation stage.");
                require((entry.inventory[0xa5] & 1) && saves.working == saved,
                    "Room discovery updates the selected inventory without writing the memory card.");
                std::cout << "Visible resident rooms:";
                for (const auto& submission : submissions) std::cout << ' ' << (submission.room & 255);
                std::cout << '\n';
                for (unsigned tick = 0; tick < 120; ++tick) {
                    managers.updatePlacement(tick / 30);
                    managers.updateGenerators();
                    for (unsigned i = 0; i < managers.enemies().slots().size(); ++i) managers.updateOwner(i);
                    furniture.updateIdle(owner.visibility(), 0);
                    furniture.updateOpacity(player->position[2]);
                }
                require(furniture.object(207)->updates == 121 && furniture.object(99)->updates == 121,
                    "Furniture receives every scene update after initialization.");
                for (const auto& object : furniture.slots()) {
                    if (!(object.flags & 2)) continue;
                    require(object.swing == std::array<float, 2>{} && object.swingVelocity == std::array<float, 2>{},
                        "Unforced first-area furniture remains at its original spring equilibrium.");
                    for (const auto& matrix : object.matrices) for (float value : matrix)
                        require(std::isfinite(value), "Live furniture updates retain finite model poses.");
                }
                require(managers.placements().size() == 46 && saves.working == saved,
                    "Scene updates preserve admission and do not write the memory card.");
                {
                    auto& pool = *managers.players().records;
                    require(pool.append(actorcollision::Volume{0, {}, {}}, {}), "Production contact pool accepts a registration.");
                    player->field9C = player->fieldA0 = 123;
                    managers.updateContacts();
                    require(player->field9C != 123 && player->fieldA0 != 123 && pool.field8 == 0 && pool.fieldC == 1,
                        "Production contact pass clears prior actor contacts before traversal and retires the registered collision pool afterward.");
                    require(player->room() >> 24 != 255 && std::isfinite(player->position[1]),
                        "Production collision and room resolution retain a valid first-area player position.");
                }
                furniture.release(99);
                require(!furniture.object(99) && furniture.object(100)->model == &residents[0].furniture.models()[0],
                    "Releasing an instance retains shared resources and its neighbor.");
                {
                    unsigned capturePasses = 0, soundPasses = 0, soundEvents = 0;
                    actormodel::Matrix retainedTransform{};
                    actoranimation::SoundServices sound{
                        [&](unsigned, actoranimation::SoundHandle*&) { ++soundEvents; },
                        [](actoranimation::SoundHandle&, float) {},
                        [](actoranimation::SoundHandle&, float) {},
                        [](actoranimation::SoundHandle&, float) {}};
                    for (unsigned frame = 0; frame < 60; ++frame) {
                        managers.actors().updatePoses([&] {
                            require(player->previousPosition == player->position,
                                "The complete registry pose pass finishes actor clocks before capture deformation.");
                            ++capturePasses;
                            managers.updateCaptures(nullptr, nullptr, retainedTransform);
                        }, [&](actors::MoveActor& actor) {
                            require(capturePasses == frame + 1,
                                "Registry animation sound follows the capture and pose publication stages.");
                            actor.updateAnimationSound(sound);
                            ++soundPasses;
                        });
                        for (float value : player->model->matrix(0, 7))
                            require(std::isfinite(value), "Repeated original pose passes preserve the live player's joint transform.");
                    }
                    require(capturePasses == 60 && soundPasses >= 60,
                        "All sixty manager-owned pose passes execute capture and active-actor sound stages.");
                    std::cout << "Registry pose passes: " << capturePasses << ", animation sound requests: " << soundEvents << '\n';
                }
                furniture.releaseAll();
                for (const auto& object : furniture.slots()) require(!(object.flags & 2), "Furniture release returns every pool slot.");
                require(bool(furniture.rowState(207).flags & 8) == bool(cycle) && furniture.rowState(270).height == 2,
                    "Releasing resident objects preserves persistent furniture row state.");
                std::cout << "Foyer resource ownership and camera verified, cycle " << cycle + 1 << ".\n";
            }
        }
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
