#include "game/player/playerrecordwriteback.h"

namespace {
    void half(playerrecord::Record& record, unsigned offset, std::uint32_t value) {
        record[offset] = static_cast<std::uint8_t>(value >> 8);
        record[offset + 1] = static_cast<std::uint8_t>(value);
    }

    void word(playerrecord::Record& record, unsigned offset, std::uint32_t value) {
        half(record, offset, value >> 16);
        half(record, offset + 2, value);
    }
}

bool playerrecord::write(Record* destination, const Values& values) {
    if (!destination) return false;
    auto& record = *destination;
    record[0x16a] = static_cast<std::uint8_t>(((values.flags & 8) ? 2 : 0) | ((values.flags & 4) ? 1 : 0));
    half(record, 0, values.life);
    half(record, 2, values.maximumLife);
    for (unsigned i = 0; i < values.entries.size(); ++i) half(record, 0x32 + i * 2, values.entries[i]);
    word(record, 0xb4, values.count);
    word(record, 0xb8, values.field12F4);
    word(record, 0xbc, values.field12F8);
    for (unsigned i = 0; i < values.fields12FC.size(); ++i) record[0xc0 + i] = static_cast<std::uint8_t>(values.fields12FC[i]);
    for (unsigned i = 0; i < values.fields1318.size(); ++i) half(record, 0xc8 + i * 2, values.fields1318[i]);
    word(record, 0xd0, values.controlWord);
    return true;
}
