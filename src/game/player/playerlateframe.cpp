#include "game/player/playerlateframe.h"
#include "game/world/dooranimationpose.h"

#include <stdexcept>

playeractor::LateFrame::LateFrame(const std::vector<std::uint8_t>& executable) : state(executable), capture(executable) {}

void playeractor::LateFrame::begin(Player& player, const LateFrameServices& services) const {
    state.begin(player, {services.hidePlayers, services.scannerTransition});
    capture.updateOpacity(player);
    player.state.fieldF5C = 255;
    auto& presentation = *player.components->state80;
    if (presentation.field10) {
        const auto joint = player.model ? dooranimation::concatenate(player.matrix,
            player.model->matrix(0, presentation.field44)) : player.matrix;
        presentation.field48 = dooranimation::concatenate(presentation.field14, joint);
    }
    player.components->weapon->updateEndpoints(*player.state.field7A4, *player.state.field7A8);
}

void playeractor::LateFrame::finish(Player& player, const playermodels::SharedModels& shared,
    const LateFrameServices& services) const {
    state.scanner(*player.components->scanner, shared);
    bool active = false;
    for (const auto value : player.components->grids->fields16BC) active = active || value != 0;
    if (active) {
        if (!services.sheets) throw std::runtime_error("Player late frame requires its active sheet owner.");
        services.sheets(player);
    }
    player.refreshView();
}
