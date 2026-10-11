#pragma once

#include "game/enemies/enemycreationcatalog.h"
#include "game/enemies/enemycreationrequests.h"
#include "game/events/jmpmessagesender.h"
#include "game/actors/characterevent.h"

#include <memory>

namespace enemies {
    class Manager;

    class Actor {
    public:
        virtual ~Actor() = default;
        virtual unsigned id() const = 0;
        virtual unsigned type() const = 0;
        virtual unsigned room() const = 0;
        virtual void setPosition(const std::array<float, 3>& value) = 0;
        virtual void setRotation(const std::array<std::int16_t, 3>& value) = 0;
        virtual void visibility(std::uint8_t primary, std::uint8_t secondary) = 0;
    };

    class Owner {
    public:
        virtual ~Owner() = default;
        virtual void attach(Actor& actor, Manager& manager) = 0;
        virtual Actor& actor() = 0;
        virtual bool activate(unsigned type) = 0;
        virtual void deactivate() = 0;
        virtual void beginStrategy() = 0;
        virtual void replaceStrategy(unsigned type) = 0;
        virtual void nextStrategyState(std::uint16_t state) = 0;
        virtual void update() = 0;
        virtual std::uint32_t event(characterevent::Event& event) = 0;
        std::uint32_t event(unsigned message, std::array<std::uint8_t, 381>* types = nullptr);
        jmp::ToolDataRef source;
    };

    class Runtime {
    public:
        virtual ~Runtime() = default;
        virtual std::unique_ptr<Owner> constructOwner(unsigned constructor, unsigned type) = 0;
        virtual void resetActorIndex() = 0;
        virtual Actor& constructActor(unsigned type, unsigned capacity) = 0;
        virtual Owner& absentOwner() = 0;
        virtual unsigned resolveRoom(const std::array<float, 3>& position) = 0;
        virtual bool roomVisible(unsigned room, unsigned buffer) = 0;
        virtual bool doorOpened(unsigned player, unsigned door) = 0;
        virtual bool typeCaptured(unsigned player, unsigned type) = 0;
        virtual bool roomCleared(unsigned roomGroup) = 0;
        virtual std::uint32_t random() = 0;
        virtual std::array<float, 3> playerPosition(unsigned player) = 0;
        virtual unsigned playerRoom(unsigned player) = 0;
        virtual bool playerSpecialState(unsigned player) = 0;
    };

    struct Slot {
        std::unique_ptr<Owner> owner;
        unsigned state = 0, category = 0;
        bool visible = false;
    };

    struct AppearPoint {
        unsigned actor = 0;
        jmp::ToolDataRef source;
        std::array<float, 3> position{};
        unsigned room = 0;
    };

    class Manager : public jmp::MessageReceiver {
    public:
        Manager(const enemytypes::Catalog& types, const enemycreation::Catalog& creation, Runtime& runtime,
            unsigned players, unsigned mapMode);
        ~Manager() override;
        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;
        void attachCharacterInfo(const jmp::ToolData& table, unsigned appearPoint);
        jmp::ToolDataRef findAppearPoint(unsigned point) const;
        void initializeActors();
        bool create(jmp::ToolDataRef& entry) override;
        bool remove(jmp::ToolDataRef& entry) override;
        bool message(jmp::ToolDataRef& entry, const char* command) override;
        Slot* activate(unsigned type, const jmp::ToolDataRef& entry);
        unsigned createActor(unsigned type, int strategy);
        void deactivate(unsigned slot);
        void activateNamed(std::string_view name);
        void update(unsigned slot);
        void updateVisibility();
        bool anyPlayerSpecialState();
        unsigned countNamed(std::string_view name) const;
        unsigned countRoom(unsigned room);
        std::array<std::uint8_t, 262> resourceMask();
        Owner& owner(int slot);
        const std::array<Slot, 128>& slots() const;
        const enemycreation::Requests& pending() const;
        const AppearPoint& appearPoint(unsigned player) const;
        std::array<float, 3> playerPosition(unsigned player);
        unsigned playerRoom(unsigned player);
        bool actorsInitialized() const;
        unsigned playerCount() const;
        Owner* activeOwner(const Actor& actor) const;
        bool restorePlacement(Owner& owner);
        int nearPlayer(const std::array<float, 3>& position, float range);
        int nearestPlayer(const std::array<float, 3>& position, std::array<float, 3>* displacement = nullptr);
        bool roomVisible(unsigned room);

    private:
        const enemytypes::Catalog& types;
        const enemycreation::Catalog& creation;
        Runtime& runtime;
        unsigned players, mapMode;
        std::array<Slot, 128> entries{};
        enemycreation::Requests requests;
        std::unique_ptr<jmp::ToolData> characters;
        std::array<AppearPoint, 2> appear{};
        bool initialized = false;
        bool isPlayer(const jmp::ToolDataRef& source) const;
        Slot* findActive(const jmp::ToolDataRef& source);
        void construct(unsigned slot, unsigned type);
        bool reactivate(Slot& slot, unsigned type);
        void deactivate(Slot& slot);
        void place(Owner& owner);
    };
}
