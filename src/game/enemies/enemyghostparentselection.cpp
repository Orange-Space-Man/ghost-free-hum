#include "game/enemies/enemyghostparentselection.h"
#include <stdexcept>
#include <utility>

namespace {
    bool validSource(const jmp::ToolDataRef& source) {
        return source.table && source.row >= 0;
    }

    void readKey(const jmp::ToolDataRef& source, const char* name, std::int32_t& value) {
        std::uint32_t found;
        if (source.table->getValue(static_cast<unsigned>(source.row), name, found)) value = static_cast<std::int32_t>(found);
    }

    bool startAnimation(actors::MoveActor& actor, characterowner::State& character, const char* name, std::uint32_t count) {
        const bool selected = actor.selectAnimation(name);
        if (selected) {
            character.field8cc = count;
            character.field8d0 = 0;
        }
        return selected;
    }
}

ghostparent::MovementChild ghostparent::movementChild(int type) {
    if (type == 4) return MovementChild::SearchFar;
    if (type < 4) return type == 1 ? MovementChild::Random : MovementChild::Default;
    if (type != 6 && type < 6) return MovementChild::SearchNear;
    return MovementChild::Default;
}

ghostparent::AppearanceChild ghostparent::appearanceChild(int type) {
    if (type == 2) return AppearanceChild::Appear2;
    if (type < 2) return type > 0 ? AppearanceChild::Frighten : AppearanceChild::None;
    if (type == 6) return AppearanceChild::Appear6;
    return AppearanceChild::None;
}

unsigned ghostparent::conditionStrategy(int type) {
    switch (type) {
    case 1: return 0x7b;
    case 2: return 0x7f;
    case 3: return 0x80;
    case 4: return 0x35;
    case 6: return 0x7c;
    case 7: return 0x7d;
    case 8: return 0x7e;
    case 9: return 0x81;
    case 10: return 0x82;
    case 0xb: return 0x84;
    case 0xc: return 0x86;
    case 0xd: return 0x8e;
    case 0xe: return 0x90;
    case 0xf: return 0x91;
    case 0x10: return 0x96;
    case 0x11: return 0xa9;
    case 0x12: return 0xb3;
    case 0x13: return 0xb4;
    default: return 0x7f;
    }
}

unsigned ghostparent::attackStrategy(std::int32_t pattern) {
    switch (pattern) {
    case 2: return 0x26;
    case 3: return 0x27;
    case 4: return 0xc;
    case 5: return 0x37;
    case 6: return 7;
    case 7: return 0xe;
    case 8: return 0x39;
    case 9: return 0xf;
    case 10: return 0x65;
    case 0xb: return 0x66;
    case 0xc: return 0x8f;
    case 0xd: return 0x95;
    case 0xe: return 0x98;
    case 0xf: return 0xb0;
    default: return 6;
    }
}

unsigned ghostparent::captureStrategy(std::int32_t tsuriType) {
    switch (tsuriType) {
    case 1: return 0x45;
    case 2: return 0x46;
    case 3: return 0x47;
    case 4: return 0x48;
    case 5: return 0xd;
    default: return 0x44;
    }
}

ghostparent::Configuration ghostparent::resolveConfiguration(const jmp::ToolDataRef& source, Overrides& overrides) {
    Configuration configuration;
    configuration.place = 0;
    configuration.movement = 1;
    configuration.search = 5;
    configuration.appearance = 2;
    configuration.condition = 7;
    configuration.invisible = 0;
    if (validSource(source)) {
        readKey(source, "move_type", configuration.movement);
        readKey(source, "search_type", configuration.search);
        readKey(source, "appear_type", configuration.appearance);
        readKey(source, "cond_type", configuration.condition);
        readKey(source, "invisible", configuration.invisible);
        readKey(source, "place_type", configuration.place);
        overrides.movement = configuration.movement;
        overrides.search = configuration.search;
        overrides.condition = configuration.condition;
        overrides.appearance = configuration.appearance;
        overrides.place = configuration.place;
    } else {
        if (-1 < overrides.place) configuration.place = overrides.place;
        if (-1 < overrides.movement) configuration.movement = overrides.movement;
        if (-1 < overrides.search) configuration.search = overrides.search;
        if (-1 < overrides.appearance) configuration.appearance = overrides.appearance;
        if (-1 < overrides.condition) configuration.condition = overrides.condition;
    }
    return configuration;
}

ghostparent::Selection ghostparent::initialize(Fields& fields, const InitializeServices& services) {
    if (!services.random) throw std::invalid_argument("Original ghost initialization requires the shared random stream.");
    auto& actor = services.actor;
    auto& character = services.character;
    fields.lightActive = false;
    startAnimation(actor, character, "NONE", 0);
    Selection selection;
    selection.configuration = resolveConfiguration(services.source, fields.overrides);
    fields.placementId = -1;
    selection.movement = movementChild(selection.configuration.movement);
    selection.search = movementChild(selection.configuration.search);
    selection.condition = conditionStrategy(selection.configuration.condition);
    selection.appearance = appearanceChild(selection.configuration.appearance);
    selection.attack = attackStrategy(parameterWord(services.parameters, Parameter::AttackPattern1));
    fields.lightActive = false;
    fields.invisibleMode = selection.configuration.invisible != 0 ? 1 : 0;
    character.fields93c[0] = 1.0f;
    character.fields93c[1] = 5.0f;
    character.fields93c[3] = 8.0f;
    character.fields93c[2] = randomOffset(speedForMode(services.parameters, fields.invisibleMode == 1 ? 1 : 0), services.random);
    if (!fields.holding && (selection.configuration.place == 1 || selection.configuration.place == 2))
        throw std::runtime_error("Original ghost placement types 1 and 2 (800C9354 / 800C93D0) are not ported.");
    fields.element = static_cast<std::uint32_t>(parameterWord(services.parameters, Parameter::Element));
    fields.fadeScaled = 0;
    return selection;
}
