#include "game/enemies/enemypearlstrategy.h"

#include <stdexcept>

enemystrategy::Pearl::Pearl(ItemEnvironment& environment, const Factory& factory) : Jewel(environment, factory) {}

void enemystrategy::Pearl::bindSource(unsigned type) {
    sourceIndex = environment.feedback.properties.savedIndex(type);
}

void enemystrategy::Pearl::initializeItem() {
    Jewel::initializeItem();
    flags &= ~2u;
    const int entry = environment.common.findPathResource("/param/th/EnFinitePearl.prm");
    if (entry >= 0) {
        std::string error;
        if (!lifetime.parameters.load(environment.common.resource(static_cast<unsigned>(entry)), error)) throw std::runtime_error(error);
    }
}

void enemystrategy::Pearl::prepareCollection(playerevents::ItemRequest& request) {
    const auto type = actor().type();
    request.grade = type == 0xc2 ? 1 : type == 0xc3 ? 2 : type == 0xc4 ? 3 : 0;
    if (sourceIndex >= 0) request.gradeIndex = sourceIndex;
}
