#include "game/enemies/enemyconditionstrategy.h"
#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"

#include <stdexcept>

enemystrategy::Condition::Condition(ConditionKind kind, enemies::Runtime& runtime, const gameflags::State& flags) :
    kind(kind), runtime(runtime), flags(flags) {}

void enemystrategy::Condition::initializeBehavior() {
    if (kind != ConditionKind::FlagOn && kind != ConditionKind::FlagOff) return;
    if (!owner) throw std::runtime_error("Original condition strategy has no owner.");
    flag = 0;
    const auto& source = owner->source;
    if (source.table && source.row >= 0) source.table->getValue(source.row, "cond_arg0", flag);
}

void enemystrategy::Condition::behavior() {
    if (kind == ConditionKind::Immediate) {
        setNextState(0x100);
        return;
    }
    if (kind == ConditionKind::FlagOn || kind == ConditionKind::FlagOff) {
        if (flags.value(static_cast<std::uint8_t>(flag)) == (kind == ConditionKind::FlagOn)) setNextState(0x100);
        return;
    }
    if (!owner) throw std::runtime_error("Original condition strategy has no owner.");
    auto& character = dynamic_cast<characterowner::Owner&>(*owner);
    auto& manager = character.manager();
    const auto room = character.actor().room();
    bool ready = false;
    switch (kind) {
    case ConditionKind::RoomEmpty:
        ready = manager.countRoom(room) == 0;
        break;
    case ConditionKind::RoomCleared:
        ready = runtime.roomCleared(room >> 24);
        break;
    case ConditionKind::PlayerInRoom:
        ready = runtime.playerRoom(0) == room;
        break;
    case ConditionKind::NamedAbsent: {
        const auto& source = character.source;
        std::string_view name;
        ready = source.table && source.row >= 0 && source.table->getValue(source.row, "cond_string_arg0", name) &&
            manager.countNamed(name) == 0;
        break;
    }
    case ConditionKind::OccupiedUnclearedRoom:
        ready = runtime.playerRoom(0) == room && manager.countRoom(room) != 0 && !runtime.roomCleared(room >> 24);
        break;
    case ConditionKind::VisibleFlag:
        ready = runtime.roomVisible(dynamic_cast<actors::MoveActor&>(character.actor()).resolvePositionRoom(), 0) && flags.value(0x5b);
        break;
    default:
        break;
    }
    if (ready) setNextState(0x100);
}
