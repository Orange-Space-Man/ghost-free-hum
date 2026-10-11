#include "resources/glme01addresses.h"
#include "game/enemies/enemystrategyfactory.h"
#include "game/enemies/enemynomovestrategy.h"
#include "game/enemies/enemypathcondition.h"
#include "game/enemies/enemygeneratordoorstrategy.h"
#include "game/enemies/enemyitemstrategy.h"
#include "game/enemies/enemykeystrategy.h"
#include "game/enemies/enemyheartstrategy.h"
#include "game/enemies/enemynegativestrategy.h"
#include "game/enemies/enemyjewelstrategy.h"
#include "game/enemies/enemygoldstrategy.h"
#include "game/enemies/enemypearlstrategy.h"
#include "game/enemies/enemycrownstrategy.h"
#include "game/enemies/enemyelementjewelstrategy.h"
#include "game/enemies/enemyheartescapestrategy.h"
#include "game/enemies/enemynegativerandomstrategy.h"
#include "game/enemies/enemybombstrategy.h"
#include "game/enemies/enemybarrierstrategy.h"
#include "game/enemies/enemybatgeneratorstrategy.h"
#include "game/enemies/enemygoldratgeneratorstrategy.h"
#include "game/enemies/enemyfiregeneratorstrategy.h"
#include "game/enemies/enemyfakedoorstrategy.h"
#include "game/enemies/enemyroomobserverstrategy.h"

#include <sstream>
#include <stdexcept>

namespace {
    class Spear final : public enemystrategy::Strategy {};
}

enemystrategy::Factory::Factory(enemies::Runtime& runtime, const gameflags::State& flags,
    const mission::Resources& resources) : runtime(runtime), flags(flags), resources(resources) {}

std::unique_ptr<enemystrategy::Strategy> enemystrategy::Factory::construct(std::uint32_t constructor) const {
    switch (constructor) {
    case glme01::barrierFactory:
        if (!barriers) throw std::runtime_error("Original door-barrier services are not bound.");
        return std::make_unique<Barrier>(*barriers);
    case glme01::batGeneratorFactory:
    case glme01::alternateBatGeneratorFactory:
        if (!fire) throw std::runtime_error("Original generator room services are not bound.");
        return std::make_unique<BatGenerator>(runtime, fire->rooms, constructor == glme01::alternateBatGeneratorFactory);
    case glme01::goldRatGeneratorFactory: return std::make_unique<GoldRatGenerator>(runtime);
    case glme01::fireGeneratorFactory:
        if (!fire) throw std::runtime_error("Original fire-generator services are not bound.");
        return std::make_unique<FireGenerator>(*fire);
    case glme01::fakeDoorFactory:
        if (!ambient) throw std::runtime_error("Original fake-door room color is not bound.");
        return std::make_unique<FakeDoor>(*ambient);
    case glme01::roomObserverFactory:
        if (!catalog) throw std::runtime_error("Original observer strategy catalog is not bound.");
        return std::make_unique<RoomObserver>(*this, *catalog);
    case glme01::bombFactory:
        if (!items) throw std::runtime_error("Original bomb services are not bound.");
        return std::make_unique<Bomb>(*items, *this);
    case glme01::heartEscapeFactory:
        if (!items) throw std::runtime_error("Original escaping heart services are not bound.");
        return std::make_unique<HeartEscape>(*items, *this);
    case glme01::negativeRandomFactory:
        if (!items) throw std::runtime_error("Original random mushroom services are not bound.");
        return std::make_unique<NegativeRandom>(*items, *this);
    case glme01::elementJewelFactory:
        if (!items) throw std::runtime_error("Original EnElementJewel services are not bound.");
        return std::make_unique<ElementJewel>(*items, *this);
    case glme01::goldFactory:
        if (!items) throw std::runtime_error("Original EnGold services are not bound.");
        return std::make_unique<Gold>(*items, *this);
    case glme01::pearlFactory:
        if (!items) throw std::runtime_error("Original EnPearl services are not bound.");
        return std::make_unique<Pearl>(*items, *this);
    case glme01::crownFactory:
        if (!items) throw std::runtime_error("Original EnCrown services are not bound.");
        return std::make_unique<Crown>(*items, *this);
    case glme01::jewelFactory:
        if (!items) throw std::runtime_error("Original EnJewel services are not bound.");
        return std::make_unique<Jewel>(*items, *this);
    case glme01::negativeItemFactory:
        if (!items) throw std::runtime_error("Original EnNegativeItem services are not bound.");
        return std::make_unique<Negative>(*items, *this);
    case glme01::heartItemFactory:
        if (!items) throw std::runtime_error("Original EnHeartItem services are not bound.");
        return std::make_unique<Heart>(*items, *this);
    case glme01::keyFactory:
        if (!items) throw std::runtime_error("Original EnKey services are not bound.");
        return std::make_unique<Key>(*items, *this);
    case glme01::itemFactory:
        if (!items) throw std::runtime_error("Original EnItem services are not bound.");
        return std::make_unique<Item>(*items, *this);
    case glme01::noMoveFactory: return std::make_unique<NoMove>();
    case glme01::spearFactory: return std::make_unique<Spear>();
    case glme01::pathConditionFactory: return std::make_unique<PathCondition>(resources);
    case glme01::generatorDoorFactory: return std::make_unique<GeneratorDoor>();
    case glme01::immediateConditionFactory: return std::make_unique<Condition>(ConditionKind::Immediate, runtime, flags);
    case glme01::roomEmptyConditionFactory: return std::make_unique<Condition>(ConditionKind::RoomEmpty, runtime, flags);
    case glme01::roomClearedConditionFactory: return std::make_unique<Condition>(ConditionKind::RoomCleared, runtime, flags);
    case glme01::playerInRoomConditionFactory: return std::make_unique<Condition>(ConditionKind::PlayerInRoom, runtime, flags);
    case glme01::namedAbsentConditionFactory: return std::make_unique<Condition>(ConditionKind::NamedAbsent, runtime, flags);
    case glme01::occupiedUnclearedRoomConditionFactory: return std::make_unique<Condition>(ConditionKind::OccupiedUnclearedRoom, runtime, flags);
    case glme01::visibleFlagConditionFactory: return std::make_unique<Condition>(ConditionKind::VisibleFlag, runtime, flags);
    case glme01::flagOnConditionFactory: return std::make_unique<Condition>(ConditionKind::FlagOn, runtime, flags);
    case glme01::flagOffConditionFactory: return std::make_unique<Condition>(ConditionKind::FlagOff, runtime, flags);
    default: {
        std::ostringstream message;
        message << "Original strategy factory is not ported: " << std::hex << constructor;
        throw std::runtime_error(message.str());
    }
    }
}

void enemystrategy::Factory::bindItems(ItemEnvironment* environment) { items = environment; }

void enemystrategy::Factory::bindPlacement(const Catalog& value, BarrierEnvironment& barrierServices,
    FireGeneratorEnvironment& fireServices, const std::array<std::uint8_t, 4>& roomAmbient) {
    catalog = &value;
    barriers = &barrierServices;
    fire = &fireServices;
    ambient = &roomAmbient;
}

std::uint32_t enemystrategy::Factory::random() const { return runtime.random(); }
