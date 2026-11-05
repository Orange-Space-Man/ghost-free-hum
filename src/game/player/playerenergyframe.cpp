#include "playerenergyframe.h"
#include "resources/dolresources.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
    template<class T> T constant(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, sizeof(T), bytes, error)) throw std::runtime_error(error);
        std::reverse(bytes.begin(), bytes.end());
        T result;
        std::memcpy(&result, bytes.data(), sizeof(result));
        return result;
    }

    constexpr unsigned energyMaximumAddress = 0x804d2c0c;
    constexpr unsigned energyRecoveryAddress = 0x804d2c10;
    constexpr unsigned vacuumIdleStrengthAddress = 0x804dab20;
    constexpr unsigned playerZeroAddress = 0x804db50c;
    constexpr unsigned playerStepAddress = 0x804db518;
    constexpr unsigned energyCategoryAddress = 0x804db52c;
    constexpr unsigned energyCostFractionAddress = 0x804db580;
}

playeractor::EnergyFrame::EnergyFrame(const std::vector<std::uint8_t>& executable) :
    maximum(constant<std::int32_t>(executable, energyMaximumAddress)),
    recovery(constant<std::int32_t>(executable, energyRecoveryAddress)),
    idleStrength(constant<float>(executable, vacuumIdleStrengthAddress)),
    zero(constant<float>(executable, playerZeroAddress)), step(constant<float>(executable, playerStepAddress)),
    costCategory(constant<float>(executable, energyCategoryAddress)),
    costFraction(constant<float>(executable, energyCostFractionAddress)) {}

void playeractor::EnergyFrame::run(Player& player, const actormodel::Catalog& catalog, bool vacuumUpdated) const {
    auto& state = player.state;
    const float limit = static_cast<float>(maximum);
    if (!vacuumUpdated) {
        auto& vacuum = *player.components->vacuum;
        vacuum.field4 = idleStrength;
        vacuum.fields8[0] = 0;
        if (state.field1008 <= zero) {
            if (player.components->controls->flags24[1]) {
                const float next = state.field1004 + static_cast<float>(maximum / 10);
                state.field1004 = next < limit ? next : limit;
            }
            const float next = state.field1004 + static_cast<float>(recovery);
            state.field1004 = next < limit ? next : limit;
        } else state.field1008 -= step;
    }
    state.field1004 = limit;
    for (unsigned i = 0; i < state.entryCount; ++i) {
        if (costCategory <= static_cast<float>(catalog.type(state.entries.at(i)).field14))
            state.field1004 -= static_cast<float>(double(costFraction) * double(limit));
    }
    if (state.field1004 < zero) state.field1004 = zero;
}

bool playeractor::EnergyFrame::beginCaptureReaction(Player& player, const playermodels::VacuumEntries& entries,
    std::uint8_t& previousReaction) const {
    auto& state = player.state;
    previousReaction = state.field11D4;
    state.fields11B0.fill(0);
    state.field11D4 = 0;
    if (entries.containsSource(&player)) return true;
    state.field11D0 = zero;
    return false;
}
