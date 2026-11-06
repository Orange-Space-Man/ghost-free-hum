#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/scene/gameplayresourceowner.h"
#include "game/player/playerinputrouting.h"
#include "game/player/playeractionframe.h"
#include "game/player/playerattachedframe.h"
#include "game/player/playervelocityframe.h"
#include "game/player/playerorientationframe.h"
#include "game/player/playerfinishframe.h"
#include "game/player/playerenvironmentframe.h"
#include "game/player/playerequipmentframe.h"
#include "game/player/playeraudioframe.h"
#include "jlib/jaudio/jaudiointerfaceclock.h"
#include "game/player/playerbreathframe.h"
#include "game/actors/actortimingsampler.h"
#include "game/player/playerenergyframe.h"
#include "game/player/playercaptureframe.h"
#include "game/player/playercontrolframe.h"
#include "game/player/playertimerframe.h"
#include "game/player/playerreactionframe.h"
#include "game/player/playercontactreactions.h"
#include "game/player/playerreactioneffects.h"
#include "game/player/playerheadtracking.h"
#include "game/world/dooranimationpose.h"
#include "game/player/playerdoorframe.h"
#include "game/player/playermainframe.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

void checkOwnedPlayerFinish(const std::filesystem::path& package, const gamesave::State& saves);
void checkOwnedPlayerHealth(const std::filesystem::path& package, const gamesave::State& saves);
void checkPlayerFrameComponents(const std::filesystem::path& package, const gamesave::State& saves);

namespace {
    [[noreturn]] void unexpectedService() { throw std::runtime_error("First-visit movement requested an inactive presentation or scanner service."); }

    class Presentation final : public playerpresentation::Services {
    public:
        void effect(unsigned, const std::array<float, 3>&) override { unexpectedService(); }
        bool specialItem(unsigned) const override { unexpectedService(); }
        void music(unsigned) override { unexpectedService(); }
        void collected(actors::MoveActor&) override { unexpectedService(); }
        void setFlag(unsigned) override { unexpectedService(); }
        bool equipmentOwned() const override { unexpectedService(); }
        void equipmentEvent(unsigned, unsigned, unsigned) override { unexpectedService(); }
        void sound(unsigned, const std::array<float, 3>&) override { unexpectedService(); }
        void scene(unsigned, std::int32_t) override { unexpectedService(); }
    };

    class Scanner final : public playerscanner::Services {
    public:
        std::int16_t delay() const override { unexpectedService(); }
        void playScanSound(unsigned) override { unexpectedService(); }
        playerscanner::Hit furniture(const playerscanner::Vector&, const playerscanner::Vector&) override { unexpectedService(); }
        playerscanner::Hit actor(const playerscanner::Vector&, const playerscanner::Vector&) override { unexpectedService(); }
        unsigned actorType(int) override { unexpectedService(); }
        std::optional<unsigned> actorMessage(int) override { unexpectedService(); }
        void setDiscoveryFlag(unsigned) override { unexpectedService(); }
        void selectMessage(unsigned, unsigned) override { unexpectedService(); }
        void openMessage() override { unexpectedService(); }
        void markFurniture(int) override { unexpectedService(); }
        bool mirror(int) override { unexpectedService(); }
        bool furnitureMessage(int) override { unexpectedService(); }
        void openFurnitureMessage() override { unexpectedService(); }
    };

    constexpr unsigned flashlightIdleAction = 0x2a;
    void require(bool condition, const std::string& message) {
        if (!condition) throw std::runtime_error(message);
    }

    float horizontalDistance(const std::array<float, 3>& a, const std::array<float, 3>& b) {
        const float x = a[0] - b[0], z = a[2] - b[2];
        return std::sqrt(x * x + z * z);
    }

