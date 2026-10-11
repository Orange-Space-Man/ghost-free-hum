#include "game/enemies/enemyroomobserverstrategy.h"
#include "game/actors/moveactor.h"

#include <stdexcept>

enemystrategy::RoomObserver::RoomObserver(const Factory& factory, const Catalog& catalog) : factory(factory), catalog(catalog) {}

void enemystrategy::RoomObserver::initializeBehavior() {
    if (state == 1) {
        if (!condition) throw std::runtime_error("Original room observer has no condition strategy.");
        condition->setNextState(0);
    } else if (state == 2) {
        throw std::runtime_error("Original room-observer action services are not bound.");
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
        setNextState(1);
    } else if (state == 1) {
        condition->update();
        if (condition->state == 0x100) setNextState(2);
    } else if (state == 2) {
        throw std::runtime_error("Original room-observer action services are not bound.");
    }
}

std::uint32_t enemystrategy::RoomObserver::playerLeftRoom(characterevent::Event&) {
    const bool reset = owner && owner->actor().room() == 0x2b010428;
    if (reset) setNextState(0);
    return reset;
}
