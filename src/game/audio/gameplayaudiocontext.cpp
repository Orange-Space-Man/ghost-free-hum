#include "game/audio/gameplayaudiocontext.h"

#include <stdexcept>

namespace {
    unsigned field(const jmp::ToolData& table, unsigned row, const char* name) {
        std::uint32_t value = 0;
        if (!table.getValue(row, name, value)) throw std::runtime_error("Missing original room audio field.");
        return value;
    }
}

unsigned gameplayaudio::RoomParameters::size() const { return (packed >> 6) & 3; }
unsigned gameplayaudio::RoomParameters::musicCode() const { return (packed >> 8) & 7; }
unsigned gameplayaudio::RoomParameters::material() const { return (packed >> 16) & 15; }

gameplayaudio::Context::Context(const mission::Resources& resources, const gameflags::State& flags, unsigned map) :
    roomInfo(resources.getJmpResource("RoomInfo")), soundInfo(resources.getJmpResource("SoundPolygonInfo")), flags(flags), map(map) {
    roomRows.fill(0xffff);
    if (roomInfo) {
        for (unsigned row = 0; row < roomInfo->count(); ++row) {
            const unsigned room = field(*roomInfo, static_cast<std::uint16_t>(row), "RoomNo");
            roomRows.at(room) = static_cast<std::uint16_t>(row);
        }
    }
    floorFields = soundInfo && soundInfo->searchItemInfo("sound_material") >= 0 &&
        soundInfo->searchItemInfo("sound_echo_switch") >= 0;
}

gameplayaudio::RoomParameters gameplayaudio::Context::parameters(unsigned room, std::int16_t soundPolygon) const {
    RoomParameters result;
    if (room != 0xffffffff) {
        const auto row = roomRows[room & 255];
        if (row != 0xffff) {
            if (map == 2 && row == 0x29 && (flags.value(0x32) || flags.value(0x31))) result.packed = 0x280;
            else result.packed = (field(*roomInfo, row, "sound_echo_parameter") & 63) |
                ((field(*roomInfo, row, "sound_room_size") & 3) << 6) |
                ((field(*roomInfo, row, "sound_room_code") & 7) << 8);
        }
    }
    if (soundPolygon >= 0 && floorFields) {
        const unsigned material = field(*soundInfo, soundPolygon, "sound_material") & 15;
        const unsigned echo = field(*soundInfo, soundPolygon, "sound_echo_switch") & 7;
        result.packed |= (material | (echo << 4)) << 16;
    }
    return result;
}