    void checkFinish(const std::filesystem::path& package, const gamesave::State& saves) {
        gameentry::State entry;
        std::string error;
        require(gameentry::selectNewFile(saves, 0, entry, error), error);
        gameplay::ResourceOwner owner(package, entry);
        auto& managers = owner.managers();
        auto& player = *managers.playerEvents().player(0);
        auto& state = player.state;
        auto& actions = *player.components->actions;
        const auto resources = managers.preparePlayerDoor(0);
        const actormodel::InitialPose pose(owner.executable());
        const playeractor::FinishFrame finish(owner.executable(), pose);
        std::string calls;
        unsigned damageCalls = 0, damageKind = 0;
        playeractor::FinishServices services{resources.catalog, resources.models,
            [&](auto& recipient, const playerevents::Payload& payload) {
                require(&recipient == &player && payload.word(0x18) == 7 && state.field112C == 7,
                    "Deferred damage dispatch must see the pending amount before it is cleared.");
                require(payload.word(0x20) == damageKind && payload.word(0x14) == 0xffffffff,
                    "Deferred damage must retain original reaction selection and absent source.");
                ++damageCalls;
            },
            [&](bool active) {
                require(bool(state.flags1388[2]) == active, "Interaction ownership must be published before notification.");
                calls += active ? 'b' : 'e';
            },
            [&](auto& recipient) { require(&recipient == &player, "Interaction event must target its player."); calls += 'v'; },
            [&](auto& target) { require(&target == &player, "Interaction notification must use the retained actor."); calls += 'o'; }};
        state.field132C = (player.fieldB4 >> 24) ^ 1;
        state.fieldF68 = 0.375f;
        state.fieldF60 = 2;
        state.fieldF64 = 0.75f;
        state.flags1388[1] = 1;
        finish.run(player, services);
        require(state.flags1330[0] == 1 && state.field132C == player.fieldB4 >> 24 &&
            player.animationSpeed == 0.375f && player.scale == std::array<float, 3>{2, 1.5f, 2} && !state.flags1388[1],
            "Frame completion must publish room change, animation speed and scale, then clear input-edge suppression.");
        finish.run(player, services);
        require(!state.flags1330[0] && calls.empty(), "A stable room and inactive interaction must not produce repeated transitions.");
        state.flags10E8[0] = 1;
        state.field1064 = &player;
        finish.run(player, services);
        require(calls == "bvo", "Interaction entry must acquire ownership, send event 0x3a, then notify the target owner.");
        finish.run(player, services);
        require(calls == "bvo", "An active interaction must not reacquire ownership each frame.");
        state.flags10E8[0] = 0;
        actions.field310 = 2;
        actions.field314 = 0x22;
        finish.run(player, services);
        require(calls == "bvo", "Protected interaction action must retain ownership after the contact flag clears.");
        actions.field314 = 0;
        finish.run(player, services);
        finish.run(player, services);
        require(calls == "bvoe", "Interaction exit must release ownership exactly once.");
        state.field1064 = nullptr;
        std::vector<std::uint8_t> bytes;
        require(dolresources::read(owner.executable(), glme01::playerCaptureConcealFrames, 4, bytes, error), error);
        const auto conceal = static_cast<std::int32_t>((unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) |
            (unsigned(bytes[2]) << 8) | bytes[3]);
        state.field105C = float(conceal);
        require(finish.bodyScale(state) == 0, "Capture concealment starts at zero scale.");
        state.field105C *= 0.5f;
        require(finish.bodyScale(state) == 1, "Capture concealment recovers the original linear half scale.");
        state.field105C = float(conceal) * 2;
        require(finish.bodyScale(state) == -2, "Out-of-range original concealment must not be silently clamped.");
        state.field105C = 0;
        state.field1170 = 1;
        state.field1174 = 0;
        state.field1178 = 0.5f;
        require(finish.bodyScale(state) == 1, "Settled power scaling must retain the original size multiplier.");
        state.field1170 = 0;
        require(finish.bodyScale(state) == 2, "Expired power with no recovery timer must restore base scale.");
        state.field1170 = 1;
        state.field1174 = 20;
        require(std::abs(finish.bodyScale(state) - 2) < 0.00001f,
            "Original twenty-frame power transition starts at base scale.");
        state.field1174 = 10;
        require(std::abs(finish.bodyScale(state) - 1.5f) < 0.00001f,
            "Imported oscillation table must produce the half-transition scale.");
        state.field1174 = 8;
        require(std::abs(finish.bodyScale(state) - 1) < 0.00001f,
            "Power scaling retains its original intermediate oscillation rather than a linear blend.");
        state.field1174 = 4;
        require(std::abs(finish.bodyScale(state) - 2) < 0.00001f,
            "Power scaling must return to base scale on the next original oscillation.");
        state.field1170 = 0;
        state.field1174 = 20;
        require(std::abs(finish.bodyScale(state) - 1) < 0.00001f,
            "Recovery begins at the altered scale using its distinct interpolation formula.");
        state.field1174 = 8;
        require(std::abs(finish.bodyScale(state) - 2) < 0.00001f,
            "Recovery reverses the original oscillation endpoints.");
        state.field1174 = 0;
        const auto& type = resources.catalog.type(player.actorType);
        bool found = false;
        for (unsigned animation = 0; animation < type.animations.size() && !found; ++animation) {
            const auto timing = resources.models.timing(type.model, type.animations[animation][0]);
            if (actoranimation::sampleTiming(timing.get(), "Ldam_timing", 0) == -10000) continue;
            for (unsigned frame = 1; frame < 600 && !found; ++frame) {
                if (actoranimation::timingCrossing(timing.get(), "Ldam_timing", float(frame - 1), float(frame)) <= 0) continue;
                player.animationState = animation;
                player.animationFrame = float(frame);
                player.animationFrameStep = 1;
                actions.field310 = 2;
                actions.field314 = 0x20;
                state.field112C = 7;
                finish.run(player, services);
                require(damageCalls == 1 && state.field112C == 0, "The original damage marker must consume pending damage once.");
                finish.run(player, services);
                require(damageCalls == 1, "Revisiting the marker must not repeat consumed damage.");
                actions.field314 = 0;
                damageKind = 1;
                state.field112C = 7;
                finish.run(player, services);
                require(damageCalls == 2 && !state.field112C, "Ordinary deferred damage must select reaction kind one.");
                player.animationFrameStep = 0;
                state.field112C = 7;
                finish.run(player, services);
                require(damageCalls == 2 && state.field112C == 7, "Without a marker crossing, pending damage must remain queued.");
                found = true;
            }
        }
        require(found, "The imported Luigi animations must supply a real deferred-damage timing marker.");
        std::cout << "Player frame completion: room, scale, interaction lifecycle and original damage marker verified.\n";
    }

