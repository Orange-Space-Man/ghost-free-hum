#include "jlib/jaudio/jaudioeffectvoice.h"

void jaudio::EffectVoice::update(EffectVoiceServices& services) {
    const auto running = services.readPort(2);
    if (state == 2) {
        if (metadata & 8) services.initializePosition();
        const unsigned pitchFlags = metadata & 0xc0;
        if (pitchFlags) {
            const unsigned mask = pitchFlags == 0x40 ? 15 : pitchFlags == 0x80 ? 31 : 63;
            pitchOffset = static_cast<std::uint8_t>(services.random() & mask);
        }
        for (unsigned port = 0; port < ports.size() && pendingPorts; ++port) {
            const auto bit = static_cast<std::uint16_t>(1u << port);
            if (pendingPorts & bit) {
                services.publishPort(port, ports[port]);
                pendingPorts ^= bit;
            }
        }
        services.updateParameters(controls, state, pitchOffset);
        services.initializeParameters(controls);
        if (stopFrames > 1) {
            controls.set(EffectControl::volume, 6, 0, 0);
            controls.set(EffectControl::volume, 6, 127, stopFrames);
            stopFrames = 0;
        }
        services.publishParameters(controls, state);
        unsigned selection = soundId & 0x3ff;
        if (metadata & 0x800) selection += services.variant();
        const unsigned distance = listenerCount == 1 && (metadata & 0x1000) ? services.distanceVariant() : 0;
        services.writePort(3, static_cast<std::uint16_t>(distance));
        services.writePort(6, static_cast<std::uint16_t>(services.roomSize()));
        services.writePort(4, static_cast<std::uint16_t>(selection));
        services.writePort(0, 1);
    } else {
        if (!running || (stopFrames && controls.value(EffectControl::volume, 6) == 0)) {
            services.release();
            return;
        }
        if (!stopFrames && state != 3) return;
        services.updateParameters(controls, state, pitchOffset);
        services.publishParameters(controls, state);
    }
    state = soundId & 0xc00 ? 4 : 5;
}
