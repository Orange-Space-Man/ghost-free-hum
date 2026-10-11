#include "resources/dolresources.h"
#include "game/player/playeractionupdate.h"
#include "game/startup/startupresources.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    unsigned word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
            (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
    }

    std::array<float, 3> normalize(const std::array<float, 3>& value) {
        const float magnitude = std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
        if (magnitude < 0.0001f) return {};
        const float factor = 1 / magnitude;
        return {value[0] * factor, value[1] * factor, value[2] * factor};
    }

    std::array<float, 3> cross(const std::array<float, 3>& a, const std::array<float, 3>& b) {
        return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    }

    float dot(const std::array<float, 3>& a, const std::array<float, 3>& b) {
        return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2];
    }
}

playeractions::Update::Update(const std::vector<std::uint8_t>& executable, const actormodel::InitialPose& pose) :
    executable(executable), pose(pose) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, 0x804da880, 56, bytes, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < constants.size(); ++i) {
        const auto bits = word(bytes, i * 4);
        std::memcpy(&constants[i], &bits, 4);
    }
    if (!dolresources::read(executable, 0x80351d38, 96, bytes, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < 3; ++i) for (unsigned j = 0; j < 8; ++j) directions[i][j] = word(bytes, i * 32 + j * 4);
}

void playeractions::Update::advanceReaction(Actions& state, const Input& input, const UpdateServices& services) const {
    if (state.field2FC) return;
    bool complete = false;
    if (state.field318 <= constants[0]) complete = input.animationFlags[state.field314 == 8 && state.fields31C[7] == 2 ? 0 : 1] != 0;
    else {
        state.field318 -= constants[1];
        complete = state.field318 <= constants[0];
    }
    switch (state.field314) {
    case 10: case 11: complete |= input.mode == 5 || input.flags[3]; break;
    case 15: complete |= input.mode != 2; break;
    case 16: complete |= input.mode == 2; break;
    case 18: complete |= input.animationFlags[4] || input.flags[1] || input.mode == 6 || input.mode == 5; break;
    case 31: complete |= input.flags[0] != 0; break;
    }
    if (!complete) return;
    ReactionPayload payload;
    const auto reaction = state.field314;
    state.field310 = 1;
    state.field314 = 0;
    const auto begin = [&](unsigned next) { state.beginReaction(next, payload); };
    switch (reaction) {
    case 4:
        if (state.fields31C[6] > 3 && state.fields31C[6] < 7) {
            payload.fields[9] = state.fields31C[8];
            if (state.fields31C[8] == 1) { payload.fields[15] = 1; begin(6); }
            else if (state.fields31C[8] > 0 && state.fields31C[8] < 5) begin(5);
        }
        break;
    case 5: payload.fields[15] = 1; begin(6); break;
    case 6: if (state.field354 == 2) begin(2); break;
    case 7: payload.fields[15] = state.field354; payload.fields[10] = state.fields31C[9]; begin(6); break;
    case 12:
        state.field30D = 0;
        if (state.field35C == 4) services.flags.setOn(15);
        else if (state.field35C == 3) begin(25);
        break;
    case 14:
        if (state.field35C && state.field358) {
            payload.fields[4] = state.field358;
            payload.fields[5] = state.field35C;
            if (state.field35C == 3) services.sound(0x80000032);
            begin(12);
        }
        break;
    case 15: if (input.mode == 2) state.fields31C[4] = 2; else begin(16); break;
    case 17: case 19: state.fields31C[4] = 5; break;
    case 20: state.fields31C[4] = 9; break;
    case 23: begin(24); break;
    case 25: begin(26); break;
    case 27: state.fields31C[4] = 11; break;
    case 31:
        payload.fields[7] = state.fields31C[6]; payload.fields[9] = state.fields31C[8];
        payload.fields[10] = state.fields31C[9]; begin(4);
        break;
    case 32: {
        const auto mode = state.fields31C[0];
        if (mode != 1 && mode != 2 && mode != 4 && mode != 5 && mode != 9) break;
        payload.fields[0] = mode == 4 ? 5 : mode == 9 ? 10 : mode;
        payload.fields[4] = state.field358; payload.fields[12] = state.fields31C[11]; payload.fields[13] = state.fields31C[12];
        begin(32);
        if (mode == 9 && input.actor) input.actor->flags = (input.actor->flags & ~0x800u) |
            (services.catalog.type(input.actor->type()).flags & 0x800);
        break;
    }
    case 33: {
        const auto mode = state.fields31C[1];
        if (mode < 1 || mode > 5) break;
        payload.fields[1] = mode == 1 || mode == 3 ? mode : 3;
        if (mode <= 3) payload.fields[2] = state.fields31C[2];
        else if (state.fields31C[2] == 1) payload.fields[2] = 2;
        else if (state.fields31C[2] == 2) payload.fields[2] = 1;
        payload.fields[4] = state.field358; payload.fields[14] = state.fields31C[13]; begin(33);
        break;
    }
    case 34:
        if (state.fields31C[3] >= 1 && state.fields31C[3] <= 3) {
            payload.fields[3] = state.fields31C[3] == 2 ? 3 : state.fields31C[3]; begin(34);
        }
        break;
    }
}

