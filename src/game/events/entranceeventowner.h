#pragma once

#include "game/events/entrancescript.h"
#include "game/events/eventpathanimation.h"
#include "game/scene/gameplaymanagerservices.h"

namespace gameevent {
    class EntranceOwner final : private EntranceServices {
    public:
        EntranceOwner(gameassets::Package& package, const enemytypes::Catalog& types,
            gameentry::State& entry, const roomlocation::Rooms& rooms, gameplay::ManagerServices& managers,
            gameplaycamera::CameraOwner& camera);
        void update(unsigned elapsedSeconds, std::uint32_t audioFrame);
        bool updateCamera();
        bool menuEnabled() const;
        bool active() const;
        bool cameraActive() const;

    private:
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
        bool messageClosed() const override;
        void finish() override;
        std::vector<std::uint8_t> resource(std::string_view name) const;
        bool eligible(unsigned elapsedSeconds) const;
        gameassets::Package& package;
        const enemytypes::Catalog& types;
        gameentry::State& entry;
        const roomlocation::Rooms& rooms;
        gameplay::ManagerServices& managers;
        gameplaycamera::CameraOwner& cameraOwner;
        gamearchive::Archive archive;
        std::unique_ptr<EntranceScript> script;
        std::unique_ptr<CameraPath> cameraPath;
        std::unique_ptr<LightPath> lightPath;
        std::array<float, 3> savedUp{}, savedTarget{}, savedPosition{};
        float savedFieldOfView = 0;
        std::uint16_t savedCC = 0, savedCE = 0;
        unsigned cameraFrame = 0, lightFrame = 0;
        std::uint32_t audioFrame = 0;
        bool menuAvailable = true;
    };
}
