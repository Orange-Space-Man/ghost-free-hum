#include "game/audio/jaudiopositionalplayback.h"
#include "game/startup/startupresources.h"
#include "jlib/jaudio/jaudioeffectparameter.h"
#include "jlib/jaudio/jaudiosequencerandom.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
    void check(bool value, const std::string& message) {
        if (!value) throw std::runtime_error(message);
    }
}

int main(int argc, char** argv) {
    try {
        for (unsigned tick = 0; tick < 32; ++tick) {
            jaudio::SequenceRandom random;
            const std::array<std::uint32_t, 4> permutations{0x48da1134, 0x3411da48, 0x114834da, 0xda344811};
            auto expected = permutations[tick & 3];
            if (tick & 8) ++expected;
            if (tick & 16) expected = 0u - expected;
            check(random.next(tick) == expected, "Audio random selection preserves seed arithmetic, byte order and tick sign bits.");
        }
        jaudio::SequenceRandom retainedRandom;
        check(retainedRandom.next(0) == 0x48da1134 && retainedRandom.next(0) == 0x73b7cdce,
            "Successive audio random requests retain and update the shared byte state.");
        jaudio::EffectParameter parameter(1);
        parameter.set(0, 3);
        parameter.advance();
        check(parameter.current == 1.0f - 1.0f / 3 && parameter.remaining == 2,
            "Effect interpolation advances by the retained step.");
        parameter.advance();
        parameter.advance();
        check(parameter.current == 0 && !parameter.remaining,
            "The final effect interpolation tick assigns the exact target.");
        parameter.set(1, 4);
        parameter.advance();
        parameter.set(0.5f, 0);
        check(parameter.remaining == 3 && parameter.current == 0.5f,
            "Original immediate parameter assignment preserves a running interpolation counter.");
        parameter.advance();
        check(parameter.current == 0.75f,
            "Original immediate assignment preserves the old interpolation step.");
        jaudio::Interpreter interpreter;
        interpreter.startEffect({0xc1, 0, 0, 0, 7, 0xc6, 0, 60, 16, 100, 100, 0, 42, 0xff}, 0);
        unsigned notes = 0;
        std::string error;
        for (unsigned block = 0; block < 1000 && !interpreter.finished(); ++block) {
            check(interpreter.advance([&](const jaudio::SequenceEvent& event) {
                if (event.type == jaudio::EventType::noteOn) {
                    ++notes;
                    check(event.duration > 0 && event.key == 60, "Child sound note did not receive its original duration.");
                }
            }, error), error);
        }
        check(notes == 1 && interpreter.finished(), "Returning from sound dispatch must allow its child sequence to finish.");
        {
            jaudio::Interpreter slide;
            slide.start({0xd9, 0xfe, 0xd4, 60, 0xd8, 0, 3, 0, 4, 0, 5, 0x40, 0, 0, 6,
                0xf0, 0, 60, 0x48, 100, 50, 24, 0x80, 2, 0xff});
            unsigned emitted = 0;
            for (unsigned block = 0; block < 1000 && !slide.finished(); ++block) {
                check(slide.advance([&](const jaudio::SequenceEvent& event) {
                    if (event.type != jaudio::EventType::noteOn) return;
                    ++emitted;
                    const auto& track = slide.track(event.track);
                    const float clock = (48.0f * 120) / ((60.0f * 32028.5f) / 80.0f);
                    check(event.key == 58 && event.targetKey == 56 && event.duration == static_cast<unsigned>(12.0f / clock),
                        "Pitch slides preserve original last-note transposition, repeated target transposition and gate-scaled duration.");
                    check(track.adsr && *track.adsr == std::array<std::uint16_t, 5>{3, 4, 5, 0x4000, 6} && track.envelopeRoute == 0,
                        "Simple ADSR decodes five words and retains its original volume oscillator route.");
                }, error), error);
            }
            check(emitted == 1 && slide.finished(), "The pitch-slide sequence retains its independent note wait and completion.");
        }
        if (argc > 1) {
            gameassets::Package package;
            check(package.open(argv[1], error), error);
            jaudio::Resources resources;
            check(jaudio::read(package, resources, error), error);
            {
                std::vector<std::uint8_t> sequence;
                check(jaudio::readSequence(resources, 0x80000800, sequence, error), error);
                const auto entry = sequence.size();
                sequence.insert(sequence.end(), {0xa4, 0, 3, 0xc4, 0, 0, 0x45, 0x67, 0xff});
                for (unsigned previous = 0; previous < 3; ++previous) {
                    for (unsigned value : {0u, 1u, 2u, 0xffffffffu}) {
                        jaudio::Interpreter selection;
                        selection.random = [value] { return value; };
                        selection.start(sequence, entry);
                        selection.writePort(0, 10, static_cast<std::uint16_t>(previous));
                        for (unsigned block = 0; block < 100 && !selection.finished(); ++block)
                            check(selection.advance([](const auto&) {}, error), error);
                        auto expected = value % 3;
                        if (expected == previous) expected = (expected + 1) % 3;
                        check(selection.finished() && selection.track(0).ports[10] == expected,
                            "The original hurt selector avoids the previous voice, wraps at its range and retains the result in its port.");
                    }
                }
            }
            std::vector<std::uint8_t> dol;
            check(package.read("sys/main.dol", dol, error), error);
            jaudio::PositionOwners owners;
            unsigned sourceQueries = 0;
            unsigned floorQueries = 0;
            jaudio::PositionalPlayback playback(owners, dol, package, resources,
                [&](const std::array<float, 3>&, bool queryFloor) {
                    ++sourceQueries;
                    if (queryFloor) ++floorQueries;
                    return 0x80u;
                });
            jaudio::DspMixer mixer;
            check(mixer.configure(resources.effects, error), error);
            jaudio::Playback level;
            check(level.loadPositionalEffect(package, resources, 0x181e, error), error);
            std::array<float, 3> position{-100, 0, -200};
            std::array<float, 12> listener{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
            auto admit = [&] {
                for (unsigned block = 0; block < 32; ++block) {
                    mixer.begin();
                    if (!playback.mix(mixer, error)) throw std::runtime_error(error);
                    std::array<std::int16_t, 160> samples{};
                    mixer.end(samples, level.outputLevel());
                    if (block % 8 == 0) playback.update(listener);
                }
            };
            check(playback.spatial({-100, 0, -200}, 0).pan < 0.5f &&
                playback.spatial({100, 0, -200}, 0).pan > 0.5f &&
                playback.spatial({0, 0, 0}, 0).volume == 1,
                "Original positional pan and near-distance volume.");
            for (unsigned id : {0x839u, 0x83du, 0x1809u, 0x180au, 0x180bu, 0x181eu, 0x280fu, 0x2810u, 0x282bu}) {
                std::cout << "Checking positional sound " << std::hex << id << std::dec << '\n';
                jaudio::PositionalSound* binding = nullptr;
                auto* handle = playback.play(id, position, &binding);
                check(handle == binding && owners.categories[id >> 12] == handle && handle->ownerPosition == &position,
                    "Real playback handle must retain owner identity and its binding slot.");
                bool audible = false;
                for (unsigned block = 0; block < 4000 && !playback.finished(); ++block) {
                    if (block % 8 == 0) playback.update(listener);
                    mixer.begin();
                    check(playback.mix(mixer, error), error);
                    std::array<std::int16_t, 160> samples{};
                    mixer.end(samples, level.outputLevel());
                    audible |= std::any_of(samples.begin(), samples.end(), [](auto sample) { return sample != 0; });
                }
                check(audible && playback.finished() && !binding && !owners.categories[id >> 12],
                    "Original positional sound must produce PCM, complete, unlink, and clear its real binding.");
            }
            jaudio::PositionalSound* first = nullptr;
            check(sourceQueries == 9, "Each admitted sound queries its source context.");
            check(floorQueries == 5, "The player categories request original floor context while door effects use room context alone.");
            {
                std::array<jaudio::PositionalSound*, 5> requests{};
                playback.play(0x839, position, &requests[0]);
                playback.play(0x1809, position, &requests[1]);
                playback.play(0x180a, position, &requests[2]);
                playback.play(0x180b, position, &requests[3]);
                playback.play(0x2810, position, &requests[4]);
                check(std::all_of(requests.begin(), requests.end(), [](auto* request) { return request != nullptr; }),
                    "Player and door categories keep separate actor admission limits at the same position.");
                admit();
                requests[0]->stop(0);
                check(!requests[0] && requests[1] && requests[2] && requests[3] && requests[4] && !owners.categories[0],
                    "Releasing a player effect clears only its category and preserves concurrent door handles.");
                playback.stop();
                check(std::all_of(requests.begin(), requests.end(), [](auto* request) { return request == nullptr; }),
                    "Shared sequence teardown releases both categories without stranding handles.");
            }
            {
                std::array<jaudio::PositionalSound*, 4> requests{};
                const std::array<unsigned, 4> ids{0x1809, 0x180a, 0x180b, 0x181e};
                for (unsigned index = 0; index < ids.size(); ++index) playback.play(ids[index], position, &requests[index]);
                check(!requests[0] && requests[1] && requests[2] && requests[3],
                    "Original category actor limit replaces the oldest equal-priority request and clears its binding.");
                playback.stop();
                check(std::all_of(requests.begin(), requests.end(), [](auto* request) { return request == nullptr; }),
                    "Teardown releases every admitted actor request.");
            }
            for (unsigned repetition = 0; repetition < 80; ++repetition) {
                auto* sound = playback.play(0x1809, position);
                check(sound != nullptr, "Repeated sound admission lost its recycled record.");
                admit();
                sound->stop(0);
                admit();
            }
            playback.stop();
            {
                auto services = playback.animationServices(position);
                actoranimation::SoundHandle* animation = nullptr;
                services.start(0x1809, animation);
                check(animation && animation->flags() == 0x1809,
                    "Animation handles expose sound-ID flags independently of positional metadata.");
                services.pitch(*animation, 0.75f);
                services.volume(*animation, 0.5f);
                services.pan(*animation, 0.25f);
                mixer.begin();
                check(playback.mix(mixer, error), error);
                services.start(0x180a, animation);
                check(animation && animation->flags() == 0x180a,
                    "Animation slot replacement binds the newly owned playback handle.");
                admit();
                animation->stop(1);
                playback.update(listener);
                check(animation != nullptr, "Animation binding survives its pending fade.");
                playback.update(listener);
                check(!animation, "Animation binding clears on fade completion before handle reuse.");
                services.start(0x180b, animation);
                playback.stop();
                check(!animation, "Playback teardown clears real animation bindings.");
            }
            {
                jaudio::PositionalSound* fading = nullptr;
                playback.play(0x181e, position, &fading);
                fading->stop(1);
                check(!fading && !owners.categories[1], "An unadmitted request cancels immediately even with a fade duration.");
                playback.play(0x181e, position, &fading);
                admit();
                fading->stop(1);
                check(fading && owners.categories[1] == fading,
                    "Timed effect stop retains its live handle while the fade is pending.");
                playback.update(listener);
                check(fading != nullptr, "Effect release checks the fade before advancing its parameter clock.");
                playback.update(listener);
                check(!fading && !owners.categories[1],
                    "Completed effect fade releases the owner and clears the original binding.");
                playback.stop();
            }
            jaudio::PositionalSound* second = nullptr;
            std::array<float, 3> otherPosition{100, 0, -200};
            playback.play(0x181e, position, &first);
            playback.play(0x181e, otherPosition, &second);
            owners.release(&position);
            check(!first && second && !playback.finished(), "Owner release must stop only matching real handles.");
            playback.stop();
            check(!second && playback.finished() && !owners.categories[1], "Scene teardown must clear playback bindings and category links.");
            {
                jaudio::PositionalSound* door = nullptr;
                auto* original = playback.play(0x181e, position, &door);
                admit();
                playback.playDeathEffects(position);
                check(door == original && owners.categories[2], "Death keeps existing category handles while starting its vocal.");
                check(!playback.play(0x1809, position, &door) && !door && owners.categories[1] == original,
                    "A blocked category clears the request binding without stopping its existing fading sound.");
                jaudio::PositionalSound* recovery = nullptr;
                check(playback.play(0x83d, position, &recovery) != nullptr,
                    "Death suppression does not block player-category requests.");
                for (unsigned block = 0; block < 4000 && !playback.finished(); ++block) {
                    if (block % 8 == 0) playback.update(listener);
                    mixer.begin();
                    check(playback.mix(mixer, error), error);
                    std::array<std::int16_t, 160> samples{};
                    mixer.end(samples, level.outputLevel());
                }
                check(playback.finished() && !recovery && !owners.categories[1] && !owners.categories[2],
                    "Death fades and voice completion preserve normal handle release.");
                playback.stop();
                check(!playback.play(0x181e, position), "Stopping playback does not reset the original category admission flags.");
            }
            auto persistentResources = resources;
            persistentResources.sounds[1][0x1e][2] |= 0x80;
            jaudio::PositionOwners retainedOwners;
            std::array<jaudio::RetainedPosition, 2> retained;
            retained[0].next = &retained[1];
            retained[1].previous = &retained[0];
            retainedOwners.free = &retained[0];
            jaudio::PositionalPlayback persistent(retainedOwners, dol, package, persistentResources);
            jaudio::PositionalSound* retainedBinding = nullptr;
            auto* retainedHandle = persistent.play(0x181e, position, &retainedBinding);
            retainedOwners.release(&position);
            check(!retainedBinding && retainedOwners.active == &retained[0] && retained[0].sound == retainedHandle &&
                retainedHandle->position == &retained[0].position && !retainedHandle->ownerPosition,
                "Persistent metadata transfers a real playback handle to retained position ownership.");
            const auto savedPosition = retained[0].position;
            position[0] += 1000;
            check(*retainedHandle->position == savedPosition, "Retained playback must not follow a released actor position.");
            persistent.play(0x181e, position);
            check(retained[0].sound == retainedHandle,
                "Reusing the released actor address must not replace its retained sound.");
            for (unsigned block = 0; block < 4000 && !persistent.finished(); ++block) {
                if (block % 8 == 0) persistent.update(listener);
                mixer.begin();
                check(persistent.mix(mixer, error), error);
                std::array<std::int16_t, 160> samples{};
                mixer.end(samples, level.outputLevel());
            }
            check(persistent.finished() && !retained[0].sound, "Natural playback completion clears the retained record's binding.");
            persistent.update(listener);
            check(!retainedOwners.active && retainedOwners.free == &retained[0], "Completed real sounds return retained position records to the original free list.");
        }
        std::cout << "Positional sound playback checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
