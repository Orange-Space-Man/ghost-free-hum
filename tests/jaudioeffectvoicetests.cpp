#include "jlib/jaudio/jaudioeffectvoice.h"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    struct Services : jaudio::EffectVoiceServices {
        std::vector<unsigned> calls;
        std::array<std::uint16_t, 16> output{}, published{};
        unsigned running = 1, observedState = 0, observedOffset = 0;
        float volume = -1;
        std::uint16_t readPort(unsigned port) override { require(port == 2, "Wrong completion port."); calls.push_back(1); return running; }
        void writePort(unsigned port, std::uint16_t value) override { calls.push_back(100 + port); output.at(port) = value; }
        void publishPort(unsigned port, std::uint16_t value) override { calls.push_back(200 + port); published.at(port) = value; }
        std::uint32_t random() override { calls.push_back(2); return 63; }
        void initializePosition() override { calls.push_back(3); }
        void updateParameters(jaudio::EffectControls& controls, unsigned state, std::uint8_t offset) override {
            calls.push_back(4); observedState = state; observedOffset = offset;
            controls.set(jaudio::EffectControl::volume, 4, 0.25f, state == 2 ? 0 : 3);
        }
        void initializeParameters(jaudio::EffectControls& controls) override {
            calls.push_back(5); controls.set(jaudio::EffectControl::volume, 5, 0.5f, 0);
        }
        void publishParameters(jaudio::EffectControls& controls, unsigned) override {
            calls.push_back(6); volume = controls.combined(jaudio::EffectControl::volume);
        }
        unsigned variant() override { calls.push_back(7); return 4; }
        unsigned distanceVariant() override { calls.push_back(8); return 9; }
        unsigned roomSize() override { calls.push_back(9); return 2; }
        void release() override { calls.push_back(10); }
    };
}

int main() {
    try {
        jaudio::EffectVoice voice;
        Services services;
        voice.soundId = 0x828;
        voice.metadata = 8 | 0x40 | 0x800 | 0x1000;
        voice.pendingPorts = (1u << 1) | (1u << 15);
        voice.ports[1] = 123; voice.ports[15] = 0xffff;
        voice.update(services);
        require(services.calls == std::vector<unsigned>{1, 3, 2, 201, 215, 4, 5, 6, 7, 8, 103, 9, 106, 104, 100},
            "Original assigned-sound startup order changed.");
        require(voice.state == 4 && services.observedState == 2 && services.observedOffset == 15 && voice.pendingPorts == 0,
            "Sound startup did not retain pitch offset or clear published ports.");
        require(services.output[4] == 44 && services.output[3] == 9 && services.output[6] == 2 && services.output[0] == 1 &&
            services.published[15] == 0xffff && services.volume == 0.125f, "Sound startup publication changed.");
        services.calls.clear();
        voice.update(services);
        require(services.calls == std::vector<unsigned>{1}, "Stable state four must not refresh targets unconditionally.");
        voice.state = 3;
        services.calls.clear();
        voice.update(services);
        require(services.calls == std::vector<unsigned>{1, 4, 6} && voice.state == 4, "State three refresh path changed.");
        voice.stopFrames = 3;
        voice.controls.set(jaudio::EffectControl::volume, 6, 0, 3);
        voice.controls.advance(false);
        services.calls.clear();
        voice.update(services);
        require(services.calls == std::vector<unsigned>{1, 4, 6}, "Pending fade released before slot six reached zero.");
        voice.controls.advance(false); voice.controls.advance(false);
        services.calls.clear();
        voice.update(services);
        require(services.calls == std::vector<unsigned>{1, 10}, "Completed fade failed to release through its owner.");
        voice = {};
        services = {};
        voice.stopFrames = 4;
        voice.update(services);
        require(voice.state == 5 && voice.stopFrames == 0 && services.volume == 0 &&
            voice.controls.parameter(jaudio::EffectControl::volume, 6).target == 127,
            "Original startup fade target or publication order changed.");
        voice.controls.advance(false);
        require(voice.controls.value(jaudio::EffectControl::volume, 6) == 31.75f, "Startup fade was normalized instead of preserved.");
        services.running = 0; services.calls.clear();
        voice.update(services);
        require(services.calls == std::vector<unsigned>{1, 10}, "Sequence completion must release before updating parameters.");
        for (unsigned flags : {0u, 0x40u, 0x80u, 0xc0u}) {
            voice = {}; services = {};
            voice.metadata = flags | 0x1000;
            voice.listenerCount = 2;
            voice.update(services);
            require(voice.pitchOffset == (flags == 0 ? 0 : flags == 0x40 ? 15 : flags == 0x80 ? 31 : 63) && services.output[3] == 0,
                "Pitch mask or multiple-listener distance gating changed.");
        }
        std::cout << "Effect voice lifecycle passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
