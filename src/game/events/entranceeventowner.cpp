#include "game/events/entranceeventowner.h"
#include "resources/roomarchivereader.h"
#include "game/events/eventsoundtable.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {
    constexpr unsigned entranceEvent = 48;
    constexpr unsigned firstKeyEvent = 7;
}

gameevent::EntranceOwner::EntranceOwner(gameassets::Package& package, const enemytypes::Catalog& types,
    gameentry::State& entry, const roomlocation::Rooms& rooms, gameplay::ManagerServices& managers,
    gameplaycamera::CameraOwner& camera, const std::vector<std::uint8_t>& executable, MessageWindows& messages, gameplaycamera::Follow& follow) :
    package(package), types(types), entry(entry), rooms(rooms), managers(managers), cameraOwner(camera),
    executable(executable), messages(messages), follow(follow) {}

void gameevent::EntranceOwner::bindPlayerDemo(std::function<void(std::string_view)> request) {
    requestDemo = std::move(request);
}

void gameevent::EntranceOwner::playerDemo(std::string_view name) {
    if (!requestDemo) throw std::runtime_error("Player demo requires its gameplay presentation owner.");
    requestDemo(name);
}

void gameevent::EntranceOwner::bindRoomLight(std::function<void(bool)> change) {
    changeRoomLight = std::move(change);
}

void gameevent::EntranceOwner::playerLight(bool enabled) {
    if (!changeRoomLight) throw std::runtime_error("Room lighting requires the current room owner.");
    changeRoomLight(enabled);
}

void gameevent::EntranceOwner::bindControllerInstructions(std::function<void()> request) {
    requestInstructions = std::move(request);
}

void gameevent::EntranceOwner::controllerInstructions() {
    if (!requestInstructions) throw std::runtime_error("Controller instructions require their gameplay owner.");
    requestInstructions();
}

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
    if (definition->trigger == 0 || (entry.map == 6 && event == 28 && definition->trigger == 1)) return true;
    const auto eventRoom = rooms.resolve(definition->position, 0xffffffff);
    if (definition->trigger == 4) {
        if (eventRoom != player->room() || managers.doors().activeDoor() != -1) return false;
    } else if (definition->trigger == 5) {
        if (((eventRoom ^ player->room()) & 0xff0000) != 0) return false;
    } else throw std::runtime_error("Unsupported entrance event activation conditions.");
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
            if ((event == entranceEvent || event == firstKeyEvent || (entry.map == 2 && event == 1) ||
                (entry.map == 1 && event == 8) ||
                (entry.map == 3 && (event == 9 || event == 10)) ||
                (entry.map == 6 && event == 28)) && eligible(event, elapsedSeconds)) {
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
        auto messageIndex = archive.findNameResource("message" + number + ".csv");
        if (messageIndex < 0) messageIndex = archive.findNameResource("message" + std::to_string(currentEvent) + ".csv");
        if (messageIndex < 0) messageIndex = archive.findNameResource("massage" + std::to_string(currentEvent) + ".csv"); // the training archives misspell the resource name
        if (messageIndex >= 0) {
            messageText = std::make_unique<MessageText>(executable, archive.resource(messageIndex));
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
        messageText.reset();
        archive = {};
    }
}

bool gameevent::EntranceOwner::flag(unsigned index) const { return managers.flags().value(static_cast<std::uint8_t>(index)); }
void gameevent::EntranceOwner::menu(bool enabled) { menuAvailable = enabled; }
bool gameevent::EntranceOwner::menuEnabled() const { return menuAvailable; }
bool gameevent::EntranceOwner::fullscreen() const { return fullscreenEnabled; }
bool gameevent::EntranceOwner::active() const { return script != nullptr; }
bool gameevent::EntranceOwner::cameraActive() const { return cameraPath != nullptr || targetCameraActive; }

void gameevent::EntranceOwner::stopPlayer() {
    managers.playerEvents().send(0, 4);
    managers.playerEvents().player(0)->setVelocity({});
}

void gameevent::EntranceOwner::releasePlayer() { managers.playerEvents().send(0, 5); }

void gameevent::EntranceOwner::music(unsigned index) {
    if (index == 10) managers.music().requestEntrance(audioFrame);
    else if (index == 0) managers.music().requestFirstKey(audioFrame);
    else if (index == 1) managers.music().requestParlor(audioFrame);
    else if (index == 8 || index == 13 || index == 14) {
        // Table 8038EAB4: 8 -> 0x80000008, 13 -> 0x80000027, 14 -> 0x80000026.
        managers.music().requestSequence(index == 8 ? 0x80000008u : index == 13 ? 0x80000027u : 0x80000026u, audioFrame);
    }
    else throw std::runtime_error("Unsupported entrance music request.");
}

