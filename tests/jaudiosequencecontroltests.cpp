#include "jlib/jaudio/jaudioplayback.h"
#include "jlib/jaudio/jaudioeffectcontrols.h"

#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    struct SpatialServices : jaudio::EffectSpatialServices {
        std::vector<unsigned> calls;
        unsigned curve = 0;
        float distanceVolume(unsigned value) override { curve = value; calls.push_back(1); return 0.25f; }
        float pan() override { calls.push_back(2); return 0.75f; }
        float dopplerPitch() override { calls.push_back(3); return 2; }
        float roomFxmix() override { calls.push_back(4); return 0.125f; }
        std::uint32_t random() override { calls.push_back(5); return 15; }
    };
}

int main(int argc, char** argv) {
    try {
        using Effect = jaudio::EffectControl;
        jaudio::EffectControls effect;
        require(effect.value(Effect::volume, 4) == 1 && effect.value(Effect::pan, 4) == 0.5f &&
            effect.value(Effect::fxmix, 4) == 0 && effect.value(Effect::dolby, 4) == 0 && effect.value(Effect::pitch, 7) == -1,
            "Effect slot initialization changed.");
        effect.set(Effect::volume, 0x104, 0, 4);
        effect.advance(true);
        require(effect.value(Effect::volume, 4) == 1, "Paused effect bank advanced.");
        effect.advance(false);
        require(effect.value(Effect::volume, 4) == 0.75f, "Effect slot ramp changed.");
        effect.set(Effect::volume, 4, 0, 4);
        require(effect.parameter(Effect::volume, 4).remaining == 4, "Repeated effect target must restart its ramp.");
        effect.set(Effect::volume, 4, 0.5f, 0);
        effect.advance(false);
        require(effect.value(Effect::volume, 4) == 0.3125f, "Immediate effect write must preserve the pending ramp step.");
        effect.reset();
        require(effect.value(Effect::volume, 4) == 1 && effect.parameter(Effect::volume, 4).remaining == 0,
            "Recycled effect bank retained active interpolation.");
        effect.set(Effect::volume, 4, 0.5f, 0);
        effect.set(Effect::volume, 1, 0.5f, 0);
        require(effect.value(Effect::volume, 4) == 0.5f && effect.combined(Effect::volume, 0.5f) == 0.125f,
            "Slot-four query was replaced by combined volume.");
        float bound = 0.25f;
        effect.bind(Effect::volume, &bound);
        require(effect.combined(Effect::volume) == 0.0625f && effect.value(Effect::volume, 0) == bound,
            "Effect source binding failed to refresh slot zero.");
        effect.set(Effect::volume, 7, 2, 0);
        bound = 0.75f;
        require(effect.combined(Effect::volume, 0.5f) == 1 && effect.value(Effect::volume, 0) == 0.25f,
            "Override must bypass slot combination and binding reads.");
        effect.set(Effect::pan, 1, 1, 0);
        effect.set(Effect::pan, 4, 1, 0);
        require(effect.combined(Effect::pan) == 1, "Combined pan must clamp after adding centered slots.");
        effect.set(Effect::pan, 7, 2, 0);
        require(effect.combined(Effect::pan) == 2, "Pan override must bypass clamping.");
        effect.set(Effect::fxmix, 1, 0.75f, 0);
        effect.set(Effect::fxmix, 4, 0.75f, 0);
        require(effect.combined(Effect::fxmix) == 1.5f, "Effect send sum was clamped.");
        effect.set(Effect::dolby, 4, -0.5f, 0);
        require(effect.combined(Effect::dolby) == -0.5f, "Surround sum gained a lower clamp.");
        effect.set(Effect::dolby, 4, 2, 0);
        require(effect.combined(Effect::dolby) == 1, "Surround sum lost its upper clamp.");
        effect.reset();
        require(effect.combined(Effect::volume) == 1, "Effect reset retained an external binding.");
        jaudio::EffectSpatialState spatialState;
        SpatialServices spatialServices;
        spatialState.maximumDistance = 1000;
        spatialState.distance = 500;
        spatialState.flags = 3u << 16;
        effect.updateSpatial(spatialState, spatialServices);
        require(spatialServices.calls == std::vector<unsigned>{1, 2, 4} && spatialServices.curve == 3,
            "Original spatial producer order or attenuation curve changed.");
        require(effect.value(Effect::volume, 4) == 0.25f && effect.value(Effect::pan, 4) == 0.75f &&
            effect.value(Effect::fxmix, 4) == 40.0f / 127 && effect.value(Effect::fxmix, 2) == 0.125f &&
            effect.value(Effect::dolby, 4) == 30.0f / 127,
            "Initial spatial values were not written to their original slots.");
        effect.reset();
        spatialState.soundState = 3;
        effect.updateSpatial(spatialState, spatialServices);
        require(effect.combined(Effect::volume) == 1 && effect.value(Effect::fxmix, 2) == 0.125f,
            "Spatial publication must precede ramp advancement, except immediate room send.");
        effect.advance(false);
        require(effect.value(Effect::volume, 4) == 0.75f, "Three-frame spatial ramp changed.");
        effect.reset();
        spatialState.soundState = 2;
        spatialState.flags = 0x4000 | 0x10 | 0x40;
        spatialState.pitchOffset = 12;
        spatialServices.calls.clear();
        effect.updateSpatial(spatialState, spatialServices);
        require(spatialServices.calls == std::vector<unsigned>{1, 2, 5, 4}, "Pitch random sampling order changed.");
        const float expectedPitch = ((1 - 15.0f / 192) + 0.2f * 0.5f) + 12.0f / 192;
        require(effect.value(Effect::pitch, 4) == expectedPitch, "Original pitch flag combination changed.");
        spatialState.flags = 0x300 | 0x4000;
        effect.updateSpatial(spatialState, spatialServices);
        require(effect.value(Effect::pitch, 4) == 1 && effect.value(Effect::pitch, 1) == 1 &&
            effect.parameter(Effect::pitch, 1).remaining == 1, "Doppler must suppress distance pitch and use one initial update.");
        effect.advance(false);
        require(effect.value(Effect::pitch, 1) == 2, "Doppler pitch did not reach its separate slot.");
        spatialState.flags = 2 | 4 | 0x400;
        spatialState.defaultFxmix = 255;
        spatialServices.calls.clear();
        effect.updateSpatial(spatialState, spatialServices);
        require(spatialServices.calls == std::vector<unsigned>{2} && effect.value(Effect::volume, 4) == 1 &&
            effect.value(Effect::fxmix, 4) == 1, "Spatial disable flags or default send clamp changed.");
        for (const auto& sample : std::array<std::pair<float, unsigned>, 5>{{{-301, 0}, {-300, 0}, {-150, 15}, {0, 30}, {1000, 127}}}) {
            spatialState.relativeDepth = sample.first;
            effect.updateSpatial(spatialState, spatialServices);
            require(effect.value(Effect::dolby, 4) == static_cast<float>(sample.second) / 127, "Surround depth boundary changed.");
        }
        jaudio::SequenceControls controls;
        require(controls.track(4).pan.value == 0.5f && controls.track(4).fxmix.value == 0 && controls.track(4).dolby.value == 0,
            "Original spatial parameter defaults changed.");
        controls.setPan(0x104, 1, 2);
        controls.setFxmix(4, 0.5f, 2);
        controls.setDolby(4, 1, 2, 2);
        require(controls.value(4, jaudio::SequenceParameter::pan, 3) == -1, "Unready sequence must not expose parameters.");
        controls.update();
        require(controls.value(4, jaudio::SequenceParameter::pan, 4) == 0.75f &&
            controls.value(4, jaudio::SequenceParameter::fxmix, 5) == 0.25f && controls.track(4).dolby.value == 0.5f,
            "Spatial interpolation or active-state query changed.");
        controls.setPan(4, 1, 60);
        require(controls.track(4).pan.remaining == 1, "Repeated pan target restarted its ramp.");
        controls.update();
        require(controls.track(4).pan.value == 1 && controls.track(4).fxmix.value == 0.5f && controls.track(4).dolby.value == 1,
            "Spatial controls failed to reach their targets.");
        controls.setDolby(4, 0.75f, 2, 1);
        controls.update();
        require(controls.track(4).dolby.value == 0.5f, "Non-surround output did not ramp toward zero.");
        controls.update();
        controls.setDolby(4, 1, 2, 2);
        controls.setDolby(4, 1, 2, 0);
        controls.update();
        require(controls.track(4).dolby.value == 0.5f, "Original zero-current surround early return was changed.");
        require(jaudio::blendTrackParameter(0.25f, 0.75f, 0.5f, 0) == 0.25f &&
            jaudio::blendTrackParameter(0.25f, 0.75f, 0.5f, 1) == 0.75f &&
            jaudio::blendTrackParameter(0.25f, 0.75f, 0.5f, 0x102) == 0.5f,
            "Original spatial blend mode selection changed.");
        require(jaudio::blendTrackParameter(0, 1, 2, 2) == 2, "Original blend must not clamp its weight or result.");
        controls.setVolume(4, 0, 4);
        controls.update();
        require(controls.track(4).volume.value == 0.75f, "Volume interpolation step changed.");
        controls.setVolume(4, 0, 60);
        require(controls.track(4).volume.remaining == 3, "Repeated target restarted the original ramp.");
        controls.update(); controls.update(); controls.update();
        require(controls.track(4).volume.value == 0 && !controls.track(4).volume.remaining, "Volume failed to reach its exact target.");
        controls.setVolume(4, 1, 0);
        require(controls.track(4).volume.value == 0, "Zero duration must still wait for parameter update.");
        controls.setMuted(4, true);
        require(!controls.track(4).muted, "Mute request applied before its frame update.");
        controls.update();
        require(controls.track(4).volume.value == 1 && controls.track(4).muted, "Pending controls did not commit.");
        controls.setVolume(0x104, 0.5f, 1);
        controls.update();
        require(controls.track(4).volume.value == 0.5f, "Original byte track index changed.");
        controls.setPitch(0x104, 2, 4);
        require(controls.track(4).pitch.value == 1, "Pitch was published before its frame update.");
        controls.update();
        require(controls.track(4).pitch.value == 1.25f && controls.track(4).volume.value == 0.5f, "Pitch ramp changed another parameter.");
        controls.setPitch(4, 2, 60);
        require(controls.track(4).pitch.remaining == 3, "Repeated pitch request restarted interpolation.");
        controls.update(); controls.update(); controls.update();
        require(controls.track(4).pitch.value == 2 && !controls.track(4).pitch.remaining, "Pitch failed to reach its target.");
        controls.setPitch(4, 0.5f, 0);
        controls.update();
        require(controls.track(4).pitch.value == 0.5f, "Zero-duration pitch did not publish on the next update.");

        jaudio::Interpreter sequence;
        std::string error;
        sequence.start({0xc1, 4, 0, 0, 9, 0x80, 100, 0xff, 0, 0xe7, 0, 0, 0x80, 100, 0xff});
        for (unsigned i = 0; i < 10 && !sequence.track(1).externalControl; ++i)
            if (!sequence.advance([](const auto&) {}, error)) throw std::runtime_error(error);
        require(sequence.track(1).route == 0x10000004 && sequence.track(1).externalControl, "Sequence sync did not retain its logical track route.");
        jaudio::Track interrupted;
        interrupted.position = 100;
        interrupted.wait = 8;
        interrupted.interrupts[3] = 300;
        interrupted.interrupts[4] = 400;
        interrupted.requestInterrupt(3);
        require(interrupted.pendingInterrupts == 0, "Disabled interrupt was queued.");
        interrupted.enabledInterrupts = (1u << 3) | (1u << 4);
        interrupted.requestInterrupt(4);
        interrupted.requestInterrupt(3);
        interrupted.enterInterrupt();
        require(interrupted.position == 300 && interrupted.wait == 0 && interrupted.interruptWait == 8 && interrupted.interruptReturn == 100,
            "Interrupt priority or suspended sequence position changed.");
        interrupted.enterInterrupt();
        require(interrupted.position == 300 && interrupted.pendingInterrupts == (1u << 4), "Blocked interrupt nested into a running handler.");

        sequence.start({0xa4, 0, 0xff, 0xad, 0, 0, 2, 0xaf, 0, 0, 1, 0x80, 100, 0xff});
        for (unsigned i = 0; i < 10 && sequence.ticks() == 0; ++i)
            if (!sequence.advance([](const auto&) {}, error)) throw std::runtime_error(error);
        require(sequence.track(0).registers[0] == 1 && sequence.track(0).registers[3] == 0,
            "Sequence byte arithmetic or compare condition changed.");
        sequence.start({0xc1, 0, 0, 0, 15, 0x80, 1, 0xd2, 0, 0x40, 0x80, 1, 0xff, 0, 0,
            0xdf, 3, 0, 0, 26, 0x80, 8, 0xff, 0, 0, 0, 0xa4, 0, 7, 0xe3});
        for (unsigned i = 0; i < 20 && sequence.ticks() < 2; ++i)
            if (!sequence.advance([](const auto&) {}, error)) throw std::runtime_error(error);
        require(sequence.track(1).registers[0] == 7 && sequence.track(1).wait == 7 && sequence.track(1).position == 22,
            "Child port interrupt did not resume the original wait and instruction position.");
        jaudio::Interpreter normalTempo, halfTempo;
        const std::vector<std::uint8_t> waiting{0x88, 0xff, 0xff, 0xff};
        normalTempo.start(waiting);
        halfTempo.start(waiting);
        halfTempo.setTempoScale(0.5f);
        halfTempo.writePort(0, 0, 0xffff);
        require(halfTempo.track(0).ports[0] == 0xffff && halfTempo.track(0).importedPorts[0],
            "Application root port write lost its unsigned value or import flag.");
        for (unsigned block = 0; block < 1000; ++block) {
            if (!normalTempo.advance([](const auto&) {}, error) || !halfTempo.advance([](const auto&) {}, error))
                throw std::runtime_error(error);
        }
        require(normalTempo.ticks() / 2 == halfTempo.ticks(), "External tempo scale failed to alter the sequence clock.");
        if (argc > 1) {
            gameassets::Package package;
            jaudio::Resources resources;
            jaudio::Playback playback;
            if (!package.open(argv[1], error)) throw std::runtime_error(error);
            if (!jaudio::read(package, resources, error)) throw std::runtime_error(error);
            if (!playback.loadMansion(package, resources, error)) throw std::runtime_error(error);
            auto mutedPlayback = playback;
            auto pitchedPlayback = playback;
            playback.start();
            mutedPlayback.start();
            pitchedPlayback.start();
            for (unsigned track = 0; track < 32; ++track) pitchedPlayback.setTrackPitch(track, 0.75f, 0);
            pitchedPlayback.updateSequenceControls();
            for (unsigned track = 0; track < 32; ++track) mutedPlayback.setTrackMuted(track, true);
            mutedPlayback.updateSequenceControls();
            std::array<std::int16_t, 160> samples{};
            std::array<std::int16_t, 160> silentSamples{};
            std::array<std::int16_t, 160> idleSamples{};
            std::array<std::int16_t, 160> pitchedSamples{};
            bool pitchChangedOutput = false;
            jaudio::DspMixer idleMixer;
            if (!idleMixer.configure(resources.effects, error)) throw std::runtime_error(error);
            std::uint64_t audibleEnergy = 0, mutedEnergy = 0;
            double frameClock = 0;
            for (unsigned block = 0; block < 8000; ++block) {
                if (block == 1000) playback.setTrackMuted(4, true);
                if (block == 2000) playback.setTrackMuted(4, false);
                frameClock += 30.0 * 80.0 / 32028.5;
                if (frameClock >= 1) { playback.updateSequenceControls(); frameClock -= 1; }
                if (!playback.render(samples, error)) throw std::runtime_error(error);
                if (!pitchedPlayback.render(pitchedSamples, error)) throw std::runtime_error(error);
                if (block < 1000 && samples != pitchedSamples) pitchChangedOutput = true;
                if (!mutedPlayback.render(silentSamples, error)) throw std::runtime_error(error);
                idleMixer.begin();
                idleMixer.end(idleSamples, playback.outputLevel());
                require(silentSamples == idleSamples, "Muted sequence differs from the original mixer's idle output.");
                for (auto sample : samples) audibleEnergy += static_cast<std::uint64_t>(std::abs(int(sample)));
                for (auto sample : silentSamples) {
                    mutedEnergy += static_cast<std::uint64_t>(std::abs(int(sample)));
                }
            }
            require(playback.notes() > 0, "Mansion sequence did not execute notes.");
            std::cout << "Sequence sample totals: " << audibleEnergy << ", muted " << mutedEnergy << ".\n";
            require(audibleEnergy > mutedEnergy * 100, "Mansion sequence produced no meaningful audio.");
            require(playback.notes() == mutedPlayback.notes(), "Default mute altered note execution or sequence timing.");
            require(pitchChangedOutput, "External track pitch did not reach rendered voices.");
            require(playback.notes() == pitchedPlayback.notes() && playback.ticks() == pitchedPlayback.ticks(), "Pitch changed sequence timing or note execution.");
            pitchedPlayback.start();
            require(pitchedPlayback.sequenceControls().track(4).pitch.value == 1, "Sequence restart retained external pitch.");
            std::cout << "Mansion sequence rendered " << playback.notes() << " notes.\n";
        }
        std::cout << "Sequence controls passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
