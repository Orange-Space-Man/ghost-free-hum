#include "game/world/mapcollisionstate.h"

namespace {
    std::uint16_t maskBit(unsigned bit) {
        bit &= 63;
        return bit < 16 ? static_cast<std::uint16_t>(1U << bit) : 0;
    }
}

mapcollision::State::State(const mission::Resources& resources) : resources(resources) {}

void mapcollision::State::load(const std::vector<std::uint8_t>& data) {
    collision = data;
    const auto* sound = resources.getJmpResource("SoundPolygonInfo");
    if (sound) {
        soundMaterial = sound->searchItemInfo("sound_material");
        soundEcho = sound->searchItemInfo("sound_echo_switch");
    }
}

void mapcollision::State::enable(unsigned bit) { enabled |= maskBit(bit); }
void mapcollision::State::disable(unsigned bit) { enabled &= static_cast<std::uint16_t>(~maskBit(bit)); }
std::uint16_t mapcollision::State::mask() const { return enabled; }
int mapcollision::State::soundMaterialField() const { return soundMaterial; }
int mapcollision::State::soundEchoField() const { return soundEcho; }
const jmp::ToolData* mapcollision::State::polygonInfo() const { return resources.getJmpResource("PolygonInfo"); }
