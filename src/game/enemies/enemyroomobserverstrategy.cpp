#include "game/enemies/enemyroomobserverstrategy.h"
#include "game/actors/moveactor.h"
#include "game/actors/characterowner.h"

#include <stdexcept>

namespace {
    class CompletedAction final : public enemystrategy::Strategy {
    public:
        void behavior() override { setNextState(0x100); }
    };
    class ClearDoorBarrier final : public enemystrategy::Strategy {
    public:
        explicit ClearDoorBarrier(const enemystrategy::Factory& factory) : factory(factory) {}
        void initializeBehavior() override { factory.clearGlobalDoorBarrier(); }
    private:
        const enemystrategy::Factory& factory;
    };
    class ChangeMotion final : public enemystrategy::Strategy {
    public:
        void initializeBehavior() override {
            fired = false;
            threshold = 0;
            name = {};
            if (!owner || !owner->source.table || owner->source.row < 0) return;
            const auto& source = owner->source;
            source.table->getValue(source.row, "arg0", threshold);
            source.table->getValue(source.row, "string_arg0", name);
        }

        void behavior() override {
            if (fired || frames <= threshold) return;
            if (!owner) throw std::runtime_error("Original change-motion action has no character owner.");
            if (!name.empty()) dynamic_cast<characterowner::Owner&>(*owner).manager().activateNamed(name);
            fired = true;
        }

    private:
        std::uint32_t threshold = 0;
        std::string_view name;
        bool fired = false;
    };
}

enemystrategy::RoomObserver::RoomObserver(const Factory& factory, const Catalog& catalog) : factory(factory), catalog(catalog) {}

void enemystrategy::RoomObserver::initializeBehavior() {
    if (state == 1) {
        if (!condition) throw std::runtime_error("Original room observer has no condition strategy.");
        condition->setNextState(0);
    } else if (state == 2) {
        if (!actionStrategy) throw std::runtime_error("Original room-observer action services are not bound: action " +
            std::to_string(action) + ", row " + std::to_string(owner ? owner->source.row : -1));
        actionStrategy->setNextState(0);
    }
}

void enemystrategy::RoomObserver::behavior() {
    if (!owner) throw std::runtime_error("Original room observer has no owner.");
    if (state == 0) {
        unsigned selection = 2;
        action = 0;
        const auto& source = owner->source;
        if (source.table && source.row >= 0) {
            source.table->getValue(source.row, "cond_type", selection);
            source.table->getValue(source.row, "do_type", action);
        }
        static constexpr unsigned selectors[]{0x7f, 0x7b, 0x7f, 0x80, 0x35, 0x7f, 0x7c, 0x7d,
            0x7e, 0x81, 0x82, 0x84, 0x86, 0x8e, 0x90, 0x91, 0x96, 0xa9, 0xb3, 0xb4};
        condition.reset();
        condition = factory.construct(catalog.constructor(selection < std::size(selectors) ? selectors[selection] : 0x7f));
        condition->owner = owner;
        condition->context = context;
        actionStrategy.reset();
        if (action == 1) {
            factory.lightRoom(owner->actor().room() >> 24);
        }
        if (action == 0 || action == 1 || action == 2 || action == 4 || action == 5 || action == 6 || action > 12)
            actionStrategy = std::make_unique<CompletedAction>();
        else if (action == 10) actionStrategy = std::make_unique<ChangeMotion>();
        else if (action == 12) actionStrategy = std::make_unique<ClearDoorBarrier>(factory);
        if (actionStrategy) {
            actionStrategy->owner = owner;
            actionStrategy->context = context;
        }
        setNextState(1);
    } else if (state == 1) {
        condition->update();
        if (condition->state == 0x100) setNextState(2);
    } else if (state == 2) {
        actionStrategy->update();
    }
}

std::uint32_t enemystrategy::RoomObserver::playerLeftRoom(characterevent::Event&) {
    const bool reset = owner && owner->actor().room() == 0x2b010428;
    if (reset) setNextState(0);
    return reset;
}
