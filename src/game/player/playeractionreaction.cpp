#include "game/player/playeractionstate.h"
#include "game/player/playerattachedstate.h"

playeractions::ReactionPayload::ReactionPayload() { reset(); }

void playeractions::ReactionPayload::reset() {
    for (unsigned i = 0; i < fields.size(); ++i) if (i != 3 && i != 8) fields[i] = 0;
}

void playeractions::Actions::beginReaction(std::uint32_t reaction, const ReactionPayload& payload) {
    const auto get = [&](unsigned index) { return payload.fields.at(index).value(); };
    if (reaction == 8) get(8);
    if (reaction == 0x22) get(3);
    field310 = 2;
    field314 = reaction;
    field318 = 0;
    switch (reaction) {
    case 3:
        fields31C[5] = get(6);
        if (fields31C[5] > 2 && fields31C[5] < 5) field30D = 0;
        break;
    case 4:
    case 0x1f:
        fields31C[6] = get(7);
        fields31C[8] = get(9);
        fields31C[9] = get(10);
        if (reaction == 4 && fields31C[6] > 3 && fields31C[6] < 7) field30D = 0;
        break;
    case 5: fields31C[8] = get(9); break;
    case 6:
    case 7: field354 = get(15); break;
    case 8: fields31C[7] = get(8); break;
    case 10: field358 = get(4); break;
    case 0xc:
    case 0xe:
        field358 = get(4);
        field35C = get(5);
        break;
    case 0x13: fields31C[10] = get(11); break;
    case 0x17: field30D = 0; break;
    case 0x20:
        fields31C[0] = get(0);
        field358 = get(4);
        fields31C[11] = get(12);
        fields31C[12] = get(13);
        break;
    case 0x21:
        fields31C[1] = get(1);
        fields31C[2] = get(2);
        field358 = get(4);
        fields31C[13] = get(14);
        break;
    case 0x22:
        field30D = 0;
        fields31C[3] = get(3);
        break;
    }
    if (attached) {
        attached->stopFirstTransition();
        attached->stopSecondTransition();
    }
    if (reaction != 0x10 && fields31C[4] == 2) fields31C[4] = 0;
    field2FC = 1;
}

std::uint32_t playeractions::Actions::reactionEntry() const {
    const auto range = [](unsigned value, unsigned first, unsigned last, unsigned entry) {
        return value >= first && value <= last ? entry + value - first : 0;
    };
    const auto locomotion = [&](bool moving) -> unsigned {
        if (field358 == 1) return moving ? 4 : 1;
        if (field358 == 2) return moving ? 9 : 8;
        if (field358 == 3) return moving ? 0xc : 0xb;
        return 0;
    };
    switch (field314) {
    case 1: return 0x37;
    case 2: return 0x38;
    case 3: return range(fields31C[5], 1, 5, 0x53);
    case 4:
        if (fields31C[9] != 1 && fields31C[9] != 2) return 0;
        switch (fields31C[6]) {
        case 2: return fields31C[9] == 1 ? 0x6c : 0x6d;
        case 3: return 0x6e;
        case 4: return 0x6f;
        case 5: return fields31C[9] == 1 ? 0x70 : 0x71;
        case 6: return fields31C[9] == 1 ? 0x72 : 0x73;
        default: return 0;
        }
    case 5: return range(fields31C[8], 2, 4, 0x74);
    case 6: return 0x77;
    case 7: return range(fields31C[9], 1, 2, 0x78);
    case 8:
        if (fields31C[7] >= 4) return range(fields31C[7], 4, 5, 0x7d);
        return range(fields31C[7], 1, 3, field30D ? 0x7f : 0x7a);
    case 9: return 0x82;
    case 10: return field358 == 3 ? 0x60 : 0x5e;
    case 0xb: return 0x63;
    case 0xc:
        if (field35C == 1) return field358 == 3 ? 0x61 : 0x5f;
        return field35C >= 2 && field35C <= 4 ? 0x62 : 0;
    case 0xd: return 0x66;
    case 0xe: return 0x68;
    case 0xf: return 0x69;
    case 0x10: return 0x6b;
    case 0x11: return field30D ? 0x46 : 0x42;
    case 0x12: return field30D ? 0x48 : 0x44;
    case 0x13: return range(fields31C[10], 1, 2, 0x4e);
    case 0x14: return 0x39;
    case 0x15: return 0x3b;
    case 0x16: return 0x59;
    case 0x17: return 0x5a;
    case 0x18: return 0x5b;
    case 0x19: return 0x64;
    case 0x1a: return 0x65;
    case 0x1b: return 0x3c;
    case 0x1c: return 0x40;
    case 0x1d: return 7;
    case 0x1e: return 0x5c;
    case 0x1f: return 0x83;
    case 0x20: {
        const auto mode = fields31C[0], variant = fields31C[11];
        if (mode == 1 || mode == 2) return locomotion(mode == 1);
        if (variant != 1 && variant != 2) return 0;
        const unsigned base = variant == 1 ? 0x84 : 0x8c;
        switch (mode) {
        case 3: return base + (field30D != 0);
        case 4: case 5: case 6: case 7: return base + mode;
        case 8: return base + 2 + (field30D != 0);
        case 9: return variant == 2 && field30D ? 0x95 : 0x94;
        case 10: return 0x96;
        case 11: return 0x97;
        default: return 0;
        }
    }
    case 0x21:
        switch (fields31C[1]) {
        case 1: return locomotion(true);
        case 2: return fields31C[13] == 1 ? 0xa0 : fields31C[13] == 2 ? 0x98 : 0;
        case 3: return range(fields31C[2], 1, 2, 0x9a);
        case 4: return range(fields31C[2], 1, 2, 0x9c);
        case 5: return range(fields31C[2], 1, 2, 0x9e);
        case 6: return fields31C[13] == 1 ? 0xa1 : fields31C[13] == 2 ? 0x99 : 0;
        default: return 0;
        }
    case 0x22:
        if (fields31C[3] == 1) return locomotion(false);
        return range(fields31C[3], 2, 4, 0xa2);
    default: return 0;
    }
}
