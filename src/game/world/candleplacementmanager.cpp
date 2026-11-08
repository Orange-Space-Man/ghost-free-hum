#include "game/world/candleplacementmanager.h"

#include <cstring>
#include <stdexcept>

namespace {
    std::string_view name(const jmp::ToolDataRef& entry) {
        std::string_view value;
        if (!entry.table || !entry.table->getValue(entry.row, "name", value))
            throw std::runtime_error("Original candle placement requires its name field.");
        return value;
    }

    std::uint32_t bits(float value) {
        std::uint32_t result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }
}

particles::CandlePlacements::CandlePlacements(ArchiveResources& resources, EmitterPool& emitters, GlowPool& glows,
    const playeractor::View& player, const roomlocation::Rooms& rooms, const roomtraversal::State& visibility,
    std::uint32_t& randomState, std::function<bool(unsigned)> cleared,
    std::function<void(unsigned, const std::array<float, 3>&)> sound) : resources(resources), emitters(emitters), glows(glows),
    player(player), rooms(rooms), visibility(visibility), randomState(randomState), cleared(std::move(cleared)), sound(std::move(sound)) {
    for (auto& candle : candles) {
        candle.manager = this;
        free.push_back(&candle);
    }
}

particles::CandlePlacements::~CandlePlacements() {
    for (auto* candle : active) retire(*candle);
    for (auto& ring : emitters.active)
        for (auto* emitter = ring.sentinel.next; emitter != &ring.sentinel; emitter = emitter->next)
            for (auto& candle : candles) if (emitter->fieldC4 == &candle) emitter->fieldC4 = nullptr;
}

bool particles::CandlePlacements::create(jmp::ToolDataRef& entry) {
    const auto kind = name(entry);
    if ((kind != "fire" && kind != "sfire") || free.empty()) return false;
    auto* candle = free.front();
    free.pop_front();
    *candle = Candle{};
    candle->manager = this;
    const char* coordinates[] = {"pos_x", "pos_y", "pos_z"};
    for (unsigned i = 0; i < 3; ++i) if (!entry.table->getValue(entry.row, coordinates[i], candle->position[i]))
        throw std::runtime_error("Original candle position is missing.");
    float direction = 0;
    std::uint32_t mode = 0, condition = 0, variant = 0;
    entry.table->getValue(entry.row, "dir_z", direction);
    entry.table->getValue(entry.row, "arg0", mode);
    entry.table->getValue(entry.row, "arg1", condition);
    entry.table->getValue(entry.row, "arg2", variant);
    candle->row = entry.row;
    candle->parameters = resources.get(0x4b485f45, variant ? "candle_vio.pcr" : "candle_moe.pcr");
    candle->texture = resources.get(0x4b485f45, "ps_ball.bti");
    candle->color = variant ? std::array<std::uint8_t, 4>{164, 89, 184, 255} : std::array<std::uint8_t, 4>{207, 126, 80, 255};
    candle->emitter = emitters.create(candle->parameters, candle->texture);
    if (!candle->emitter) {
        *candle = Candle{};
        candle->manager = this;
        free.push_back(candle);
        return false;
    }
    auto& emitter = *candle->emitter;
    emitter.position = candle->position;
    emitter.field220 = emitter.field22C = {};
    emitter.fieldC4 = candle;
    emitter.fieldC8 = nullptr;
    constexpr unsigned modes[] = {0, 1, 8, 16, 9, 24, 40};
    if (mode < std::size(modes)) candle->flags = modes[mode];
    if (condition == 1) candle->flags |= 2;
    candle->height = direction == 0 ? 1 : -1;
    emitter.field220[1] = float(candle->height);
    emitter.parameters.fields174 = {0, bits(0.93f * float(candle->height)), 0};
    candle->state = condition == 1 ? 5 : (candle->flags & 8) ? 4 : 1;
    if (candle->state != 1) emitter.flags &= 0xfffd;
    candle->room = rooms.resolve(candle->position);
    if (kind == "sfire") candle->flags |= 4;
    active.push_back(candle);
    return true;
}

void particles::CandlePlacements::retire(Candle& candle) {
    if (candle.emitter) candle.emitter->flags = 0x1c;
    glows.release(candle.glow);
    candle = Candle{};
    candle.manager = this;
}

