#include "resources/dolresources.h"
#include "game/player/playeractionstate.h"
#include "game/player/playerattachedstate.h"
#include "game/startup/startupresources.h"

#include <stdexcept>

namespace {
    std::uint32_t word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (std::uint32_t(bytes.at(offset)) << 24) | (std::uint32_t(bytes.at(offset + 1)) << 16) |
            (std::uint32_t(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
    }

    std::vector<std::uint8_t> range(const std::vector<std::uint8_t>& executable, std::uint32_t address, std::size_t size) {
        std::vector<std::uint8_t> result;
        std::string error;
        if (!dolresources::read(executable, address, size, result, error)) throw std::runtime_error(error);
        return result;
    }
}

playeractions::Actions::Actions(const std::vector<std::uint8_t>& executable, int actorType) {
    reset();
    unsigned address = 0x80352398;
    if (actorType == 0x27) address = 0x803527e0;
    else if (actorType >= 0 && actorType < 2) address = 0x803527d4;
    else if (actorType == 0xb3) address = 0x80352b9c;
    const auto bytes = range(executable, address, 12);
    for (unsigned i = 0; i < 3; ++i) parameters[i] = word(bytes, i * 4);
    entries.fill(parameters[2]);
    if (parameters[1]) {
        if (parameters[1] > 65535) throw std::runtime_error("Original player action override count is invalid.");
        const auto records = range(executable, parameters[0], std::size_t(parameters[1]) * 8);
        for (unsigned i = 0; i < parameters[1]; ++i) entries.at(word(records, i * 8)) = word(records, i * 8 + 4);
    }
    field2FC = 0;
    field308 = 0;
    field30C = 1;
    field30D = 0;
    field310 = 1;
    field314 = 0;
    field318 = 0;
    fields31C.fill(0);
    field358 = field35C = 0;
    field60 = field64 = 0;
    field300 = 0;
    fields304.fill(0);
    field360 = 0;
    field50 = 0;
}

void playeractions::Actions::reset() {
    attached = nullptr;
    actor = nullptr;
    fields0.fill(0);
    field10 = 1;
    field11 = field14 = 0;
    field18 = field1C = 0;
    field20 = field2C = 0;
    fields39.fill(0);
    field3F = 0;
    fields40.fill(0);
}

bool playeractions::Actions::suctionReaction(std::uint32_t reaction, std::uint32_t kind) {
    if (field310 == 2 && (field314 == 0x20 || field314 == 0x21 || field314 == 0x22)) return false;
    ReactionPayload payload;
    payload.fields[4] = reaction;
    payload.fields[5] = kind;
    beginReaction(0xe, payload);
    return true;
}

void playeractions::Actions::updateNormalState() {
    ReactionPayload payload;
    if (!field11) {
        if (field30D && field3F && fields31C[4] != 10 && fields31C[4] != 13 && fields31C[4] != 14) {
            if (!attached) {
                ReactionPayload release;
                release.fields[4] = fields0[1];
                release.fields[5] = fields0[3];
                beginReaction(0xc, release);
            } else if (attached->field2C0 != 8) {
                attached->startFirstTransition(2, 1);
                attached->startSecondTransition(2, 1);
            }
        }
        field30D = 0;
    } else {
        if (!field30D) {
            ReactionPayload start;
            start.fields[4] = fields0[1];
            beginReaction(10, start);
            if (fields31C[4] == 5) fields31C[4] = 0;
        }
        field30D = 1;
    }
    if (field310 == 2 || fields40[0] == fields31C[4]) return;
    switch (fields31C[4]) {
    case 2: beginReaction(0x10, payload); fields31C[4] = 0; break;
    case 5:
        if (fields0[1] > 0 && fields0[1] < 4) beginReaction(0x12, payload);
        fields31C[4] = 0;
        break;
    case 9: beginReaction(0x15, payload); fields31C[4] = 0; break;
    case 10: beginReaction(0x16, payload); fields31C[4] = 0; break;
    case 13: beginReaction(0x1c, payload); fields31C[4] = 0; break;
    case 14:
        if (attached) {
            attached->startFirstTransition(6, 1);
            attached->startFirstTransition(6, 1);
        }
        fields31C[4] = 0;
        break;
    default: fields31C[4] = fields40[0]; break;
    }
    switch (fields40[0]) {
    case 2: beginReaction(0xf, payload); break;
    case 5: if (fields0[1] > 0 && fields0[1] < 4) beginReaction(0x11, payload); break;
    case 9: beginReaction(0x14, payload); break;
    case 11: beginReaction(0x1b, payload); break;
    }
}

bool playeractions::Actions::query74544() const {
    return field30D && !(field310 == 2 && field314 == 10);
}

bool playeractions::Actions::query74588() const {
    if (!field30D) return false;
    for (unsigned value : {5U, 2U, 10U, 13U, 11U, 12U, 14U, 6U, 7U, 8U})
        if (fields31C[4] == value) return false;
    if (field310 == 2) {
        for (unsigned value : {0x11U, 0x12U, 1U, 0xfU, 0x10U, 0x20U, 10U, 11U, 12U, 0x19U, 0x1aU, 4U,
            8U, 9U, 0x22U, 7U, 4U, 0x21U, 0x16U, 0x17U, 0x18U, 0x1cU, 0x1bU, 0x14U, 0x15U})
            if (field314 == value) return false;
        if (field314 == 3 && fields31C[5] == 5) return false;
    }
    return true;
}

bool playeractions::Actions::query74968() const {
    if (!field30D) return false;
    for (unsigned value : {10U, 13U, 11U, 12U, 14U, 5U})
        if (fields31C[4] == value) return false;
    if (field310 == 2) {
        for (unsigned value : {11U, 10U, 12U, 0x19U, 0x1aU, 4U, 8U, 9U, 0x22U, 0x21U, 0x1bU, 0x1cU, 0x17U, 0x18U, 0x16U})
            if (field314 == value) return false;
        if (field314 == 0x20 && (fields31C[0] == 9 || fields31C[0] == 10 || fields31C[0] == 11)) return false;
    }
    return true;
}

playeractions::Reactions::Reactions(const std::vector<std::uint8_t>& executable, int actorType) {
    const auto descriptor = range(executable, actorType >= 0 && actorType < 2 ? 0x804d2428 : 0x804d2420, 8);
    const auto count = word(descriptor, 4);
    if (count > 65535) throw std::runtime_error("Original player reaction count is invalid.");
    if (!count) return;
    const auto bytes = range(executable, word(descriptor, 0), std::size_t(count) * 52);
    records.resize(count);
    for (unsigned i = 0; i < count; ++i) {
        auto& record = records[i];
        for (unsigned field = 0; field < 13; ++field) record[field] = word(bytes, i * 52 + field * 4);
        entries.at(record[0]) = &record;
    }
}
