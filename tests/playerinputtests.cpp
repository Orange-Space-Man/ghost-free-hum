#include "game/player/playerinputdecoder.h"
#include "game/player/playerpadsampler.h"
#include "platform/windows/windowpadinput.h"
#include "resources/gameassetpackage.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    bool near(float first, float second) { return std::abs(first - second) < 0.00001f; }

    std::vector<std::uint8_t> fixture() {
        std::vector<std::uint8_t> data(0x10100);
        const auto word = [&](unsigned offset, unsigned value) {
            for (unsigned i = 0; i < 4; ++i) data.at(offset + i) = static_cast<std::uint8_t>(value >> (24 - i * 8));
        };
        word(0, 0x100);
        word(0x48, 0x804d0000);
        word(0x90, 0x10000);
        const auto scalar = [&](unsigned address, float value) {
            unsigned bits;
            std::memcpy(&bits, &value, 4);
            word(address - 0x804d0000 + 0x100, bits);
        };
        word(0x7f58, 1);
        word(0x7ff0, 0x1eb40f48);
        word(0x7ff4, 0x280f3b1f);
        scalar(0x804dd6c8, -0.25f); scalar(0x804dd6cc, 0.25f);
        scalar(0x804dd6d0, -0.5f); scalar(0x804dd6d4, 0.5f);
        scalar(0x804daa84, 0.5f); scalar(0x804daa88, 0.0001f);
        scalar(0x804daa94, 15); scalar(0x804daa90, 170);
        scalar(0x804daaa0, 0.7f); scalar(0x804daab0, 0.33333337f);
        return data;
    }

    void check(const std::vector<std::uint8_t>& data) {
        playerinput::PadSampler sampler(data);
        {
            playerinput::HostPad host{0xf31f, -32768, 32767, -32768, 32767, 249, 250};
            const auto status = playerinput::translate(host);
            require(status.stickX == -87 && status.stickY == 87 && status.substickX == -74 && status.substickY == 74,
                "Host stick endpoints map symmetrically into the original unclamped main and C-stick ranges.");
            require(status.buttons == 0x1f7f && status.analogA == 255 && status.analogB == 255,
                "Host buttons and full trigger clicks map into their original GameCube bits.");
            host.buttons = 0;
            const auto triggers = playerinput::translate(host);
            require(triggers.buttons == 0x20 && triggers.triggerLeft == 249 && triggers.triggerRight == 250,
                "Analog trigger values reach PADClamp independently of the host full-click threshold.");
            playerinput::Pad sampled;
            sampler.update(sampled, status);
            require(sampled.stickMagnitude == 1 && sampled.substickMagnitude == 1 && sampled.triggerRight == 150,
                "Translated host input still passes through the original clamp and normalization stage.");
            const auto disabled = playerinput::pollWindowPad(nullptr, false);
            sampler.update(sampled, disabled.status);
            require(sampled.held == 0 && sampled.released != 0 && disabled.port == -1,
                "Disabled or unfocused host input releases prior controls instead of retaining a stuck key.");
            sampler.update(sampled, disabled.status);
            require(sampled.released == 0, "Release edges occur once at the simulation sampling cadence.");
        }
        playerinput::Decoder decoder(data);
        playerinput::Pad pad{0};
        playerinput::PadStatus raw;
        raw.buttons = 0x100;
        raw.analogA = 15;
        raw.stickX = 15;
        raw.stickY = -15;
        raw.triggerLeft = 30;
        sampler.update(pad, raw);
        require(pad.held == 0x100 && pad.pressed == 0x100 && pad.released == 0 && pad.stickMagnitude == 0 && pad.triggerLeft == 0,
            "Inclusive SDK dead zones must preserve digital A without creating stick or trigger input.");
        sampler.update(pad, raw);
        require(pad.pressed == 0 && pad.held == 0x100, "Held input must not repeat its press edge.");
        raw.stickX = 42;
        raw.stickY = 0;
        raw.substickY = 36;
        raw.triggerLeft = 255;
        sampler.update(pad, raw);
        require(pad.stick[0] == 0.5f && pad.substick[1] == 0.5f && pad.triggerLeft == 150 &&
            pad.pressed == 0x02080000, "Stick directions include the half threshold; triggers clamp before subtracting their dead zone.");
        raw.error = -2;
        sampler.update(pad, raw);
        require(pad.pressed == 0 && pad.released == 0 && pad.stick[0] == 0.5f && pad.held == 0x02080100,
            "Transient pad errors retain previous state while clearing edges.");
        raw.error = -1;
        sampler.update(pad, raw);
        require(pad.held == 0 && pad.released == 0x02080100 && pad.stickMagnitude == 0 && pad.analogA == 0,
            "Disconnected pads release held buttons and clear analog state.");
        raw = {};
        raw.stickX = 127;
        raw.stickY = 127;
        sampler.update(pad, raw);
        require(near(pad.stick[0], std::sqrt(0.5f)) && near(pad.stick[1], std::sqrt(0.5f)) && pad.stickMagnitude == 1,
            "Octagonal clamp must precede circular JUT normalization.");
        raw.stickX = -128;
        raw.stickY = 16;
        sampler.update(pad, raw);
        require(pad.stick[0] == -1 && pad.stick[1] == 0, "SDK integer clamp must retain asymmetric-axis truncation.");

        playercontrols::State state;
        playerinput::Context context;
        context.pad = &pad;
        raw = {};
        raw.buttons = 0x300;
        raw.analogA = 15;
        raw.analogB = 7;
        sampler.update(pad, raw);
        decoder.update(state, context);
        require(state.flags0 == std::array<std::uint8_t, 4>{1, 1, 1, 1} && state.field4 == 1 && near(state.field8, 7.0f / 15),
            "Default A/B bindings must preserve digital edges and separate analog pressure.");
        sampler.update(pad, raw);
        decoder.update(state, context);
        require(state.flags0 == std::array<std::uint8_t, 4>{0, 1, 0, 1} && state.flagsC[7] && !state.flagsC[4],
            "Action aliases must distinguish held A from its first-frame edge.");
        context.edges = false;
        raw.buttons = 0x1000;
        sampler.update(pad, raw);
        decoder.update(state, context);
        require(!state.flagsC[0], "Disabled edge processing must suppress Start.");
        context.edges = true;
        raw = {};
        raw.stickX = 69;
        raw.substickY = 57;
        sampler.update(pad, raw);
        decoder.update(state, context);
        require(state.motions[0].first[1] == 1 && state.motions[2].first[0] == -1 && state.flagsC[16],
            "Original standard preset uses the substick for motion zero and main stick for motion two.");
        state.setMode(1);
        decoder.update(state, context);
        require(state.motions[0].first[0] == 1 && state.motions[2].first[1] == -1 && !state.flagsC[16],
            "Alternative preset swaps the sticks without reintroducing a held-direction edge.");
        context.view = {0, 0, 1, 50, 0, 1, 0, 60, -1, 0, 0, 70};
        state.settings.fields[38] = 1;
        decoder.update(state, context);
        require(state.motions[0].first == std::array<float, 3>{0, 0, 1} &&
            state.motions[0].second == std::array<float, 3>{0, 0, 1}, "Camera-relative direction must use transpose and ignore translation.");
        context.view = playerinput::Context{}.view;
        context.orientation = {0, 1, 0, 0};
        state.settings.fields[38] = 2;
        decoder.update(state, context);
        require(state.motions[0].first[0] == -1 && state.motions[0].second[0] == -1,
            "Player-relative mode must concatenate the original quaternion matrix before transposing.");
        state.settings.fields[38] = 0;
        state.settings.fields[39] = 2;
        decoder.update(state, context);
        require(state.motions[0].first[0] == -1, "Horizontal inversion must apply after transformation.");
        require(near(decoder.magnitude(0.3f, 0.3f), std::sqrt(0.18f)) &&
            decoder.magnitude(0.3f, 0.299f) > decoder.magnitude(0.3f, 0.3f),
            "Exact equal diagonals preserve the original correction discontinuity.");
        require(decoder.direction({1, {}, {0.8f, 0, 0.8f}}) == 0 && decoder.direction({1, {}, {-1, 0, 0}}) == 3,
            "Direction classification excludes strong diagonals and retains signed cardinal codes.");
        state.settings.fields[0] = 0x8000006;
        state.settings.fields[2] = 0x8000005;
        state.settings.fields[3] = 12;
        state.settings.fields[4] = 11;
        pad.held = pad.pressed = 0x300;
        pad.analogA = 15;
        pad.analogB = 3;
        decoder.update(state, context);
        require(state.flags0 == std::array<std::uint8_t, 4>{1, 1, 1, 1} && state.field4 == 1 && near(state.field8, 0.2f),
            "Recursive A/B aliases must fall back to their physical buttons and pressure channels.");
        state.settings.fields[37] = 5;
        state.settings.fields[38] = state.settings.fields[39] = 0;
        pad.triggerLeft = 0;
        pad.triggerRight = 119;
        decoder.update(state, context);
        require(near(state.motions[0].value, 0.7f) && state.motions[0].first[0] == 1,
            "Analog trigger difference must use the original 170-unit range rather than the SDK maximum.");
        state.settings.fields[37] = 6;
        decoder.update(state, context);
        require(state.motions[0].value == 0, "Digital trigger motion uses a strict threshold.");
        pad.triggerRight = 120;
        decoder.update(state, context);
        require(state.motions[0].value == 1, "Digital trigger motion starts above the threshold.");
        pad.triggerLeft = 120;
        decoder.update(state, context);
        require(state.motions[0].value == 0, "Equal trigger input cancels rather than preferring one trigger.");
        state.settings.fields[37] = 7;
        pad.held = 15;
        decoder.update(state, context);
        require(state.motions[0].value == 0, "Opposing D-pad directions must cancel on both axes.");
        state.setMode(1);
        context.suppress = true;
        raw.stickX = -69;
        raw.substickY = 0;
        sampler.update(pad, raw);
        decoder.update(state, context);
        require(state.flagsC[16] && state.motions[0].value == 0 && state.history[0].value == 1,
            "Suppression must retain only the direction-change event, while still advancing input history.");
        decoder.update(state, context);
        require(!state.flagsC[16], "Suppressed direction events must not repeat.");
        context.pad = nullptr;
        decoder.update(state, context);
        require(state.history[0].value == 1 && state.motions[0].value == 0,
            "Null-pad reset preserves history, matching the original pause gate.");
    }
}

int main(int argc, char** argv) {
    try {
        check(fixture());
        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> executable;
            std::string error;
            require(package.open(argv[1], error) && package.read("sys/main.dol", executable, error), error.c_str());
            check(executable);
        }
        std::cout << "Original pad sampling and player input checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
