#include "jlib/jaudio/jaudioplayback.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
    void check(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    void tick(jaudio::Interpreter& sequence, const jaudio::Interpreter::Listener& listener) {
        std::string error;
        jaudio::Wave roundingWave;
        std::array<std::int16_t, 256> roundingCoefficients{};
        roundingCoefficients[0] = 16384;
        for (const int sample : {-5, -3, -1, 1, 3, 5}) {
            roundingWave.samples = {static_cast<std::int16_t>(sample)};
            const int expected = sample == -3 ? -2 : sample == 3 ? 2 : sample / 2;
            check(jaudio::resampleWave(roundingWave, 4 * 4096, 4096, roundingCoefficients) == expected,
                "DSP resampling must round half-way products to even.");
        }
        const auto before = sequence.ticks();
        for (unsigned block = 0; sequence.ticks() == before && block < 100; ++block) {
            check(sequence.advance(listener, error), error.c_str());
        }
        check(sequence.ticks() == before + 1, "Sequence clock did not tick.");
    }

    void writeWave(const char* path, const std::vector<std::int16_t>& pcm) {
        std::ofstream output(path, std::ios::binary);
        const auto number = [&output](std::uint32_t value, unsigned bytes) {
            for (unsigned i = 0; i < bytes; ++i) output.put(static_cast<char>(value >> (i * 8)));
        };
        output.write("RIFF", 4); number(static_cast<unsigned>(36 + pcm.size() * 2), 4);
        output.write("WAVEfmt ", 8); number(16, 4); number(1, 2); number(2, 2);
        number(32029, 4); number(128116, 4); number(4, 2); number(16, 2);
        output.write("data", 4); number(static_cast<unsigned>(pcm.size() * 2), 4);
        for (const auto sample : pcm) number(static_cast<std::uint16_t>(sample), 2);
        check(output.good(), "Could not write diagnostic WAV.");
    }
}

