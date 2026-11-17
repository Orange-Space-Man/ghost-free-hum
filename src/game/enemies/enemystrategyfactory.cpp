#include "resources/glme01addresses.h"
#include "game/enemies/enemystrategyfactory.h"
#include "game/enemies/enemynomovestrategy.h"
#include "game/enemies/enemylabprofessorstrategy.h"
#include "game/enemies/enemytraininggeneratorstrategy.h"
#include "game/enemies/enemytrainingservices.h"
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
#include "game/enemies/enemyratgeneratorstrategy.h"
#include "game/enemies/enemytelesageneratorstrategy.h"
#include "game/enemies/enemyfiregeneratorstrategy.h"
#include "game/enemies/enemyfakedoorstrategy.h"
#include "game/enemies/enemyroomobserverstrategy.h"
#include "game/enemies/enemyghostparenthost.h"
#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"
#include "game/world/dooranimationpose.h"
#include "game/effects/particleeffectfactory.h"

#include <sstream>
#include <stdexcept>

namespace {
    class Spear final : public enemystrategy::Strategy {};
    class Kinopio final : public enemystrategy::Strategy {
    public:
        Kinopio(enemies::Runtime& runtime, particles::EffectFactory& effects) : runtime(runtime), effects(effects) {}
        ~Kinopio() override { if (tears) tears->release(); }
        void behavior() override {
            if (!owner) throw std::runtime_error("Kinopio strategy requires its character owner.");
            auto& actor = dynamic_cast<actors::MoveActor&>(owner->actor());
            if (runtime.roomCleared(actor.room() >> 24) || !actor.model) {
                if (tears) { tears->release(); tears = nullptr; }
                return;
            }
            const auto matrix = dooranimation::concatenate(actor.matrix, actor.model->matrix(0, 0x18));
            const std::array<float, 3> position{matrix[3], matrix[7], matrix[11]};
            if (!tears) tears = effects.create(14, position, {});
            else tears->position = position;
        }
    private:
        enemies::Runtime& runtime;
        particles::EffectFactory& effects;
        particles::Emitter* tears = nullptr;
    };
}

enemystrategy::Factory::Factory(enemies::Runtime& runtime, gameflags::State& flags,
    const mission::Resources& resources) : runtime(runtime), flags(flags), resources(resources) {}

void enemystrategy::Factory::bindRoomLight(std::function<void(unsigned)> callback) { roomLight = std::move(callback); }
void enemystrategy::Factory::bindGlobalDoorBarrierClear(std::function<void()> callback) {
    globalDoorBarrierClear = std::move(callback);
}
void enemystrategy::Factory::clearGlobalDoorBarrier() const {
    if (!globalDoorBarrierClear) throw std::runtime_error("Original door-lock action has no door manager binding.");
    globalDoorBarrierClear();
}
void enemystrategy::Factory::bindKinopioEffects(particles::EffectFactory& effects) { kinopioEffects = &effects; }
void enemystrategy::Factory::lightRoom(unsigned room) const {
    if (!roomLight) throw std::runtime_error("Original room light action has no scene binding.");
    roomLight(room);
}

std::unique_ptr<enemystrategy::Strategy> enemystrategy::Factory::construct(std::uint32_t constructor) const {
    switch (constructor) {
    case glme01::trainingGeneratorFactory: {
        auto strategy = std::make_unique<TrainingGenerator>(flags);
        if (training) strategy->bindServices(training->bind(*strategy));
        return strategy;
    }
    case glme01::ghostParentFactory:
        if (!ghost) throw std::runtime_error("Original ghost parent services are not bound.");
        return ghostparent::construct(*ghost);
    case glme01::barrierFactory:
        if (!barriers) throw std::runtime_error("Original door-barrier services are not bound.");
        return std::make_unique<Barrier>(*barriers);
    case glme01::batGeneratorFactory:
    case glme01::alternateBatGeneratorFactory:
        if (!fire) throw std::runtime_error("Original generator room services are not bound.");
        return std::make_unique<BatGenerator>(runtime, fire->rooms, constructor == glme01::alternateBatGeneratorFactory);
    case glme01::goldRatGeneratorFactory: return std::make_unique<GoldRatGenerator>(runtime);
    case glme01::telesaGeneratorFactory: return std::make_unique<TelesaGenerator>(runtime);
    case glme01::ratGeneratorFactory:
        if (!pose) throw std::runtime_error("Original rat-generator pose services are not bound.");
        return std::make_unique<RatGenerator>(runtime, *pose);
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
    case glme01::labProfessorFactory: return std::make_unique<LabProfessor>();
    case 0x800f02dc:
        if (!kinopioEffects) throw std::runtime_error("Kinopio requires the original particle effect owner.");
        return std::make_unique<Kinopio>(runtime, *kinopioEffects);
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

void enemystrategy::Factory::bindTraining(TrainingEnvironment& environment) { training = &environment; }

void enemystrategy::Factory::bindItems(ItemEnvironment* environment) { items = environment; }
void enemystrategy::Factory::bindPose(const actormodel::InitialPose& value) { pose = &value; }
void enemystrategy::Factory::bindGhostParent(ghostparent::Environment& environment) { ghost = &environment; }

void enemystrategy::Factory::bindPlacement(const Catalog& value, BarrierEnvironment& barrierServices,
    FireGeneratorEnvironment& fireServices, const std::array<std::uint8_t, 4>& roomAmbient) {
    catalog = &value;
    barriers = &barrierServices;
    fire = &fireServices;
    ambient = &roomAmbient;
}

std::uint32_t enemystrategy::Factory::random() const { return runtime.random(); }
