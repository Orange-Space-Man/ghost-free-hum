#include "game/events/entrancescript.h"

#include <charconv>
#include <stdexcept>
#include <utility>

namespace {
    void unsupportedService() {
        throw std::runtime_error("Event command requires an unbound scene service.");
    }
}

bool gameevent::EntranceServices::actorExists(std::string_view) const { return true; }
void gameevent::EntranceServices::actorCommand(std::string_view, const ActorCommand&) { unsupportedService(); }
void gameevent::EntranceServices::setFlag(unsigned) { unsupportedService(); }
void gameevent::EntranceServices::sound(unsigned) { unsupportedService(); }
void gameevent::EntranceServices::effect(std::string_view, std::string_view) { unsupportedService(); }
void gameevent::EntranceServices::fullscreen(bool) { unsupportedService(); }
void gameevent::EntranceServices::playerEvent(unsigned) { unsupportedService(); }
void gameevent::EntranceServices::releaseActor() { unsupportedService(); }
void gameevent::EntranceServices::blendCamera() { unsupportedService(); }
void gameevent::EntranceServices::speak(unsigned, unsigned) { unsupportedService(); }
void gameevent::EntranceServices::messageColor(unsigned, int) { unsupportedService(); }
void gameevent::EntranceServices::closeWindow(unsigned) { unsupportedService(); }

gameevent::EntranceScript::EntranceScript(std::string source) : source(std::move(source)) {}

std::string_view gameevent::EntranceScript::token(char begin, char end) {
    const auto first = source.find(begin, cursor);
    const auto last = first == std::string::npos ? first : source.find(end, first + 1);
    if (last == std::string::npos) throw std::runtime_error("Truncated entrance event command.");
    cursor = last + 1;
    return std::string_view(source).substr(first + 1, last - first - 1);
}

unsigned gameevent::EntranceScript::number() {
    return static_cast<unsigned>(integer());
}

std::int32_t gameevent::EntranceScript::integer() {
    const auto value = token('(', ')');
    std::int32_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        throw std::runtime_error("Invalid entrance event number.");
    return result;
}

void gameevent::EntranceScript::actor(EntranceServices& services) {
    const auto name = token('"', '"');
    const auto command = token('<', '>');
    if (!services.actorExists(name)) return;
    if (command == "SHADOWOFF") {
        services.shadowOff(name);
        return;
    }
    ActorCommand call{};
    if (command == "ACT" || command == "LOOK" || command == "PATH") {
        call.operation = command == "ACT" ? ActorOperation::Animation :
            command == "LOOK" ? ActorOperation::Look : ActorOperation::Path;
        call.argument = token('"', '"');
    } else if (command == "POS") {
        call.operation = ActorOperation::Position;
        for (auto& coordinate : call.position) coordinate = static_cast<float>(integer());
    } else if (command == "ALPHA" || command == "ALPHAF" || command == "SE") {
        call.operation = command == "SE" ? ActorOperation::Sound :
            command == "ALPHAF" ? ActorOperation::FixedAlpha : ActorOperation::Alpha;
        call.value = number() & (command == "SE" ? 65535 : 255);
    } else if (command == "STOP") call.operation = ActorOperation::Stop;
    else if (command == "START") call.operation = ActorOperation::Start;
    else if (command == "COLON") call.operation = ActorOperation::CollisionOn;
    else if (command == "COLOFF") call.operation = ActorOperation::CollisionOff;
    else if (command == "SHADOWON") call.operation = ActorOperation::ShadowOn;
    else if (command == "BODYON") call.operation = ActorOperation::BodyOn;
    else if (command == "BODYOFF") call.operation = ActorOperation::BodyOff;
    else if (command == "MASKON") call.operation = ActorOperation::MaskOn;
    else if (command == "MASKOFF") call.operation = ActorOperation::MaskOff;
    else if (command == "DISPON") call.operation = ActorOperation::DisplayOn;
    else if (command == "DISPOFF") call.operation = ActorOperation::DisplayOff;
    else throw std::runtime_error("Unsupported event actor command: " + std::string(command));
    services.actorCommand(name, call);
}

bool gameevent::EntranceScript::update(EntranceServices& services) {
    if (state == 0) {
        if (!completed) services.finish();
        completed = true;
        return true;
    }
    if (state == 9) {
        if (!services.messageClosed()) return false;
        state = 0;
        return false;
    }
    if (state == 4 && --wait != 0) return false;
    state = 1;
    bool grouped = false;
    do {
        const auto command = token('<', '>');
        if (command == "CHECKFLAG") {
            const auto index = number();
            const auto on = token('"', '"');
            const auto off = token('"', '"');
            const auto selected = services.flag(index) ? on : off;
            for (;;) {
                if (token('<', '>') == "CASE" && token('"', '"') == selected) break;
            }
        } else if (command == "MENUOFF") services.menu(false);
        else if (command == "MENUON") services.menu(true);
        else if (command == "LUIGISTOP3") services.stopPlayer();
        else if (command == "LUIGIFREE2") services.releasePlayer();
        else if (command == "LUIGIFREE") services.releaseActor();
        else if (command == "FLAGON") services.setFlag(number() & 255);
        else if (command == "SE") services.sound(number() & 65535);
        else if (command == "FULLSCREEN") services.fullscreen(true);
        else if (command == "NORMALSCREEN") services.fullscreen(false);
        else if (command == "HLIGHTOFF") services.playerEvent(0x1e);
        else if (command == "LLIGHTOFF") services.playerEvent(0x1b);
        else if (command == "LLIGHTON") services.playerEvent(0x1c);
        else if (command == "EFFECT") {
            const auto placement = token('"', '"');
            const auto resource = token('"', '"');
            services.effect(placement, resource);
        }
        else if (command == "BGM") services.music(number() & 255);
        else if (command == "WINDOW") messageWindow = static_cast<std::uint8_t>(number());
        else if (command == "COLOR") services.messageColor(messageWindow, integer());
        else if (command == "CLOSEWINDOW") services.closeWindow(number());
        else if (command == "SPEAK") {
            services.speak(messageWindow, number());
            state = 2;
        }
        else if (command == "FRAMESTART") grouped = true;
        else if (command == "FRAMEEND") grouped = false;
        else if (command == "GCAMERA") services.camera(token('"', '"'));
        else if (command == "GPATHLIGHT") services.lighting(token('"', '"'));
        else if (command == "ACTOR") actor(services);
        else if (command == "CAMENDFAST") services.endCamera();
        else if (command == "CAMERAEND") services.blendCamera();
        else if (command == "WAIT") {
            wait = number();
            state = 4;
        } else if (command == "END") {
            services.closeMessage();
            state = 9;
        } else throw std::runtime_error("Unsupported entrance command: " + std::string(command));
    } while (grouped);
    return false;
}

bool gameevent::EntranceScript::finished() const { return completed; }
