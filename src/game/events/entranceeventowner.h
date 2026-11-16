#pragma once

#include <functional>

#include "game/events/entrancescript.h"
#include "game/scene/gameplaycamerafollow.h"
#include "game/startup/scenetransition.h"
#include "game/events/eventactorcommands.h"
#include "game/events/eventpathanimation.h"
#include "game/events/eventmessagetext.h"
#include "game/events/eventmessagewindows.h"
#include "game/scene/gameplaymanagerservices.h"

namespace gameevent {
    class EntranceOwner final : private EntranceServices, private ActorServices {
    public:
        EntranceOwner(gameassets::Package& package, const enemytypes::Catalog& types,
            gameentry::State& entry, const roomlocation::Rooms& rooms, gameplay::ManagerServices& managers,
            gameplaycamera::CameraOwner& camera, const std::vector<std::uint8_t>& executable, MessageWindows& messages, gameplaycamera::Follow& follow);
        void bindRoomLight(std::function<void(bool)> change);
        void bindControllerInstructions(std::function<void()> request);
        void bindPlayerDemo(std::function<void(std::string_view)> request);
        void playerDemo(std::string_view name) override;
        void update(unsigned elapsedSeconds, std::uint32_t audioFrame);
        bool updateCamera();
        bool menuEnabled() const;
        bool fullscreen() const;
        bool active() const;
        bool cameraActive() const;
        const scenetransition::Request& transition() const;

    private:
        std::function<void(bool)> changeRoomLight;
        void playerLight(bool enabled) override;
        std::function<void()> requestInstructions;
        void controllerInstructions() override;
        void wipeKind(std::int32_t kind) override;
        void wipeTime(std::int32_t duration) override;
        void warp(std::int32_t destination) override;
        bool acknowledgeMessage() override;
        void cameraTarget(std::string_view actor, std::int32_t height) override;
        scenetransition::Request requestedTransition;
        bool flag(unsigned index) const override;
        void menu(bool enabled) override;
        void stopPlayer() override;
        void releasePlayer() override;
        void music(unsigned index) override;
        void camera(std::string_view resource) override;
        void lighting(std::string_view resource) override;
        void shadowOff(std::string_view actor) override;
        void endCamera() override;
        void closeMessage() override;
        void speak(unsigned window, unsigned line) override;
        void messageColor(unsigned window, int color) override;
        void closeWindow(unsigned window) override;
        bool messageClosed() const override;
        void finish() override;
        actors::MoveActor* find(std::string_view name) override;
        bool actorExists(std::string_view name) const override;
        void actorCommand(std::string_view name, const ActorCommand& command) override;
        void lookPlayer(actors::MoveActor& target) override;
        void cleaner(actors::MoveActor& actor, actors::MoveActor& head, actors::MoveActor& body) override;
        void beam(actors::MoveActor& target) override;
        void cutBeam() override;
        void sound(actors::MoveActor& actor, unsigned index) override;
        void sound(unsigned index) override;
        void stopSound(unsigned index) override;
        void setFlag(unsigned index) override;
        void effect(std::string_view placement, std::string_view command) override;
        void fullscreen(bool enabled) override;
        void playerEvent(unsigned event) override;
        void releaseActor() override;
        void stopActor() override;
        void stopPlayerMotion() override;
        std::uint32_t ghostTotal() const override;
        void blendCamera() override;
        std::vector<std::uint8_t> resource(std::string_view name) const;
        bool eligible(unsigned event, unsigned elapsedSeconds) const;
        gameassets::Package& package;
        const enemytypes::Catalog& types;
        gameentry::State& entry;
        const roomlocation::Rooms& rooms;
        gameplay::ManagerServices& managers;
        gameplaycamera::CameraOwner& cameraOwner;
        const std::vector<std::uint8_t>& executable;
        MessageWindows& messages;
        gameplaycamera::Follow& follow;
        std::function<void(std::string_view)> requestDemo;
        bool targetCameraActive = false;
        std::array<float, 3> cameraSubject{};
        std::unique_ptr<MessageText> messageText;
        gamearchive::Archive archive;
        std::unique_ptr<EntranceScript> script;
        std::unique_ptr<CameraPath> cameraPath;
        std::unique_ptr<LightPath> lightPath;
        std::array<float, 3> savedUp{}, savedTarget{}, savedPosition{};
        float savedFieldOfView = 0;
        std::uint16_t savedCC = 0, savedCE = 0;
        unsigned cameraFrame = 0, lightFrame = 0;
        unsigned currentEvent = 0;
        std::uint32_t audioFrame = 0;
        bool menuAvailable = true;
        bool fullscreenEnabled = false;
    };
}