bool particles::CandlePlacements::remove(jmp::ToolDataRef& entry) {
    const auto kind = name(entry);
    if (kind != "fire" && kind != "sfire") return false;
    for (auto it = active.begin(); it != active.end(); ++it) {
        auto* candle = *it;
        if (candle->row != entry.row) continue;
        retire(*candle);
        active.erase(it);
        free.push_back(candle);
        return true;
    }
    return false;
}

bool particles::CandlePlacements::message(jmp::ToolDataRef& entry, const char* command) {
    const auto kind = name(entry);
    if (kind != "fire" && kind != "sfire") return false;
    for (auto* candle : active) {
        if (candle->row != entry.row) continue;
        const auto source = resources.get(0x4b485f45, std::string(command) + ".pcr");
        if (!source) continue;
        candle->emitter->reload(source, candle->texture);
        if (std::strcmp(command, "candle_kie") == 0) {
            glows.release(candle->glow);
            candle->glow = nullptr;
        } else {
            if (!candle->glow) candle->glow = glows.create(candle->position, 2.6f, candle->height, candle->color);
            if (std::strcmp(command, "candle_fat") == 0) sound(0x18bd, candle->position);
        }
        return true;
    }
    return false;
}

unsigned particles::CandlePlacements::count() const { return static_cast<unsigned>(active.size()); }
void particles::CandlePlacements::Candle::advance(Emitter&) { manager->update(*this); }

float particles::CandlePlacements::random() {
    randomState = randomState * 0x5eedf715u + 0x1b0cb173u;
    return 0x1p-32f * float(randomState);
}

void particles::CandlePlacements::disturb(Candle& candle) {
    candle.disturbance[0] = 16.0f * random() - 8.0f;
    candle.disturbance[1] = 5.0f * float(candle.height) * (1.0f + random());
    candle.disturbance[2] = 16.0f * random() - 8.0f;
}

void particles::CandlePlacements::update(Candle& candle) {
    if (!candle.emitter) return;
    auto& emitter = *candle.emitter;
    auto& parameters = emitter.parameters;
    const bool visible = (visibility.record(candle.room, 0).flags & 1) != 0;
    const float x = (*player.position)[0] - candle.position[0], z = (*player.position)[2] - candle.position[2];
    const float distance = x * x + z * z;
    if (candle.state == 1 && !(candle.flags & 1)) {
        parameters.flags &= ~0x300000u;
        parameters.field1E4 = 0;
        if (visible && player.field89E && player.field8A0 != 5)
            throw std::runtime_error("Candle interaction requires the original elemental equipment path.");
        if (distance < 90000.0f) {
            disturb(candle);
            candle.state = 2;
        }
    } else if (candle.state == 2) {
        if (!visible || distance > 90000.0f || (player.field89E && player.field8A0 != 5)) candle.state = 1;
        else {
            const float strength = 0.007f * player.magnitude;
            if (random() < 0.6f) disturb(candle);
            parameters.flags |= 0x300000;
            for (unsigned i = 0; i < 3; ++i) parameters.fields1AC[i] = bits(candle.disturbance[i]);
            parameters.fields1AC[3] = bits(strength);
            parameters.fields1BC.fill(bits(0.25f * (strength - 0.007f)));
            parameters.fields1C8 = {2, 5, 2};
            parameters.field1E4 = 2;
        }
    } else if (candle.state == 5 && cleared(candle.room >> 24)) {
        emitter.flags |= 2;
        candle.state = 1;
    }
    if ((candle.flags & 32) && candle.room != player.room) {
        if (candle.flags & 8) {
            emitter.flags &= 0xfffd;
            candle.state = 4;
        } else {
            emitter.flags |= 2;
            candle.state = 1;
        }
    }
    if (emitter.nodes.count + emitter.shortNodes.count > 4 && (emitter.flags & 8) && !(emitter.flags & 0x40)) {
        if (!candle.glow) candle.glow = glows.create(candle.position, 2.6f, candle.height, candle.color);
    } else {
        glows.release(candle.glow);
        candle.glow = nullptr;
    }
}