unsigned playeractions::Update::modeEntry(const Actions& state, const Input& input) const {
    const auto type = input.modes[0], kind = input.modes[1];
    const auto still = [&]() -> unsigned { return state.field30D ? 0x47 : kind == 4 ? 0x45 : kind > 0 && kind < 4 ? 0x43 : 0; };
    switch (state.fields31C[4]) {
    case 1: return 0x34;
    case 2: return 0x6a;
    case 3: return 0x67;
    case 4: return 0x5d;
    case 5:
        if (type == 2) {
            if (input.turn <= constants[0]) {
                if (constants[0] <= input.turn) return still();
                return state.field30D ? 0x4b : 0x49;
            }
            return state.field30D ? 0x4c : 0x4a;
        }
        if (type < 1 || type > 4) return 0;
        return constants[13] <= state.field50 ? 0x4d : still();
    case 6: return kind == 4 ? 0x45 : kind > 0 && kind < 4 ? 0x50 : 0;
    case 7: return 0x51;
    case 8: return 0x52;
    case 9: return 0x3a;
    case 10: return 0x58;
    case 11: return 0x3d;
    case 12: return 0x3e;
    case 13: return 0x3f;
    case 14: return 0x41;
    default: return 0;
    }
}

unsigned playeractions::Update::locomotion(Actions& state, const Input& input) const {
    unsigned entry = 0;
    bool moving = true, stopped = false;
    const auto type = input.modes[0], kind = input.modes[1];
    const auto idle = [&]() -> unsigned { return kind >= 1 && kind <= 4 ? std::array<unsigned, 4>{1, 8, 11, 13}[kind - 1] : 0; };
    const auto aimedIdle = [&]() -> unsigned { return kind == 1 || kind == 2 ? 15 : kind == 3 ? 46 : 0; };
    if (type == 3) {
        const auto forward = normalize(input.direction);
        std::array<float, 3> up{0, 1, 0};
        const std::array<float, 3> difference{up[0] - forward[0], up[1] - forward[1], up[2] - forward[2]};
        if (std::sqrt(dot(difference, difference)) <= 0.0001f) up = {0, 0, 1};
        const auto right = normalize(cross(up, forward));
        const auto angle = pose.angle(-double(dot(forward, input.velocity)), -double(dot(right, input.velocity)));
        float degrees = constants[8] + constants[9] + constants[10] * float(angle);
        if (degrees < constants[0]) degrees += constants[11];
        if (constants[11] <= degrees) degrees -= constants[11];
        const unsigned sector = static_cast<unsigned>(constants[12] * degrees);
        if (input.thresholds[0] <= state.field50) {
            const unsigned speed = input.thresholds[1] <= state.field50 ? (constants[7] <= state.field50 ? 2 : 1) : 0;
            entry = input.flags[2] ? 0x2b + speed : directions[speed].at(sector);
        } else {
            entry = aimedIdle();
            if (kind == 1) entry = input.flags[4] ? 0x10 : input.flags[6] ? 0x11 : input.flags[2] ? 0x2a : 0xf;
            stopped = true; moving = false;
        }
    } else if (type == 1) {
        if (input.thresholds[0] <= state.field50) {
            if (kind == 1) entry = input.thresholds[1] <= state.field50 ? (constants[7] <= state.field50 ? 6 : input.flags[5] ? 5 : 4) : input.flags[5] ? 3 : 2;
            else if (kind == 2) entry = input.thresholds[1] <= state.field50 && constants[7] <= state.field50 ? 10 : 9;
            else if (kind == 3) entry = 12;
            else if (kind == 4) entry = 14;
        } else { entry = idle(); stopped = true; moving = false; }
    } else if (type == 2) {
        if (input.turn <= constants[0] && constants[0] <= input.turn) {
            entry = state.field30D ? aimedIdle() : idle(); stopped = true;
        } else if (state.field30D) entry = input.turn <= constants[0] ? 0x18 : 0x1b;
        else if (kind == 4) entry = 14;
        else if (kind >= 1 && kind <= 3) entry = input.turn <= constants[0] ? 0x2f : 0x30;
        moving = false;
    } else if (type == 4) {
        entry = input.modes[2] >= 1 && input.modes[2] <= 3 ? 0x30 + input.modes[2] : 0;
        moving = false;
    }
    if (moving) { state.fields304[0] = 1; ++state.field300; }
    else if (!stopped) state.field300 = 0;
    if (stopped) {
        state.fields304[2] = 1;
        if (state.field300 == 0 || state.field300 > 4) state.field300 = 0;
        else {
            if (input.animationFlags[input.animationFlags[2] ? 3 : 0]) state.field300 = 0;
            if (state.field300) entry = state.field64;
        }
    }
    return entry;
}