    void run(const std::filesystem::path& package, gamesave::State& saves, unsigned direction, unsigned idleFrames, std::ostream& trace) {
        std::string error;
        gameentry::State entry;
        require(gameentry::selectNewFile(saves, 0, entry, error), error);
        gameplay::ResourceOwner owner(package, entry);
        auto& managers = owner.managers();
        auto* player = managers.playerEvents().player(0);
        require(player != nullptr, "Locomotion requires the manager-owned player.");
        require(managers.enemies().playerCount() == 1, "Normal startup must not create a second collidable Luigi.");
        require((player->state.flags792 & 12) == 4, "The first visit must retain flashlight ownership without the Poltergust.");
        owner.publishCamera();
        const auto services = managers.preparePlayerDoor(0);
        const auto firstVisitIdleAnimation = player->components->actions->entries.at(flashlightIdleAction);
        const actormodel::InitialPose pose(owner.executable());
        const playerenvironment::Frame environment(owner.executable());
        const playeractor::EquipmentFrame equipment(owner.executable());
        const playeractor::MainFrame mainFrame(owner.executable());
        Presentation presentationServices;
        Scanner scannerServices;
        unsigned lightFrames = 0;
        std::uint8_t specialActive = 0;
        auto& life = managers.playerLife();
        jaudio::InterfaceClock audioClock;
        unsigned footstepTicks = 0;
        unsigned roomNotifications = 0;
        auto environmentServices = managers.preparePlayerEnvironment(0);
        const auto notifyRoom = environmentServices.roomSound;
        environmentServices.roomSound = [&, notifyRoom](unsigned room) {
                require(room == player->fieldB4, "Environment audio notification must carry the actual packed room.");
                notifyRoom(room);
                require(managers.music().routing().room == (room & 255),
                    "Environment notification must reach the production music owner's room routing.");
                ++roomNotifications;
            };
        require(!player->components->quaternion->field10, "The constructor must retain the originally unwritten turning slot.");
        const playeractions::FrameServices actionServices{managers.flags(), services.catalog, services.models,
            [](unsigned id) { throw std::runtime_error("Locomotion fixture reached global sound " + std::to_string(id)); },
            [&](unsigned id, const auto& position) { managers.playback()->play(id, position); }};
        auto origin = player->position;
        const auto spawn = origin;
        unsigned grounded = 0, movingAnimations = 0, wallContacts = 0;
        bool returnedToIdle = false;
        std::array<float, 3> releasedPosition{}, wallPosition{};
        for (unsigned frame = 0; frame < 300; ++frame) {
            managers.setAudioInterfaceFrame(audioClock.frame());
            if (frame == idleFrames) {
                origin = player->position;
                require(horizontalDistance(spawn, origin) < 0.001f,
                    "Idle contact processing must not displace Luigi from his original spawn.");
            }
            playerinput::PadStatus pad;
            if (frame >= idleFrames && frame < 210) {
                if (direction == 0) pad.stickX = 87;
                else if (direction == 1) pad.stickX = -87;
                else if (direction == 2) pad.stickY = 87;
                else pad.stickY = -87;
            }
            managers.sampleInput(pad, 0);
            auto doorServices = managers.preparePlayerDoor(0);
            playerinteraction::DoorFrameServices doorUpdate{doorServices, managers.actors(), managers.flags(), life, false,
                actionServices.globalSound};
            playeractor::TimerServices timerServices{life, false, frame,
                [](bool) { throw std::runtime_error("First-visit locomotion unexpectedly requested an audio-rate change."); },
                [](auto&, const auto&, unsigned) { throw std::runtime_error("Locomotion unexpectedly reached deferred attack damage."); },
                [](auto&, unsigned, unsigned, unsigned, unsigned) { throw std::runtime_error("Locomotion unexpectedly requested a weapon pulse."); },
                [](unsigned, const auto&) { throw std::runtime_error("Locomotion unexpectedly requested an attack matrix effect."); }};
            const std::array<playermotion::PlatformMotion, 2> platforms{};
            playeractor::MainFrameServices mainServices{environmentServices, doorUpdate, managers.preparePlayerLadder(0),
                &presentationServices, &scannerServices, managers.playerEvents(), timerServices, specialActive};
            if (frame == 0) {
                auto incomplete = mainServices;
                incomplete.owner = {};
                const auto position = player->position;
                bool rejected = false;
                try { mainFrame.run(*player, incomplete); }
                catch (const std::invalid_argument&) { rejected = true; }
                require(rejected && player->position == position,
                    "An incomplete main binding must fail before mutating the player.");
                auto foreignLife = life;
                playeractor::TimerServices foreignTimers{foreignLife, false, frame, {}, {}, {}, {}};
                rejected = false;
                try { managers.updatePlayerMain(0, {&presentationServices, &scannerServices, foreignTimers, platforms}); }
                catch (const std::invalid_argument&) { rejected = true; }
                require(rejected && player->position == position && managers.players().frame() == 0,
                    "The owned scheduler rejects a foreign health environment before updating the actor or clocks.");
                rejected = false;
                try { managers.updatePlayerMain(1, {&presentationServices, &scannerServices, timerServices, platforms}); }
                catch (const std::out_of_range&) { rejected = true; }
                require(rejected && player->position == position, "The owned scheduler rejects an absent player before updating the scene.");
                player->components->scanner->state[2] = 1;
            }
            managers.updatePlayerMain(0, {&presentationServices, &scannerServices, timerServices, platforms});
            require(!player->state.flags1140[3] && !player->components->scanner->state[2],
                "The original scanner stage must publish its inactive result before equipment and movement.");
            if (frame == 0) require(player->state.field10E0 >= 0,
                "Before contact integration, the original forward probe must resolve the imported foyer floor.");
            auto& weapon = *player->components->weapon;
            if (frame == 0) require(player->components->quaternion->field10 == 0,
                "The initial direction lock must clear retained turning speed through the original update.");
            require(player->view.room == player->fieldB4 && player->view.direction == &player->state.direction,
                "The completed player frame must publish the current room and direction to its manager view.");
            managers.updateContacts();
            owner.updateCamera();
            managers.updatePoses();
            require(managers.players().frame() == float(frame), "Shared player clock must retain one advance per original manager frame.");
            managers.completePlayerFrame({false,
                [](auto&, bool) { throw std::runtime_error("First-visit late frame unexpectedly requested scanner cancellation."); },
                [](auto&, auto, auto, auto, auto) { throw std::runtime_error("First-visit late frame unexpectedly exposed vacuum pipe geometry."); },
                {}});
            require(player->components->grids->fields16BC[0] == 1,
                "The complete interaction flow must retain the actual foyer furniture sheet.");
            const auto& sheet = *player->components->grids->grids[0];
            require(sheet.field148 == 0 && sheet.suctionPosition == player->view.field8A4 && !sheet.suctionVolume,
                "Late updates must use the real flashlight-only player's published references for furniture cloth.");
            for (unsigned point = 0; point < sheet.count; ++point)
                for (float coordinate : sheet.points[point].field2C)
                    require(std::isfinite(coordinate), "The integrated foyer sheet must retain finite geometry throughout movement.");
            require((player->surface().materials.at(1).flags & 2) && !(player->state.field7A4->flags & 0xc000) &&
                (player->state.field7A8->flags & 0xc000) == 0xc000,
                "First-visit late state must retain flashlight visibility and hide vacuum equipment.");
            if (player->state.field1118 > 0) require(managers.playerFootstepFrames()[0] == audioClock.frame(),
                "Footstep history must use the retained audio interface clock, including stalled audio frames.");
            footstepTicks += player->state.field1118 > 0;
            if (player->components->light->state) {
                ++lightFrames;
                require(managers.lights().count() == 2 && managers.lights().entry(1).priority == 8 && player->components->light->owner == player &&
                    player->components->light->fieldC == weapon.first->position,
                    "Late flashlight update must use the manager-owned player, endpoint and room-light pool.");
                const auto& cone = *managers.players().models->animation(2);
                require(cone.frame == float(frame % static_cast<unsigned>(cone.frameCount)),
                    "Manager-bound cone animation must consume the shared player clock rather than a separate light clock.");
            }
            require(managers.players().frame() == float(frame + 1), "The owned late frame advances the shared player clock exactly once.");
            owner.publishCamera();
            if (frame % 3 != 0) {
                audioClock.mixed();
                audioClock.mixed();
            }
            audioClock.update({});
            for (const auto value : weapon.first->position) require(std::isfinite(value), "Flashlight endpoint must remain finite.");
            require(weapon.firstType == 7 && player->state.field7A4->parent == player,
                "The weapon frame must retain the original flashlight child ownership.");
            for (const auto value : player->position) require(std::isfinite(value), "Locomotion produced a non-finite position.");
            for (const auto value : player->state.quaternion) require(std::isfinite(value), "Locomotion produced a non-finite orientation.");
            require(player->room() != 0xffffffff, "Locomotion lost its room owner.");
            grounded += player->fieldA0 >= 0 || player->fieldB2 >= 0;
            wallContacts += player->fieldB0 >= 0;
            if (frame >= idleFrames && frame < 210) movingAnimations += player->animationState != firstVisitIdleAnimation;
            if (frame >= 210 && player->animationState == firstVisitIdleAnimation) returnedToIdle = true;
            if (frame == 180) wallPosition = player->position;
            if (frame > 180 && frame < 210) require(horizontalDistance(wallPosition, player->position) < 0.01f,
                "Sustained input must not push Luigi through the imported foyer wall.");
            if (frame == 240) releasedPosition = player->position;
            trace << idleFrames << ',' << direction << ',' << frame << ',' << player->position[0] << ',' << player->position[1] << ',' <<
                player->position[2] << ',' << player->velocity[0] << ',' << player->velocity[1] << ',' << player->velocity[2] << ',' <<
                player->animationState << ',' << player->animationFrame << ',' << player->fieldB2 << ',' << player->fieldB0 << '\n';
        }
        require(horizontalDistance(origin, player->position) > 10, "Held controller input did not move Luigi through the imported foyer.");
        require(managers.music().routing().room == (player->fieldB4 & 255),
            "The manager-owned main update must publish the player's actual audio room.");
        require(lightFrames > 250, "Original flashlight state gating must remain active during first-visit movement.");
        require(player->state.field1180 == 5 && player->state.field7A4->actorType == 7,
            "The uncleared foyer must select the real flashlight attachment without a vacuum.");
        require(grounded > 250, "Locomotion did not retain the imported foyer's floor contact.");
        require(wallContacts > 0, "The held-input path must exercise an actual map wall.");
        require(movingAnimations > 0, "Movement did not select an original locomotion animation.");
        require(footstepTicks > 0, "Movement must cross original footstep timing markers and submit real sound handles.");
        const auto& finalAction = *player->components->actions;
        require(returnedToIdle && finalAction.field310 == 1 && finalAction.field60 == 0x50 &&
            finalAction.fields31C[4] == 6 && player->animationState == finalAction.entries.at(0x50) && player->state.field116C == 90,
            "Released movement must return to idle, then enter the original flashlight-only idle-timeout animation.");
        require(horizontalDistance(releasedPosition, player->position) < 1, "Released input did not settle through original friction.");
        if (direction == 0 && idleFrames == 0) {
            const auto equipmentFlags = player->state.flags792;
            const auto apply = [&](const playerinput::PadStatus& pad) {
                managers.sampleInput(pad, 0);
                environment.run(*player, environmentServices);
                managers.updatePlayerInput(0, true, {});
                equipment.run(*player, actionServices.positionalSound);
                require(player->state.flags792 == equipmentFlags && player->state.field1180 == 5 && !player->state.flags1140[2],
                    "Pre-Poltergust buttons and triggers must not grant vacuum, elements or scanner access.");
            };
            for (unsigned button = 0; button < 16; ++button) {
                playerinput::PadStatus pad;
                pad.buttons = static_cast<std::uint16_t>(1u << button);
                apply(pad);
                apply({});
            }
            for (unsigned trigger = 0; trigger < 3; ++trigger) {
                playerinput::PadStatus pad;
                pad.triggerLeft = trigger != 1 ? 255 : 0;
                pad.triggerRight = trigger != 0 ? 255 : 0;
                apply(pad);
                apply({});
            }
            const auto room = player->fieldB4 >> 24;
            const auto originalRoomFlags = owner.roomFlags().at(room);
            owner.roomFlags().at(room) |= 0x8000;
            environment.run(*player, environmentServices);
            equipment.run(*player, actionServices.positionalSound);
            require(managers.roomCleared(room) && !player->state.field119D && !player->state.field1180,
                "The original cleared-room flag must put away the flashlight.");
            managers.flags().setOn(0x3d);
            environment.run(*player, environmentServices);
            equipment.run(*player, actionServices.positionalSound);
            require(!managers.roomCleared(room) && player->state.field119D && player->state.field1180 == 5,
                "The original map-two override must restore the uncleared-room flashlight behavior.");
            managers.flags().setOff(0x3d);
            owner.roomFlags().at(room) = originalRoomFlags;
            auto& presentation = *player->components->state80;
            presentation.field10 = 1;
            presentation.field44 = 13;
            presentation.field14 = {0, -1, 0, 23, 1, 0, 0, -17, 0, 0, 1, 9};
            const auto expectedPresentation = dooranimation::concatenate(presentation.field14,
                dooranimation::concatenate(player->matrix, player->model->matrix(0, presentation.field44)));
            player->flags |= 0x40;
            player->state.field1118 = 123;
            const auto footsteps = managers.playerFootstepFrames();
            player->components->grids->fields16BC[2] = 1;
            unsigned sheets = 0;
            managers.lights().beginFrame();
            managers.updatePlayerLate(0, {false, {}, {}, [&](auto& recipient) {
                require(&recipient == player && presentation.field48 == expectedPresentation &&
                    recipient.state.fieldF5C == 255,
                    "Late sheet dispatch must follow opacity, presentation composition and endpoint publication.");
                ++sheets;
            }});
            require(sheets == 1 && player->state.field1118 == 123 && managers.playerFootstepFrames() == footsteps,
                "Paused late frames must retain footstep state while continuing the other original late stages.");
            player->components->grids->fields16BC[2] = 0;
            player->flags &= ~0x40u;
            presentation.field10 = 0;
            bool rejected = false;
            try { managers.updatePlayerLate(1, {}); }
            catch (const std::out_of_range&) { rejected = true; }
            require(rejected, "Late scheduling must reject an unowned player index.");
            player->state.flags792 |= 8;
            player->flags &= ~0xc000u;
            rejected = false;
            try { managers.updatePlayerLate(0, {}); }
            catch (const std::runtime_error& error) {
                rejected = std::string(error.what()).find("vacuum pipe") != std::string::npos;
            }
            require(rejected, "Visible vacuum equipment must require its original pipe dependency rather than skipping it.");
            player->state.flags792 = equipmentFlags;
            const std::array<playermotion::PlatformMotion, 2> platforms{};
            rejected = false;
            try { managers.updatePlayerMotion(1, {life, false, 300, platforms, actionServices.globalSound}); }
            catch (const std::out_of_range&) { rejected = true; }
            require(rejected, "Motion scheduling must reject a player not owned by the manager.");
            player->view.field8C0 = 0.5f;
            managers.updatePlayerMotion(0, {life, false, 300, platforms, actionServices.globalSound});
            const auto weaponAttachment = [&]() -> const actors::MoveActor::Child& {
                for (const auto& child : player->children) if (child.actor == player->state.field7A8) return child;
                throw std::runtime_error("Weapon shake requires the original attached child.");
            };
            require(weaponAttachment().transformed, "Weapon shake must publish into the retained child attachment.");
            const auto firstShake = weaponAttachment().transform;
            player->view.field8C0 = 0.5f;
            managers.updatePlayerMotion(0, {life, false, 301, platforms, actionServices.globalSound});
            require(firstShake != weaponAttachment().transform,
                "Successive weapon shakes must consume the retained manager random stream.");
            for (float value : weaponAttachment().transform)
                require(std::isfinite(value), "Manager-owned weapon shake must produce finite transforms.");
        }
        std::cout << "Idle frames " << idleFrames << ", direction " << direction << ": distance " << horizontalDistance(origin, player->position) <<
            ", grounded " << grounded << ", wall contacts " << wallContacts << ", moving animation ticks " << movingAnimations << '\n';
    }
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) throw std::invalid_argument("Supply the imported asset package for the locomotion check.");
        gameassets::Package package;
        std::vector<std::uint8_t> executable, initialFlags;
        std::string error;
        require(package.open(argv[1], error) && package.read("sys/main.dol", executable, error) &&
            dolresources::read(executable, glme01::initialSaveFlags, 31, initialFlags, error), error);
        gamesave::State saves;
        require(gamesave::initialize(initialFlags, saves, error), error);
        checkFinish(argv[1], saves);
        checkOwnedPlayerFinish(argv[1], saves);
        checkOwnedPlayerHealth(argv[1], saves);
        checkPlayerFrameComponents(argv[1], saves);
        std::ofstream trace;
        if (argc > 2) {
            trace.open(argv[2]);
            require(bool(trace), "Cannot open the requested locomotion trace.");
            trace << "idle_frames,direction,frame,x,y,z,vx,vy,vz,animation,animation_frame,floor,wall\n";
        }
        for (unsigned idleFrames : {0u, 30u})
            for (unsigned direction = 0; direction < 4; ++direction) run(argv[1], saves, direction, idleFrames, trace);
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
