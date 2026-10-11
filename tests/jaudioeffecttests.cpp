#include "jlib/jaudio/jaudioresources.h"
#include "jlib/jaudio/jaudiosequence.h"
#include "jlib/jaudio/jaudiodspmixer.h"
#include "jlib/jaudio/jaudioplayback.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
    void check(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    void write16(std::vector<std::uint8_t>& data, std::size_t offset, unsigned value) {
        data.at(offset) = static_cast<std::uint8_t>(value >> 8);
        data.at(offset + 1) = static_cast<std::uint8_t>(value);
    }

    void write32(std::vector<std::uint8_t>& data, std::size_t offset, unsigned value) {
        write16(data, offset, value >> 16);
        write16(data, offset + 2, value);
    }

    std::vector<std::uint8_t> configuration() {
        std::vector<std::uint8_t> data(176);
        write32(data, 0, 2);
        write32(data, 4, 3);
        write32(data, 20, 48);
        write32(data, 24, 48);
        data[48] = 1;
        write16(data, 50, 1);
        write16(data, 52, 0x9234);
        write16(data, 54, 11);
        write32(data, 60, 6);
        write16(data, 64, 0x8000);
        write16(data, 78, 0x7fff);
        return data;
    }
}

int main(int argc, char** argv) {
    try {
        jaudio::DspBlock minimumSource{}, minimumBus{};
        minimumSource.fill(-32768);
        jaudio::mixVoice(minimumBus, minimumSource, 15480, 15480);
        check(minimumBus[0] == -7740 && jaudio::scaleOutput(minimumBus[0], 8192) == -15480,
            "Exact negative mixer products must not acquire an extra rounding step.");
        std::string error;
        jaudio::DspBlock source;
        source.fill(32767);
        jaudio::DspBlock mixed{};
        jaudio::mixVoice(mixed, source, 0, 16384);
        check(mixed[0] == 0 && mixed[1] == 0 && mixed[2] == 0 && mixed[3] == 255 && mixed[4] == 255,
            "DSP pipelined gain update boundary changed.");
        check(mixed[63] == 7935 && mixed[64] == 7935 && mixed[65] == 8191 && mixed[79] == 8191,
            "DSP gain did not reach its hold after 32 pair updates.");
        source.fill(-1);
        mixed.fill(0);
        jaudio::mixVoice(mixed, source, 32767, 32767);
        jaudio::mixVoice(mixed, source, 32767, 32767);
        check(mixed[0] == -2 && mixed[79] == -2, "Voice contributions must round separately toward negative infinity.");
        mixed.fill(-32768);
        jaudio::mixVoice(mixed, source, 32767, 32767);
        check(mixed[0] == -32768, "DSP voice accumulation did not saturate.");
        mixed.fill(0);
        jaudio::mixVoice(mixed, source, 1, 0);
        check(mixed[2] == -1 && mixed[3] == 0, "Fractional descending gain rounded incorrectly.");
        check(jaudio::scaleOutput(-1, 2048) == -1 && jaudio::scaleOutput(1, 2048) == 0 &&
            jaudio::scaleOutput(20000, 8192) == 32767 && jaudio::scaleOutput(-20000, 8192) == -32768,
            "DSP output scaling rounding or saturation changed.");

        jaudio::EffectConfiguration effectSetup;
        effectSetup.allocationBlocks[0] = 1;
        effectSetup.presets.resize(1);
        auto& firstLine = effectSetup.presets[0][0];
        firstLine.flags = 1;
        firstLine.delayBlocks = 1;
        firstLine.coefficients[6] = 16384;
        firstLine.destinations = {{{0, 16384}, {11, 0}}};
        jaudio::EffectProcessor processor;
        check(processor.configure(effectSetup, 0, error), error.c_str());
        jaudio::DspBuses buses{};
        processor.begin(buses);
        buses[2][0] = 16384;
        processor.end(buses);
        buses = {};
        processor.begin(buses);
        check(buses[0][0] == 0 && buses[0][1] == 0 && buses[0][2] == 4096 && buses[2][2] == 8192,
            "Reverb impulse delay, FIR alignment, or destination gain changed.");
        processor.end(buses);
        buses = {};
        processor.begin(buses);
        check(buses[0][4] == 2048 && buses[2][4] == 4096, "Reverb feedback was not stored after mixing.");
        processor.reset();
        buses = {};
        processor.begin(buses);
        check(buses == jaudio::DspBuses{}, "Reverb reset retained its tail.");
        buses[2][79] = 16384;
        processor.end(buses);
        buses = {};
        processor.begin(buses);
        check(buses[0] == jaudio::DspBlock{}, "FIR history appeared one block early.");
        processor.end(buses);
        buses = {};
        processor.begin(buses);
        check(buses[0][1] == 4096, "FIR history across an 80-sample boundary was lost.");

        firstLine.flags = 2;
        check(processor.configure(effectSetup, 0, error), error.c_str());
        buses = {};
        processor.begin(buses);
        buses[2][0] = 16384;
        processor.end(buses);
        buses = {};
        processor.begin(buses);
        check(buses[0][8] == 8192 && buses[0][2] == 0 && buses[2][2] == 8192,
            "Post-filtering incorrectly filtered the destination output.");

        firstLine.flags = 4;
        firstLine.destinations[0].gain = 16384;
        check(processor.configure(effectSetup, 0, error), error.c_str());
        buses = {};
        processor.begin(buses);
        buses[2][0] = 1; buses[2][1] = 3; buses[2][2] = -1; buses[2][3] = -3;
        processor.end(buses);
        buses = {};
        processor.begin(buses);
        check(buses[0][8] == 0 && buses[0][9] == 2 && buses[0][10] == 0 && buses[0][11] == -2,
            "Effect multiply must round halfway values to even.");
        firstLine.flags = 1;
        firstLine.coefficients.fill(32767);
        check(processor.configure(effectSetup, 0, error), error.c_str());
        buses = {};
        processor.begin(buses);
        buses[2].fill(32767);
        processor.end(buses);
        buses = {};
        processor.begin(buses);
        check(buses[2][8] == -16 && buses[0][8] == -8, "Effect filter incorrectly saturated in DSP 16-bit mode.");
        firstLine.coefficients.fill(0);
        firstLine.coefficients[6] = 16384;
        firstLine.delayBlocks = 2;
        check(processor.configure(effectSetup, 0, error), error.c_str());
        buses = {};
        processor.begin(buses);
        buses[2][0] = 16384;
        processor.end(buses);
        buses = {};
        processor.begin(buses);
        check(buses == jaudio::DspBuses{}, "Two-block delay returned early.");
        processor.end(buses);
        buses = {};
        buses[0].fill(32767);
        processor.begin(buses);
        check(buses[0][2] == -28673, "Effect destination addition must wrap in DSP 16-bit mode.");
        check(!processor.configure(effectSetup, 1, error), "Invalid effect preset accepted.");
        buses = {};
        processor.begin(buses);
        check(buses == jaudio::DspBuses{}, "Failed configuration retained active delay lines.");
        firstLine.delayBlocks = 1;
        jaudio::DspMixer mixer;
        check(mixer.configure(effectSetup, error), error.c_str());
        std::array<std::int16_t, 160> output{};
        mixer.begin();
        mixer.end(output, 4096);
        check(output[0] == 0 && output[1] == -1 && output[159] == -1, "Original inverted return's idle value changed.");
        source.fill(0);
        source[0] = 16384;
        mixer.begin();
        mixer.mix(3, source, 16384, 16384);
        mixer.end(output, 4096);
        check(output[0] == 0 && mixer.hasTail(), "Effect send bypassed its delay or lost its tail.");
        mixer.begin();
        mixer.end(output, 4096);
        check(output[4] == 1024, "Live bus mixer did not return the delayed effect.");
        mixer.reset();
        check(!mixer.hasTail(), "Mixer reset retained effect memory.");

        jaudio::Interpreter routeSequence;
        routeSequence.start({0xdd, 0, 0x0b, 0x10, 0xc1, 0, 0, 0, 11, 0x80, 5, 0x80, 5, 0xff});
        for (unsigned block = 0; routeSequence.ticks() == 0; ++block) {
            check(block < 100 && routeSequence.advance([](const auto&) {}, error), "Route sequence failed.");
        }
        check(routeSequence.track(0).buses[0] == 0xb10 && routeSequence.track(1).buses[0] == 0x150,
            "Child channels must initialize their own original bus assignments.");
        jaudio::EffectConfiguration effects;
        const auto data = configuration();
        check(jaudio::decodeEffectConfiguration(data, effects, error), error.c_str());
        check(effects.presets.size() == 2 && effects.allocationBlocks[0] == 3, "Effect directory count/allocation lost.");
        for (const auto& preset : effects.presets) {
            const auto& line = preset[0];
            check(line.flags == 1 && line.delayBlocks == 6, "Shared preset offsets or delay units decoded incorrectly.");
            check(line.destinations[0].bus == 1 && line.destinations[0].gain == 0x9234 && line.destinations[1].bus == 11,
                "Effect bus or unsigned gain changed.");
            check(line.coefficients[0] == -32768 && line.coefficients[7] == 32767, "Signed FIR coefficients changed.");
        }
        for (std::size_t size = 0; size < data.size(); ++size) {
            check(!jaudio::decodeEffectConfiguration({data.begin(), data.begin() + size}, effects, error) &&
                effects.presets.empty() && !error.empty(), "Truncated effect table retained usable state.");
        }
        for (unsigned fault = 0; fault < 6; ++fault) {
            auto invalid = data;
            if (fault == 0) write32(invalid, 0, 256);
            if (fault == 1) write32(invalid, 20, 24);
            if (fault == 2) write32(invalid, 24, 0xfffffff0);
            if (fault == 3) write32(invalid, 60, 7);
            if (fault == 4) write32(invalid, 60, 0);
            if (fault == 5) write16(invalid, 50, 12);
            check(!jaudio::decodeEffectConfiguration(invalid, effects, error) && effects.presets.empty(), "Malformed effect table accepted.");
        }
        if (argc > 1) {
            gameassets::Package package;
            jaudio::Resources resources;
            check(package.open(argv[1], error), error.c_str());
            check(jaudio::read(package, resources, error), error.c_str());
            check(resources.effects.presets.size() == 2, "Original effect presets were not loaded.");
            check(resources.effects.allocationBlocks == std::array<std::uint32_t, 4>{32, 32, 32, 80}, "Original effect allocation changed.");
            for (unsigned preset = 0; preset < 2; ++preset) {
                const auto& lines = resources.effects.presets[preset];
                check(lines[0].delayBlocks == 32 && lines[1].delayBlocks == 32 && lines[2].delayBlocks == 24 &&
                    lines[3].delayBlocks == (preset == 0 ? 40 : 64), "Original effect delays changed.");
                check(lines[0].destinations[0].gain == 0x2000 && lines[1].destinations[1].gain == 0x2000 &&
                    lines[2].destinations[0].bus == 10 && lines[2].destinations[1].bus == 7, "Original effect routes changed.");
            }
            for (const unsigned id : {0x80000021U, 0x80000028U}) {
                std::vector<std::uint8_t> sequence;
                check(jaudio::readSequence(resources, id, sequence, error), error.c_str());
                jaudio::Interpreter interpreter;
                interpreter.start(sequence);
                float maximumSend = 0;
                unsigned notes = 0;
                for (unsigned block = 0; block < 48000 && !interpreter.finished(); ++block) {
                    check(interpreter.advance([&](const auto& event) {
                        if (event.type == jaudio::EventType::noteOn) ++notes;
                    }, error), error.c_str());
                    for (unsigned track = 0; track < 65; ++track) {
                        const auto& state = interpreter.track(track);
                        if (state.active) maximumSend = std::max(maximumSend, state.parameters[2].value);
                    }
                }
                std::cout << "Sequence " << std::hex << id << std::dec << ": " << notes << " notes, maximum track effect parameter " << maximumSend << '\n';
            }
            resources.sequenceArchive = {0xa4, 32, 4, 0xa4, 33, 21, 0x98, 2, 64,
                0xdd, 0, 0, 0, 0xdd, 1, 0, 0, 64, 1, 100, 0x80, 8, 0x81, 0xff};
            auto& entry = resources.sequences.at(33);
            entry.redirect = 0xffff;
            entry.offset = 0;
            entry.size = static_cast<std::uint32_t>(resources.sequenceArchive.size());
            jaudio::Playback playback;
            check(playback.load(package, resources, error), error.c_str());
            playback.start();
            std::vector<std::int16_t> rendered;
            bool audible = false;
            for (unsigned block = 0; block < 20000 && !playback.finished(); ++block) {
                check(playback.render(output, error), error.c_str());
                for (unsigned frame = 0; frame < 80; ++frame) {
                    if (block < 32) check(output[frame * 2] == 0 && output[frame * 2 + 1] == -2,
                        "Wet-only sequence produced sound before its original delay.");
                    if (output[frame * 2] != 0) audible = true;
                }
                rendered.insert(rendered.end(), output.begin(), output.end());
            }
            check(audible && playback.finished() && playback.notes() == 1, "Wet-only sequence failed to render and drain its effect tail.");
            playback.start();
            for (std::size_t offset = 0; offset < rendered.size(); offset += output.size()) {
                check(playback.render(output, error), error.c_str());
                check(std::equal(output.begin(), output.end(), rendered.begin() + offset), "Effect restart retained old delay history.");
            }
            check(playback.finished(), "Restarted effect tail did not finish deterministically.");
            jaudio::DspMixer shared;
            check(shared.configure(resources.effects, error), error.c_str());
            playback.start();
            for (std::size_t offset = 0; offset < rendered.size(); offset += output.size()) {
                shared.begin();
                if (!playback.voicesFinished()) check(playback.mix(shared, error), error.c_str());
                shared.end(output, playback.outputLevel());
                check(std::equal(output.begin(), output.end(), rendered.begin() + offset),
                    "Shared bus playback differs from standalone DSP processing.");
            }
            check(playback.voicesFinished() && !shared.hasTail(), "Shared effect history did not drain.");
            playback.start();
            for (unsigned block = 0; block < 32; ++block) {
                shared.begin();
                check(playback.mix(shared, error), error.c_str());
                shared.end(output, playback.outputLevel());
            }
            playback.stop();
            check(shared.hasTail(), "Stopping a sequence erased shared effect history.");
            bool retainedTail = false;
            for (unsigned block = 0; block < 20000 && shared.hasTail(); ++block) {
                shared.begin();
                shared.end(output, playback.outputLevel());
                for (unsigned frame = 0; frame < 80; ++frame) retainedTail |= output[frame * 2] != 0;
            }
            check(retainedTail && !shared.hasTail(), "Stopped sequence lost its delayed shared return.");
            std::cout << "Wet-only playback: " << rendered.size() / 2 << " frames, deterministic restart.\n";
        }
        std::cout << "JAudio effect configuration checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
