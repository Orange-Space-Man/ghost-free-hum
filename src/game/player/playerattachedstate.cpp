#include "resources/dolresources.h"
#include "game/player/playerattachedstate.h"
#include "game/startup/startupresources.h"

#include <cstring>
#include <stdexcept>

void playercomponents::AttachedState::stopFirstTransition() {
    if (!field2C0) return;
    field2C0 = 0;
    field2C4 = field2CC = 0;
    field2C8 = 1;
}

void playercomponents::AttachedState::stopSecondTransition() {
    if (!field2D0) return;
    field2D0 = 0;
    field2D4 = 0;
    field2D8 = 1;
}
namespace {
    unsigned word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
            (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
    }

    float number(std::uint32_t value) {
        float result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    std::vector<std::uint8_t> range(const std::vector<std::uint8_t>& executable, unsigned address, std::size_t size) {
        std::vector<std::uint8_t> result;
        std::string error;
        if (!dolresources::read(executable, address, size, result, error)) throw std::runtime_error(error);
        return result;
    }
}

playercomponents::AttachedState::AttachedState(const std::vector<std::uint8_t>& executable, int actorType,
    actormodel::Instance& model, actors::MoveActor* actor) : actor(actor), model(model) {
    unsigned address = 0x80354ca8;
    if (actorType == 0x27) address = 0x80354ec0;
    else if (actorType >= 0 && actorType < 2) address = 0x80354dec;
    else if (actorType == 0xb3) address = 0x80354fd4;
    const auto bytes = range(executable, address, parameters.size() * 4);
    for (unsigned i = 0; i < parameters.size(); ++i) parameters[i] = word(bytes, i * 4);
    for (unsigned i = 0; i < 3; ++i) model.attachment(parameters[i]);
    fields24C.fill(parameters[52]);
    fields270.fill(0xffffffff);
    fields294.fill(0xffffffff);
    if (parameters[51]) {
        if (parameters[51] > 65535) throw std::runtime_error("Original attached-state override count is invalid.");
        const auto records = range(executable, parameters[50], std::size_t(parameters[51]) * 16);
        for (unsigned i = 0; i < parameters[51]; ++i) {
            const auto index = word(records, i * 16);
            fields24C.at(index) = word(records, i * 16 + 4);
            fields270.at(index) = word(records, i * 16 + 8);
            fields294.at(index) = word(records, i * 16 + 12);
        }
    }
    effects[0].flags = effects[1].flags = effects[2].flags = 0;
    effects[0].field8 = effects[1].field8 = 0;
    effects[2].field10 = {0, 0, 1000000};
    const float first = static_cast<float>(static_cast<std::int32_t>(parameters[10]));
    const float last = static_cast<float>(static_cast<std::int32_t>(parameters[11]));
    const float value = static_cast<float>(static_cast<std::int32_t>(parameters[12]));
    setBlend((value - first) / (last - first),
        (number(parameters[43]) - number(parameters[41])) / (number(parameters[42]) - number(parameters[41])));
    for (unsigned i = 0; i < 3; ++i) model.attachment(parameters[i]).object = &effects[i];
}

playercomponents::AttachedState::~AttachedState() {
    if (!fields2B8[3]) return;
    for (unsigned i = 0; i < 3; ++i) model.attachment(parameters[i]).object = nullptr;
    fields2B8[3] = 0;
}

void playercomponents::AttachedState::startFirstTransition(unsigned state, double speed) {
    if (field2C0 == state) return;
    field2C0 = state;
    field2C4 = field2CC = 0;
    field2C8 = static_cast<float>(speed);
}

void playercomponents::AttachedState::startSecondTransition(unsigned state, double speed) {
    if (field2D0 == state) return;
    field2D0 = state;
    field2D4 = 0;
    field2D8 = static_cast<float>(speed);
}

std::array<std::int32_t, 2> playercomponents::AttachedState::setBlend(double first, double second) {
    std::array<std::int32_t, 2> result;
    const double values[]{first, second};
    float* destinations[]{&field240, &field244};
    for (unsigned i = 0; i < 2; ++i) {
        double value = values[i];
        result[i] = 0 <= value ? (value <= 1 ? 0 : 1) : -1;
        if (value < 0) value = 0;
        if (1 < value) value = 1;
        *destinations[i] = static_cast<float>(value);
    }
    return result;
}

playercomponents::AttachedSingle::AttachedSingle(const std::vector<std::uint8_t>& executable, int actorType,
    actormodel::Instance& model, actors::MoveActor* actor) : actor(actor), model(model) {
    unsigned address = 0x803550a8;
    if (actorType == 0x27) address = 0x803550f8;
    else if (actorType >= 0 && actorType < 2) address = 0x803550d0;
    else if (actorType == 0xb3) address = 0x80355120;
    const auto bytes = range(executable, address, parameters.size() * 4);
    for (unsigned i = 0; i < parameters.size(); ++i) parameters[i] = word(bytes, i * 4);
    effect.flags = 0;
    effect.field40 = 0;
    effect.field34 = 1;
    effect.field38 = effect.field3C = 0;
    model.attachment(parameters[0]).object = &effect;
}

playercomponents::AttachedSingle::~AttachedSingle() {
    if (!field29) return;
    model.attachment(parameters[0]).object = nullptr;
    field29 = 0;
}
