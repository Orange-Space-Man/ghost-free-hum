#include "game/events/entranceeventowner.h"
#include "resources/roomarchivereader.h"

#include <algorithm>
#include <stdexcept>

namespace {
    constexpr unsigned entranceEvent = 48;
    constexpr unsigned firstKeyEvent = 7;
}

gameevent::EntranceOwner::EntranceOwner(gameassets::Package& package, const enemytypes::Catalog& types,
    gameentry::State& entry, const roomlocation::Rooms& rooms, gameplay::ManagerServices& managers,
    gameplaycamera::CameraOwner& camera) : package(package), types(types), entry(entry), rooms(rooms),
    managers(managers), cameraOwner(camera) {}

bool gameevent::EntranceOwner::eligible(unsigned event, unsigned elapsedSeconds) const {
    const auto& events = managers.missionEvents();
    const auto* definition = events.definition(event);
    if (!definition) return false;
    const auto& condition = managers.missionEvents().condition(event);
    if (condition.flags & 0x49) return false;
    for (const auto& other : events.conditions()) if ((other.flags & 0x101) == 0x101) return false;
    if ((condition.appearFlag && !flag(condition.appearFlag)) || flag(condition.disappearFlag) ||
        checkTime(condition, static_cast<std::int32_t>(elapsedSeconds), 0) != 1) return false;
    const auto* player = managers.playerEvents().player(0);
    if (!player || !player->view.life) return false;
    if (definition->trigger != 5)
        throw std::runtime_error("Unsupported entrance event activation conditions.");
    if (((rooms.resolve(definition->position, 0) ^ player->room()) & 0xff0000) != 0) return false;
    if (condition.characterType != 0xffffffff) {
        bool found = false;
        for (const auto& slot : managers.enemies().slots())
            if (slot.state == 2 && slot.owner && slot.owner->actor().type() == condition.characterType) found = true;
        if (!found) return false;
    }
    const float x = player->position[0] - definition->position[0];
    const float z = player->position[2] - definition->position[2];
    return x * x + z * z < static_cast<float>(definition->area * definition->area);
}

std::vector<std::uint8_t> gameevent::EntranceOwner::resource(std::string_view name) const {
    const auto index = archive.findNameResource(name);
    if (index < 0) throw std::runtime_error("Missing entrance event resource: " + std::string(name));
    return archive.resource(index);
}

void gameevent::EntranceOwner::update(unsigned elapsedSeconds, std::uint32_t frame) {
    audioFrame = frame;
    if (!script) {
        currentEvent = 0;
        for (const auto& condition : managers.missionEvents().conditions()) {
            const auto event = static_cast<unsigned>(condition.eventNumber);
            if ((event == entranceEvent || event == firstKeyEvent) && eligible(event, elapsedSeconds)) {
                currentEvent = event;
                break;
            }
        }
        if (!currentEvent) return;
        std::vector<std::uint8_t> compressed, bytes;
        std::string error;
        const auto number = (currentEvent < 10 ? "0" : "") + std::to_string(currentEvent);
        if (!package.read("files/Event/event" + number + ".szp", compressed, error) ||
            !roomarchive::decompress(compressed, bytes, error) || !archive.read(bytes, error))
            throw std::runtime_error(error);
        const auto text = resource("event" + number + ".txt");
        const auto messageIndex = archive.findNameResource("message" + number + ".csv");
        if (messageIndex >= 0) {
            const auto messages = archive.resource(messageIndex);
            if (std::any_of(messages.begin(), messages.end(), [](auto value) { return value != '\r' && value != '\n'; }))
                throw std::runtime_error("Event requires its original message presentation owner.");
        }
        script = std::make_unique<EntranceScript>(std::string(text.begin(), text.end()));
        auto& condition = managers.missionEvents().condition(currentEvent);
        condition.flags |= 0x11;
        if (condition.flags & 0x200) {
            playerevents::Payload payload;
            payload.word(0x28, 30);
            managers.playerEvents().send(0, 6, &payload);
        }
        return;
    }
    if (lightPath) lightPath->sample(static_cast<float>(lightFrame++), managers.lights());
    const bool completed = script->update(*this);
    managers.missionEvents().condition(currentEvent).flags |= 0x20;
    if (completed) {
        lightPath.reset();
        managers.flags().setOff(0x5e);
        managers.playerEvents().player(0)->flags &= ~0xc00u;
        managers.missionEvents().complete(currentEvent, entry.inventory.at(0x20 + currentEvent));
        script.reset();
        archive = {};
    }
}

bool gameevent::EntranceOwner::flag(unsigned index) const { return managers.flags().value(static_cast<std::uint8_t>(index)); }
void gameevent::EntranceOwner::menu(bool enabled) { menuAvailable = enabled; }
bool gameevent::EntranceOwner::menuEnabled() const { return menuAvailable; }
bool gameevent::EntranceOwner::fullscreen() const { return fullscreenEnabled; }
bool gameevent::EntranceOwner::active() const { return script != nullptr; }
bool gameevent::EntranceOwner::cameraActive() const { return cameraPath != nullptr; }

