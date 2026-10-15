#include "jlib/jaudio/jaudioeffectcontrols.h"

#include <stdexcept>

jaudio::EffectControls::EffectControls() { reset(); }

void jaudio::EffectControls::reset() {
    constexpr float defaults[] = {1, 0.5f, 1, 0, 0, 0};
    for (unsigned bank = 0; bank < banks.size(); ++bank) {
        for (auto& slot : banks[bank]) {
            slot.current = defaults[bank];
            slot.remaining = 0;
        }
        banks[bank][7].current = -1;
    }
    bindings = {};
}

void jaudio::EffectControls::set(EffectControl control, unsigned slot, float value, std::uint32_t frames) {
    banks.at(static_cast<unsigned>(control)).at(slot & 255).set(value, frames);
}

void jaudio::EffectControls::bind(EffectControl control, const float* value) {
    bindings.at(static_cast<unsigned>(control)) = value;
}

void jaudio::EffectControls::advance(bool paused) {
    if (paused) return;
    for (auto control : {EffectControl::volume, EffectControl::pan, EffectControl::fxmix,
        EffectControl::auxiliary, EffectControl::dolby, EffectControl::pitch}) {
        for (auto& slot : banks[static_cast<unsigned>(control)]) slot.advance();
    }
}

void jaudio::EffectControls::updateSpatial(const EffectSpatialState& state, EffectSpatialServices& services) {
    const unsigned frames = state.soundState == 2 ? 0 : 3;
    set(EffectControl::volume, 4, state.flags & 2 ? 1 : services.distanceVolume((state.flags >> 16) & 7), frames);
    set(EffectControl::pan, 4, services.pan(), frames);
    float pitch = 1;
    if (state.flags & 0x10) pitch -= static_cast<float>(services.random() & 15) / 192.0f;
    if ((state.flags & 0x4000) && !(state.flags & 2) && !(state.flags & 0x300) && state.listenerCount == 1) {
        pitch += state.distance < state.maximumDistance ? 0.2f * (state.distance / state.maximumDistance) : 0.2f;
    }
    if (state.flags & 0xc0) pitch += static_cast<float>(state.pitchOffset) / 192.0f;
    set(EffectControl::pitch, 4, pitch, frames);
    if ((state.flags & 0x300) && state.listenerCount == 1) {
        set(EffectControl::pitch, 1, services.dopplerPitch(), state.soundState == 2 ? 1 : 15);
    }
    unsigned fxmix = state.defaultFxmix;
    if (!(state.flags & 4) && state.listenerCount == 1) {
        fxmix = state.distance >= state.maximumDistance ? state.distantFxmix :
            static_cast<unsigned>(static_cast<float>(state.distantFxmix) * (state.distance / state.maximumDistance));
    }
    if ((fxmix & 0xffff) > 127) fxmix = 127;
    set(EffectControl::fxmix, 4, static_cast<float>(fxmix & 255) / 127.0f, frames);
    if (!(state.flags & 0x400)) set(EffectControl::fxmix, 2, services.roomFxmix(), 0);
    float surround = 0;
    if (state.relativeDepth >= -300) {
        if (state.relativeDepth >= 0) {
            surround = state.relativeDepth < 1000 ? 30 + 97 * (state.relativeDepth / 1000) : 127;
        } else surround = (30 * (-300 - state.relativeDepth)) / -300;
    }
    set(EffectControl::dolby, 4, static_cast<float>(static_cast<int>(surround) & 255) / 127.0f, frames);
}

const jaudio::EffectParameter& jaudio::EffectControls::parameter(EffectControl control, unsigned slot) const {
    return banks.at(static_cast<unsigned>(control)).at(slot & 255);
}

float jaudio::EffectControls::value(EffectControl control, unsigned slot) const { return parameter(control, slot).current; }

float jaudio::EffectControls::combined(EffectControl control, float categoryVolume) {
    if (control == EffectControl::auxiliary) throw std::runtime_error("Auxiliary effect parameter combination is not recovered.");
    const auto index = static_cast<unsigned>(control);
    auto& bank = banks.at(index);
    float result = bank[7].current;
    if (result == -1) {
        if (bindings[index]) bank[0].current = *bindings[index];
        if (control == EffectControl::volume || control == EffectControl::pitch) {
            result = 1;
            for (unsigned slot = 0; slot < 7; ++slot) result *= bank[slot].current;
        } else if (control == EffectControl::pan) {
            result = 0;
            for (unsigned slot = 0; slot < 7; ++slot) {
                if (bank[slot].current != 0.5f) result += bank[slot].current - 0.5f;
            }
            result += 0.5f;
            if (!(0 <= result)) result = 0;
            else if (result > 1) result = 1;
        } else {
            result = 0;
            for (unsigned slot = 0; slot < 7; ++slot) result += bank[slot].current;
            if (control == EffectControl::dolby && result > 1) result = 1;
        }
    }
    if (control == EffectControl::volume) result *= categoryVolume;
    return result;
}
