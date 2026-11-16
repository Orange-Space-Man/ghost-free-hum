#include "game/enemies/enemycapturelifecycle.h"
#include "resources/dolresources.h"
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
    unsigned word(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }

    float number(unsigned bits) {
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }
}

void ghostcapture::release(characterowner::Owner& owner, enemystrategy::Strategy& strategy) {
    auto& actor = dynamic_cast<actors::MoveActor&>(owner.actor());
    auto& state = owner.state();
    actor.fieldE4 = (actor.fieldE4 & 0xffffff00) | 255;
    actor.fieldE8 = 4;
    actor.previousRotation[0] = 0;
    if (state.actorUsers[0]) {
        owner.releaseActorReference(state.actorUsers[0]);
        state.flags904[2] = false;
    }
    strategy.setNextState(state.captureRecord ? 0x101 : 0x102);
}

bool ghostcapture::releaseIfUnlinked(characterowner::Owner& owner, enemystrategy::Strategy& strategy) {
    if (owner.state().captureRecord) return false;
    release(owner, strategy);
    return true;
}

ghostcapture::Bookkeeping::Bookkeeping(const std::vector<std::uint8_t>& executable,
    const enemycreation::Catalog& types, DropServices services) : executable(executable), types(types), services(std::move(services)) {
    if (!this->services.pearl || !this->services.periodic) {
        throw std::invalid_argument("Capture bookkeeping requires original pearl and periodic item services.");
    }
    for (unsigned i = 0; i < portraitTypes.size(); ++i) portraitTypes[i] = word(executable, 0x80338580 + 4 * i);
    launch = {number(word(executable, 0x804db794)), number(word(executable, 0x804db7a0)),
        number(word(executable, 0x804db794))};
}

void ghostcapture::Bookkeeping::reset() {
    ticks = 0;
    pearlIndex = 0;
}

void ghostcapture::Bookkeeping::update(unsigned state, const actors::MoveActor& actor,
    const characterowner::State& owner, int period, int repeatColumn, unsigned itemTable) {
    if (state <= 1) return;
    ++ticks;
    int portrait = -1;
    for (unsigned i = 0; i < portraitTypes.size(); ++i) {
        if (portraitTypes[i] == actor.type()) { portrait = static_cast<int>(i); break; }
    }
    if (portrait >= 0 && types.group(actor.type()) == 3) {
        const auto life = static_cast<std::int16_t>(actor.life);
        const auto delta = static_cast<std::int32_t>(static_cast<std::uint32_t>(life) - owner.life);
        if (delta < 0 && life != 0 && life % 10 == 0) {
            const unsigned type = word(executable, 0x8020c828 + pearlIndex * 4);
            if (services.pearl(type, portrait, actor.position, launch)) ++pearlIndex;
        }
    }
    const auto signedTicks = static_cast<std::int32_t>(ticks);
    if (!period || (signedTicks == std::numeric_limits<int>::min() && period == -1)) {
        throw std::runtime_error("Capture drop period is invalid for the original signed division.");
    }
    const int quotient = signedTicks / period;
    if (signedTicks % period == 0) services.periodic(itemTable, quotient, repeatColumn, actor.position, launch);
}