void gameevent::EntranceOwner::camera(std::string_view name) {
    if (cameraActive()) endCamera();
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
    if (targetCameraActive) {
        const auto* player = managers.playerEvents().player(0);
        follow.updateTarget(cameraOwner, player->position, cameraSubject, player->room());
        return true;
    }
    if (!cameraPath) return false;
    cameraPath->sample(static_cast<float>(cameraFrame), cameraOwner);
    if (cameraFrame < cameraPath->duration()) ++cameraFrame;
    return true;
}

void gameevent::EntranceOwner::endCamera() {
    if (!cameraActive()) return;
    cameraOwner.up = savedUp;
    cameraOwner.target = savedTarget;
    cameraOwner.position = savedPosition;
    cameraOwner.fieldOfView = savedFieldOfView;
    cameraOwner.fieldCC = savedCC;
    cameraOwner.fieldCE = savedCE;
    cameraOwner.flags &= ~9u;
    cameraPath.reset();
    targetCameraActive = false;
    cameraOwner.callback = 0;
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

void gameevent::EntranceOwner::cleaner(actors::MoveActor& actor, actors::MoveActor& head, actors::MoveActor& body) {
    auto& auxiliary = managers.players().auxiliary;
    if (!auxiliary) throw std::runtime_error("Event cleaner requires its initialized auxiliary owner.");
    auxiliary->attach(actor, head, body);
}

void gameevent::EntranceOwner::beam(actors::MoveActor& target) {
    auto& auxiliary = managers.players().auxiliary;
    if (!auxiliary) throw std::runtime_error("Event beam requires its initialized auxiliary owner.");
    auxiliary->startBeam(0, target);
}

void gameevent::EntranceOwner::cutBeam() { managers.stopAuxiliaryBeam(); }

void gameevent::EntranceOwner::sound(actors::MoveActor& actor, unsigned index) {
    if (index != 0) throw std::runtime_error("Event actor sound requires its original positional routing.");
    managers.playback()->play(soundId(executable, index), actor.position);
}

void gameevent::EntranceOwner::sound(unsigned index) {
    managers.playback()->playGlobal(soundId(executable, index));
}

void gameevent::EntranceOwner::stopSound(unsigned index) {
    managers.playback()->stopSound(soundId(executable, index));
}

void gameevent::EntranceOwner::setFlag(unsigned index) { managers.flags().setOn(static_cast<std::uint8_t>(index)); }
void gameevent::EntranceOwner::effect(std::string_view placement, std::string_view command) { managers.eventEffect(placement, command); }
void gameevent::EntranceOwner::fullscreen(bool enabled) { fullscreenEnabled = enabled; }
void gameevent::EntranceOwner::playerEvent(unsigned event) { managers.playerEvents().send(0, event); }
void gameevent::EntranceOwner::releaseActor() { managers.playerEvents().player(0)->flags &= ~0x10u; }
std::uint32_t gameevent::EntranceOwner::ghostTotal() const {
    return managers.playerEvents().player(0)->view.count;
}

void gameevent::EntranceOwner::stopPlayerMotion() {
    stopPlayer();
    auto& player = *managers.playerEvents().player(0);
    player.setVelocity({});
    player.selectAnimation(0u);
}

void gameevent::EntranceOwner::stopActor() {
    auto& player = *managers.playerEvents().player(0);
    player.flags |= 0x10;
    player.setVelocity({});
    player.selectAnimation(0u);
}

void gameevent::EntranceOwner::blendCamera() {
    if (!cameraActive()) return;
    cameraOwner.flags &= ~9u;
    cameraOwner.callback = 0;
    cameraPath.reset();
    targetCameraActive = false;
}

void gameevent::EntranceOwner::closeMessage() { messages.closeAll(); }
bool gameevent::EntranceOwner::messageClosed() const { return !messages.visible(); }
void gameevent::EntranceOwner::speak(unsigned window, unsigned line) {
    if (!messageText) throw std::runtime_error("Event speech requires its original message resource.");
    messages.speak(window, messageText->line(line));
}
void gameevent::EntranceOwner::messageColor(unsigned window, int color) { messages.color(window, color); }
void gameevent::EntranceOwner::closeWindow(unsigned window) { messages.close(window); }
void gameevent::EntranceOwner::finish() { managers.playerEvents().player(0)->flags &= ~0x10u; }

const scenetransition::Request& gameevent::EntranceOwner::transition() const { return requestedTransition; }
void gameevent::EntranceOwner::wipeKind(std::int32_t kind) { requestedTransition.kind = kind; }
void gameevent::EntranceOwner::wipeTime(std::int32_t duration) { requestedTransition.duration = duration; }
void gameevent::EntranceOwner::mansionMode(bool hidden) { entry.mansionMode = hidden ? 1 : 0; }

void gameevent::EntranceOwner::opening() {
    requestedTransition.destination = 2;
    requestedTransition.opening = true;
}

void gameevent::EntranceOwner::warp(std::int32_t destination) {
    const bool parlor = entry.map == 2 && destination == 1 && requestedTransition.kind == 1 && requestedTransition.duration == 126;
    const bool training = entry.map == 1 && (destination == 3 || destination == 6) && requestedTransition.kind == 2 && requestedTransition.duration == 30;
    const bool trainingExit = entry.map == 3 && (destination == 1 || destination == 6) && requestedTransition.kind == 2 && requestedTransition.duration == 60;
    const bool galleryReturn = entry.map == 6 && destination == 1 && requestedTransition.kind == 2 && requestedTransition.duration == 30;
    if (!parlor && !training && !trainingExit && !galleryReturn)
        throw std::runtime_error("Scene transition requires its original destination and wipe lifecycle.");
    requestedTransition.destination = destination;
}

bool gameevent::EntranceOwner::acknowledgeMessage() {
    return messages.acknowledgeInput(managers.controller().pressed);
}

void gameevent::EntranceOwner::cameraTarget(std::string_view name, std::int32_t height) {
    auto* actor = find(name);
    if (!actor) throw std::runtime_error("Target camera requires its named actor.");
    const auto* player = managers.playerEvents().player(0);
    if (player->type() != 0) throw std::runtime_error("Target camera requires the alternate-player height adjustment.");
    if (cameraActive()) endCamera();
    savedUp = cameraOwner.up;
    savedTarget = cameraOwner.target;
    savedPosition = cameraOwner.position;
    savedFieldOfView = cameraOwner.fieldOfView;
    savedCC = cameraOwner.fieldCC;
    savedCE = cameraOwner.fieldCE;
    cameraSubject = actor->position;
    cameraSubject[1] += static_cast<float>(height);
    follow.beginTarget(cameraOwner);
    targetCameraActive = true;
}

void gameevent::EntranceOwner::bindRandom(std::function<std::uint32_t()> value) { eventRandom = std::move(value); }

void gameevent::EntranceOwner::clearFlag(unsigned index) { managers.flags().setOff(static_cast<std::uint8_t>(index)); }

// 800EAA00 (GENON/GENOFF): the first placement row whose CodeName matches receives "start"/"stop" through the message
// sender, the same path as the EFFECT command.
void gameevent::EntranceOwner::generator(std::string_view name, bool start) {
    managers.eventEffect(name, start ? "start" : "stop");
}
std::uint32_t gameevent::EntranceOwner::random() {
    if (!eventRandom) throw std::runtime_error("Random event jumps require the gameplay random state.");
    return eventRandom();
}

// ACTOR <FREE> (80062EE8 case 0x15): clears the 0x10 stop bit and zeroes the velocity.
void gameevent::EntranceOwner::releaseNamedActor(std::string_view name) {
    if (auto* actor = find(name)) {
        actor->flags &= ~0x10u;
        actor->setVelocity({});
    }
}

// ACTOR <HP> (80062EE8 case 8): the actor's life halfword (+0xFC).
void gameevent::EntranceOwner::setActorLife(std::string_view name, unsigned value) {
    if (auto* actor = find(name)) actor->life = static_cast<std::uint16_t>(value);
}

void gameevent::EntranceOwner::beginChoice(const std::vector<unsigned>& lines) {
    if (!messageText) throw std::runtime_error("Event choices require their original message resource.");
    std::vector<std::string> choices;
    for (auto line : lines) choices.push_back(messageText->line(line));
    messages.beginChoice(std::move(choices));
}

int gameevent::EntranceOwner::choiceResult() { return messages.choiceInput(managers.controller().pressed); }
void gameevent::EntranceOwner::endChoice() { messages.endChoice(); }