unsigned playeractions::Update::select(Actions& state, const Input& input) const {
    state.fields304[1] = state.fields304[0];
    state.fields304[0] = state.fields304[2] = 0;
    if (state.field310 == 2) { state.field300 = 0; return state.reactionEntry(); }
    if (state.field310 != 1) return 0;
    if (!state.field30C && !(state.field308 < constants[6])) return 0x35;
    if (!state.fields31C[4]) return locomotion(state, input);
    state.field300 = 0;
    return modeEntry(state, input);
}

unsigned playeractions::Update::run(Actions& state, const Input& input, const UpdateServices& services) const {
    state.fields0 = input.modes;
    state.field10 = input.flags[0]; state.field11 = input.flags[1]; state.field14 = input.flags[4];
    state.field20 = input.frame; state.fields24 = input.thresholds; state.field2C = input.maximumSpeed;
    std::memcpy(&state.fields30[0], &input.motion, 4); std::memcpy(&state.fields30[1], &input.turn, 4);
    state.field38 = input.useMotion;
    for (unsigned i = 0; i < state.fields39.size(); ++i) state.fields39[i] = input.animationFlags[i];
    state.field3F = input.animationFlags[6]; state.fields40[0] = input.mode; state.attached = input.attached;
    state.actor = input.actor;
    state.field64 = state.field60;
    state.field308 = !state.field30C || (state.field310 == 2 && state.field314 == 30) ? state.field308 + constants[1] : constants[0];
    state.field50 = input.useMotion ? input.motion : std::sqrt(input.velocity[0] * input.velocity[0] + input.velocity[2] * input.velocity[2]) / input.maximumSpeed;
    if (state.field310 == 2) advanceReaction(state, input, services);
    if (state.field310 != 2 || state.field314 == 30 || state.field314 == 10) {
        if (!input.flags[0]) state.field30C = 0;
        else {
            if (!state.field30C && constants[2] <= state.field308) {
                state.beginReaction(1, ReactionPayload{});
                if (input.controller) input.controller->play(executable, 1);
            }
            state.field30C = 1;
        }
    }
    if (state.field310 == 1) state.updateNormalState();
    state.field60 = select(state, input);
    if (state.field310 == 2 && state.entries.at(state.field60) == state.parameters[2] && state.field318 <= constants[0])
        state.field318 = static_cast<float>(services.fallbackDuration(state.field60));
    state.field2FC = 0;
    return state.entries.at(state.field360 ? state.field360 : state.field60);
}