int main(int argc, char** argv) {
    try {
        std::string error;
        std::array<std::int16_t, 32> coefficients{};
        coefficients[2] = 2048;
        std::vector<std::uint8_t> encoded(9, 0x17);
        encoded[0] = 0x01;
        std::vector<std::int16_t> decoded;
        check(jaudio::decodeAfc(encoded, 16, coefficients, 100, 0, decoded, error), "AFC decode failed.");
        check(decoded[0] == 101 && decoded[1] == 108 && decoded[15] == 164, "AFC predictor history is wrong.");
        encoded[1] = 0x8f;
        check(jaudio::decodeAfc(encoded, 2, coefficients, 0, 0, decoded, error) && decoded[0] == -8 && decoded[1] == -9,
            "AFC signed residual is wrong.");
        check(!jaudio::decodeAfc({0}, 16, coefficients, 0, 0, decoded, error) && decoded.empty(), "Truncated AFC accepted.");
        encoded.assign(9, 0x77); encoded[0] = 0xf0;
        check(jaudio::decodeAfc(encoded, 16, coefficients, 0, 0, decoded, error) && decoded[0] == 32767, "AFC clipping failed.");

        jaudio::Oscillator oscillator;
        oscillator.attack = {{0, 0, 16384}, {14, 0, 0}};
        jaudio::Envelope envelope;
        check(envelope.advance(oscillator) == 0.5f && envelope.advance(oscillator) == 0.5f, "Envelope hold failed.");
        envelope.release(oscillator, 10);
        for (unsigned i = 0; i < 10; ++i) envelope.advance(oscillator);
        check(envelope.ended, "Envelope release did not finish.");

        jaudio::Interpreter sequence;
        std::vector<jaudio::SequenceEvent> events;
        const auto collect = [&events](const auto& event) { events.push_back(event); };
        sequence.start({60, 1, 100, 0x80, 2, 0x81, 0xff});
        tick(sequence, collect);
        check(events.size() == 1 && events[0].type == jaudio::EventType::noteOn, "First note timing failed.");
        tick(sequence, collect);
        check(events.size() == 1, "Wait ended one tick early.");
        tick(sequence, collect);
        check(events.size() == 3 && events[1].type == jaudio::EventType::noteOff && sequence.finished(), "Wait/note-off timing failed.");

        events.clear();
        sequence.start({0xc1, 0, 0, 0, 9, 0x80, 2, 0xff, 0, 64, 2, 80, 0x80, 10, 0xff});
        tick(sequence, collect);
        check(events.size() == 1 && events[0].track == 1 && events[0].key == 64, "Child did not run in its opening tick.");
        tick(sequence, collect); tick(sequence, collect);
        check(sequence.finished() && !sequence.track(1).active, "Parent finish did not stop its child.");

        events.clear();
        sequence.start({0xc4, 0, 0, 0, 6, 0xff, 60, 8, 80, 100, 8, 0xc6, 0}, 0, 120, 48);
        tick(sequence, collect);
        check(events.size() == 1 && events[0].slot == 0 && events[0].duration == 33, "Timed note gate conversion failed.");
        for (unsigned i = 0; i < 8; ++i) tick(sequence, collect);
        check(sequence.finished(), "Timed note call did not return after its wait.");

        sequence.start({0x9e, 1, 0x40, 0, 4, 0x80, 1, 0x9c, 1, 0, 0, 0x80, 1, 0xff});
        tick(sequence, collect);
        check(sequence.track(0).parameters[1].value == 0.125f, "Parameter interpolation failed.");
        tick(sequence, collect);
        check(sequence.track(0).parameters[1].remaining == 2 && sequence.track(0).parameters[1].value > 0.08f,
            "Parameter command did not reuse its remaining duration.");
        for (const auto& malformed : std::vector<std::vector<std::uint8_t>>{{0xc1}, {0xfb}, {0xc1, 0, 0, 1, 0}, {60, 0, 100}}) {
            sequence.start(malformed);
            bool accepted = true;
            for (unsigned i = 0; i < 10 && accepted; ++i) accepted = sequence.advance(collect, error);
            check(!accepted && !error.empty(), "Invalid sequence accepted.");
        }

        if (argc > 1) {
            gameassets::Package package;
            jaudio::Resources resources;
            jaudio::Playback playback;
            check(package.open(argv[1], error), error.c_str());
            check(jaudio::read(package, resources, error), error.c_str());
            jaudio::VoiceBank bank;
            check(bank.load(package, resources, error), error.c_str());
            check(bank.channelLevel == 14745 && bank.outputLevel == 8192, "Game mixer initialization was replaced by JAudio defaults.");
            check(bank.pitch(-1.0e-10f, 1) == bank.pitch(0, 1), "Pitch slide rounding dropped a semitone at zero.");
            check(bank.pitch(-0.25f, 1) < bank.pitch(0, 1), "Negative semitone bend was lost.");
            check(playback.load(package, resources, error), error.c_str());
            playback.start();
            std::vector<std::int16_t> pcm;
            std::array<std::int16_t, 160> block{};
            for (unsigned i = 0; i < 24000 && !playback.finished(); ++i) {
                check(playback.render(block, error), error.c_str());
                pcm.insert(pcm.end(), block.begin(), block.end());
            }
            check(playback.finished(), "Title did not finish within one minute.");
            const auto peak = std::max_element(pcm.begin(), pcm.end());
            check(peak != pcm.end() && *peak > 1000, "Title output is silent.");
            check(*peak < 32767 && *std::min_element(pcm.begin(), pcm.end()) > -32768, "Title output is clipping.");
            std::cout << "Title: " << playback.notes() << " notes, " << playback.ticks() << " ticks, "
                << playback.peakVoices() << " peak voices, " << pcm.size() / 2 << " frames, peak " << *peak << '\n';
            check(playback.notes() == 97 && playback.ticks() == 3148, "Title sequence traversal changed.");
            playback.start();
            for (unsigned i = 0; i < 200; ++i) {
                check(playback.render(block, error), error.c_str());
                check(std::equal(block.begin(), block.end(), pcm.begin() + i * 160), "Title restart changed the output.");
            }
            playback.stop();
            check(playback.finished(), "Title did not stop.");
            if (argc > 2) writeWave(argv[2], pcm);
            check(playback.loadStartSound(package, resources, error), error.c_str());
            playback.start();
            pcm.clear();
            for (unsigned i = 0; i < 4000 && !playback.finished(); ++i) {
                check(playback.render(block, error), error.c_str());
                pcm.insert(pcm.end(), block.begin(), block.end());
            }
            check(playback.finished() && playback.notes() == 3, "Original Start sound did not execute its three notes.");
            check(pcm.size() > 32000 && pcm.size() < 128000, "Unexpected Start sound duration.");
            check(*std::max_element(pcm.begin(), pcm.end()) > 1000, "Start sound is silent.");
            playback.start();
            for (unsigned i = 0; i < pcm.size() / 160; ++i) {
                check(playback.render(block, error), error.c_str());
                check(std::equal(block.begin(), block.end(), pcm.begin() + i * 160), "Start sound restart changed the output.");
            }
            std::cout << "Start: " << playback.notes() << " notes, " << pcm.size() / 2 << " frames\n";
            if (argc > 3) writeWave(argv[3], pcm);
            check(playback.loadMenu(package, resources, error), error.c_str());
            playback.start();
            unsigned menuPeak = 0;
            for (unsigned i = 0; i < 48000; ++i) {
                if (!playback.render(block, error)) throw std::runtime_error(error);
                for (const auto sample : block) menuPeak = std::max(menuPeak, static_cast<unsigned>(std::abs(int(sample))));
            }
            check(menuPeak > 1000 && playback.notes() > 0, "Menu music is silent.");
            std::cout << "Menu: " << playback.notes() << " notes, peak " << menuPeak << '\n';
            for (const auto id : {0x3801U,0x3803U}) {
                if (!playback.loadMenuEffect(package, resources, id, error)) throw std::runtime_error(error);
                playback.start();
                unsigned peak = 0;
                for (unsigned i = 0; i < 4000 && !playback.finished(); ++i) {
                    if (!playback.render(block, error)) throw std::runtime_error(error);
                    for (const auto sample : block) peak = std::max(peak, static_cast<unsigned>(std::abs(int(sample))));
                }
                check(playback.finished() && peak > 0, "Menu effect did not finish audibly.");
            }
        }
        std::cout << "JAudio checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
