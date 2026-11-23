#include "game/effects/trainingfireworks.h"
#include "resources/dolresources.h"
#include "resources/gameedition.h"

#include <cstring>
#include <stdexcept>
#include <utility>

particles::TrainingFireworks::TrainingFireworks(const std::vector<std::uint8_t>& executable,
    ArchiveResources& resources, EmitterPool& pool, std::function<void(unsigned)> sound) :
    resources(resources), pool(pool), sound(std::move(sound)) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, gameedition::trainingFireworks(executable),
        27 * 12, bytes, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < positions.size(); ++i) for (unsigned axis = 0; axis < 3; ++axis) {
        const auto offset = i * 12 + axis * 4;
        const std::uint32_t bits = (unsigned(bytes[offset]) << 24) | (unsigned(bytes[offset + 1]) << 16) |
            (unsigned(bytes[offset + 2]) << 8) | bytes[offset + 3];
        std::memcpy(&positions[i][axis], &bits, sizeof(bits));
    }
}

bool particles::TrainingFireworks::start(std::string_view command) {
    if (command == "hanabiLL") variant = 0;
    else if (command == "hanabiL") variant = 1;
    else if (command == "hanabiM") variant = 2;
    else if (command == "hanabiS") variant = 3;
    else return false;
    const char* names[]{"fireworks.pcr", "fireworks_r.pcr", "fireworks_y.pcr", "fireworks_b.pcr"};
    for (unsigned i = 0; i < parameters.size(); ++i) parameters[i] = resources.get(0x4b485f45, names[i]);
    texture = resources.get(0x4b485f45, "fireworks.bti");
    frame = repeats = 0;
    active = true;
    return true;
}

void particles::TrainingFireworks::update(unsigned map) {
    if (map != 3 || !active) return;
    static constexpr unsigned times[]{0, 3, 9, 9, 12, 18, 25, 27, 31};
    static constexpr unsigned colors[]{0, 0, 0, 2, 1, 0, 3, 3, 0};
    const unsigned count = variant == 3 ? 3 : variant == 2 ? 6 : 9;
    const unsigned first = variant == 3 ? 24 : variant == 2 ? 18 : variant * 9;
    bool sounded = false;
    for (unsigned i = 0; i < count; ++i) {
        if (frame != times[i]) continue;
        if (!sounded) { sound(0x1895); sounded = true; }
        auto* emitter = pool.create(parameters[variant == 3 ? 0 : colors[i]], texture);
        if (!emitter) continue;
        emitter->position = positions[first + i];
        emitter->field220 = emitter->field22C = {};
        emitter->fieldC4 = nullptr;
        emitter->fieldC8 = nullptr;
    }
    if (frame == times[count - 1]) {
        if (variant != 0) active = false;
        else if (++repeats == 3) active = false;
    }
    ++frame;
}