void gameevent::EntranceOwner::stopPlayer() {
    managers.playerEvents().send(0, 4);
    managers.playerEvents().player(0)->setVelocity({});
}

void gameevent::EntranceOwner::releasePlayer() { managers.playerEvents().send(0, 5); }

void gameevent::EntranceOwner::music(unsigned index) {
    if (index == 10) managers.music().requestEntrance(audioFrame);
    else if (index == 0) managers.music().requestFirstKey(audioFrame);
    else throw std::runtime_error("Unsupported entrance music request.");
}

void gameevent::EntranceOwner::camera(std::string_view name) {
    if (cameraPath) endCamera();
    savedUp = cameraOwner.up;
    savedTarget = cameraOwner.target;
    savedPosition = cameraOwner.position;
    savedFieldOfView = cameraOwner.fieldOfView;
    savedCC = cameraOwner.fieldCC;
    savedCE = cameraOwner.fieldCE;
    cameraPath = std::make_unique<CameraPath>(resource(name));
    cameraFrame = 0;
    cameraOwner.flags |= 1;
    cameraOwner.visibilityMode = 2;
    cameraPath->sample(0, cameraOwner);
}

bool gameevent::EntranceOwner::updateCamera() {
    if (!cameraPath) return false;
    cameraPath->sample(static_cast<float>(cameraFrame), cameraOwner);
    if (cameraFrame < cameraPath->duration()) ++cameraFrame;
    return true;
}

void gameevent::EntranceOwner::endCamera() {
    if (!cameraPath) return;
    cameraOwner.up = savedUp;
    cameraOwner.target = savedTarget;
    cameraOwner.position = savedPosition;
    cameraOwner.fieldOfView = savedFieldOfView;
    cameraOwner.fieldCC = savedCC;
    cameraOwner.fieldCE = savedCE;
    cameraOwner.flags &= ~9u;
    cameraPath.reset();
}

void gameevent::EntranceOwner::lighting(std::string_view name) {
    lightPath = std::make_unique<LightPath>(resource(name));
    lightFrame = 0;
}

void gameevent::EntranceOwner::shadowOff(std::string_view name) {
    ActorCommand command;
    command.operation = ActorOperation::ShadowOff;
    actorCommand(name, command);
}

actors::MoveActor* gameevent::EntranceOwner::find(std::string_view name) {
    if (name == "demo_luigi") return managers.playerEvents().player(0);
    for (const auto& slot : managers.enemies().slots()) {
        if (slot.state != 2 || !slot.owner) continue;
        const auto& source = slot.owner->source;
        std::string_view codeName;
        if (!source.table || source.row < 0 || !source.table->getValue(source.row, "CodeName", codeName))
            codeName = types.name(slot.owner->actor().type());
        if (codeName == name) return managers.actors().actor(slot.owner->actor().id());
    }
    return nullptr;
}

bool gameevent::EntranceOwner::actorExists(std::string_view name) const {
    return const_cast<EntranceOwner*>(this)->find(name) != nullptr;
}

void gameevent::EntranceOwner::actorCommand(std::string_view name, const ActorCommand& command) {
    if (auto* actor = find(name)) applyActorCommand(*actor, command, *this);
}

void gameevent::EntranceOwner::lookPlayer(actors::MoveActor& target) {
    playerevents::Payload payload;
    payload.look(&target.position);
    managers.playerEvents().send(0, 7, &payload);
}

void gameevent::EntranceOwner::sound(actors::MoveActor& actor, unsigned index) {
    if (index != 0) throw std::runtime_error("Event actor sound requires its original sound-table entry.");
    managers.playback()->play(0x1827, actor.position);
}

void gameevent::EntranceOwner::sound(unsigned index) {
    if (index != 6 && index != 7) throw std::runtime_error("Event sound requires its original sound-table entry.");
    managers.playback()->playGlobal(index == 6 ? 0x4870 : 0x4871);
}

void gameevent::EntranceOwner::setFlag(unsigned index) { managers.flags().setOn(static_cast<std::uint8_t>(index)); }
void gameevent::EntranceOwner::effect(std::string_view placement, std::string_view command) { managers.eventEffect(placement, command); }
void gameevent::EntranceOwner::fullscreen(bool enabled) { fullscreenEnabled = enabled; }
void gameevent::EntranceOwner::playerEvent(unsigned event) { managers.playerEvents().send(0, event); }
void gameevent::EntranceOwner::releaseActor() { managers.playerEvents().player(0)->flags &= ~0x10u; }

void gameevent::EntranceOwner::blendCamera() {
    if (!cameraPath) return;
    cameraOwner.flags &= ~9u;
    cameraOwner.callback = 0;
    cameraPath.reset();
}

// Both supported events have no window commands or message text.
void gameevent::EntranceOwner::closeMessage() {}
bool gameevent::EntranceOwner::messageClosed() const { return true; }
void gameevent::EntranceOwner::finish() { managers.playerEvents().player(0)->flags &= ~0x10u; }
