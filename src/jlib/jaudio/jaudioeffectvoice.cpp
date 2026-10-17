#include "jlib/jaudio/jaudioeffectvoice.h"
#include "jlib/jaudio/jaudioeffectparameterpool.h"
#include "jlib/jaudio/jaudioplayback.h"
#include "jlib/jaudio/jaudioeffectrelease.h"

#include <stdexcept>

jaudio::EffectSequenceVoiceServices::EffectSequenceVoiceServices(Playback& playback, EffectRecord& record,
    unsigned channel, float categoryVolume, unsigned outputMode, EffectReleaseContext& releaseContext)
    : playback(playback), record(record), channel(channel), categoryVolume(categoryVolume), outputMode(outputMode), releaseContext(releaseContext) {
    if (record.state == 2) record.channel = static_cast<std::uint8_t>(channel);
}

void jaudio::EffectSequenceVoiceServices::release() { releaseEffect(record, releaseContext); }

std::uint16_t jaudio::EffectSequenceVoiceServices::readPort(unsigned port) {
    std::uint16_t value = 0;
    if (!playback.readEffectPort(channel, port, value)) throw std::runtime_error("Assigned effect sequence track is unavailable.");
    return value;
}

void jaudio::EffectSequenceVoiceServices::writePort(unsigned port, std::uint16_t value) {
    if (!playback.writeEffectPort(channel, port, value)) throw std::runtime_error("Assigned effect sequence track is unavailable.");
}

void jaudio::EffectSequenceVoiceServices::publishPort(unsigned port, std::uint16_t value) {
    playback.publishEffectPort(channel, port, value);
}

void jaudio::EffectSequenceVoiceServices::publishParameters(EffectControls& controls, unsigned state) {
    if (!record.parameters || &record.parameters->controls != &controls || record.state != state) {
        throw std::runtime_error("Effect sequence publication lost admitted ownership.");
    }
    playback.publishEffectParameters(channel, record, categoryVolume, outputMode);
}

void jaudio::updateEffectVoice(EffectRecord& record, unsigned listenerCount, EffectVoiceServices& services) {
    if (!record.parameters) throw std::runtime_error("Assigned effect requires its admitted parameter block.");
    auto& controls = record.parameters->controls;
    auto& ports = record.parameters->ports;
    auto& pendingPorts = record.parameters->pendingPorts;
    const auto running = services.readPort(2);
    if (record.state == 2) {
        if (record.metadataFlags() & 8) services.initializePosition();
        const unsigned pitchFlags = record.metadataFlags() & 0xc0;
        if (pitchFlags) {
            const unsigned mask = pitchFlags == 0x40 ? 15 : pitchFlags == 0x80 ? 31 : 63;
            record.pitchOffset = static_cast<std::uint8_t>(services.random() & mask);
        }
        for (unsigned port = 0; port < ports.size() && pendingPorts; ++port) {
            const auto bit = static_cast<std::uint16_t>(1u << port);
            if (pendingPorts & bit) {
                services.publishPort(port, ports[port]);
                pendingPorts ^= bit;
            }
        }
        services.updateParameters(controls, record.state, record.pitchOffset);
        services.initializeParameters(controls);
        if (record.stopFrames > 1) {
            controls.set(EffectControl::volume, 6, 0, 0);
            controls.set(EffectControl::volume, 6, 127, record.stopFrames);
            record.stopFrames = 0;
        }
        services.publishParameters(controls, record.state);
        unsigned selection = record.soundId & 0x3ff;
        if (record.metadataFlags() & 0x800) selection += services.variant();
        const unsigned distance = listenerCount == 1 && (record.metadataFlags() & 0x1000) ? services.distanceVariant() : 0;
        services.writePort(3, static_cast<std::uint16_t>(distance));
        services.writePort(6, static_cast<std::uint16_t>(services.roomSize()));
        services.writePort(4, static_cast<std::uint16_t>(selection));
        services.writePort(0, 1);
    } else {
        if (!running || (record.stopFrames && controls.value(EffectControl::volume, 6) == 0)) {
            services.release();
            return;
        }
        if (!record.stopFrames && record.state != 3) return;
        services.updateParameters(controls, record.state, record.pitchOffset);
        services.publishParameters(controls, record.state);
    }
    record.state = record.soundId & 0xc00 ? 4 : 5;
}

