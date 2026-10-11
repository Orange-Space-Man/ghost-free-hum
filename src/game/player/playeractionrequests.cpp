#include "game/player/playeractionstate.h"
#include "game/player/playerattachedstate.h"
#include "game/actors/moveactor.h"

#include <stdexcept>

bool playeractions::Actions::request(unsigned message, const std::array<std::uint32_t, 12>* payload) {
    const auto read = [&](unsigned index) {
        if (!payload) throw std::runtime_error("Original player action request requires its payload.");
        return payload->at(index);
    };
    bool accepted = false;
    if (field310 == 2 && field314 == 0x20) {
        auto& stage = fields31C[0];
        switch (message) {
        case 0x13: if (stage == 1) { field314 = 0; accepted = true; } break;
        case 0x14: if (stage == 1) { stage = 2; accepted = true; } break;
        case 0x15:
            if (stage == 1 || stage == 2) {
                if (fields31C[12] == 2) stage = 4;
                else if (fields31C[12] == 1) stage = 3;
                else if (fields31C[12] == 3) {
                    stage = 9;
                    if (actor) actor->flags |= 0x800;
                }
                accepted = true;
            } else if (stage == 3 && fields31C[12] == 1) { stage = 8; accepted = true; }
            break;
        case 0x16:
            if (stage == 1 || stage == 2) { field310 = 1; field314 = 0; stage = 0; accepted = true; }
            break;
        case 0x17: if (stage == 5) { stage = 6; accepted = true; } break;
        case 0x18: if (stage == 5) { stage = 7; accepted = true; } break;
        case 0x19: if (stage == 10) { stage = 11; accepted = true; } break;
        }
        if (accepted) field318 = 0;
    } else if (field310 == 2 && field314 == 0x21) {
        auto& stage = fields31C[1];
        switch (message) {
        case 0x1a: if (stage == 1) { field314 = 0; accepted = true; } break;
        case 0x1b: if (stage == 1) { stage = 2; accepted = true; } break;
        case 0x1c: if (stage == 3) { stage = 4; accepted = true; } break;
        case 0x1d: if (stage == 3) { stage = 5; accepted = true; } break;
        case 0x1e:
            if (stage >= 3 && stage <= 5) { fields31C[13] = read(10); stage = 6; accepted = true; }
            break;
        case 0x1f:
            if (stage >= 3 && stage <= 5) { field310 = 1; field314 = 0; stage = 0; accepted = true; }
            break;
        }
        if (accepted) field318 = 0;
    } else if (field310 == 2 && field314 == 0x22) {
        if (message == 0x20 && fields31C[3] == 3) { fields31C[3] = 4; field318 = 0; accepted = true; }
    } else {
        ReactionPayload values;
        const auto begin = [&](unsigned reaction) { beginReaction(reaction, values); accepted = true; };
        switch (message) {
        case 1: begin(2); break;
        case 2: values.fields[6] = read(0); begin(3); break;
        case 3:
            values.fields[7] = read(1);
            if (values.fields[7] == 1) {
                if (attached) {
                    attached->startFirstTransition(4, 1);
                    attached->startFirstTransition(4, 1);
                }
                accepted = true;
            } else { values.fields[9] = read(3); values.fields[10] = read(4); begin(4); }
            break;
        case 4: values.fields[15] = read(11); values.fields[10] = read(4); begin(7); break;
        case 5: values.fields[8] = read(2); begin(8); break;
        case 6: begin(9); break;
        case 7: begin(11); break;
        case 8: begin(13); break;
        case 9: values.fields[4] = read(6); values.fields[5] = read(7); begin(14); break;
        case 10: values.fields[15] = read(11); begin(6); break;
        case 11: begin(29); break;
        case 12:
            values.fields[0] = 1; values.fields[4] = read(6); values.fields[12] = read(8); values.fields[13] = read(9);
            begin(32);
            break;
        case 13:
            values.fields[1] = 1; values.fields[4] = read(6); values.fields[14] = read(10); values.fields[2] = 2;
            begin(33);
            break;
        case 14: values.fields[3] = 2; beginReaction(34, values); break;
        case 15: begin(23); break;
        case 16: begin(30); break;
        case 17: values.fields[7] = read(1); values.fields[9] = read(3); values.fields[10] = read(4); begin(31); break;
        case 18:
            if (field310 == 1 || field314 == 17 || field314 == 18) { values.fields[11] = read(5); begin(19); }
            break;
        }
    }
    if (accepted) field2FC = 1;
    return accepted;
}
