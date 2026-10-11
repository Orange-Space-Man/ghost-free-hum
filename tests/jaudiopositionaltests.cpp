#include "game/audio/jaudiopositionalplayback.h"
#include "game/startup/startupresources.h"
#include "jlib/jaudio/jaudioeffectparameter.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
}

int main(int argc, char** argv) {
    try {
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
            }, error), error.c_str());
        }
        check(notes == 1 && interpreter.finished(), "Returning from sound dispatch must allow its child sequence to finish.");
        if (argc > 1) {
            gameassets::Package package;
            check(package.open(argv[1], error), error.c_str());
            jaudio::Resources resources;
            check(jaudio::read(package, resources, error), error.c_str());
            std::vector<std::uint8_t> dol;
            check(package.read("sys/main.dol", dol, error), error.c_str());
            jaudio::PositionOwners owners;
            jaudio::PositionalPlayback playback(owners, dol, package, resources);
            jaudio::DspMixer mixer;
            check(mixer.configure(resources.effects, error), error.c_str());
            jaudio::Playback level;
            check(level.loadPositionalEffect(package, resources, 0x181e, error), error.c_str());
            std::array<float, 3> position{-100, 0, -200};
            std::array<float, 12> listener{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
            check(playback.spatial({-100, 0, -200}, 0).pan < 0.5f &&
                playback.spatial({100, 0, -200}, 0).pan > 0.5f &&
                playback.spatial({0, 0, 0}, 0).volume == 1,
                "Original positional pan and near-distance volume.");
            for (unsigned id : {0x1809u, 0x180au, 0x180bu, 0x181eu}) {
                jaudio::PositionalSound* binding = nullptr;
                auto* handle = playback.play(id, position, &binding);
                check(handle == binding && owners.categories[1] == handle && handle->ownerPosition == &position,
                    "Real playback handle must retain owner identity and its binding slot.");
                bool audible = false;
                for (unsigned block = 0; block < 4000 && !playback.finished(); ++block) {
                    if (block % 8 == 0) playback.update(listener);
                    mixer.begin();
                    check(playback.mix(mixer, error), error.c_str());
                    std::array<std::int16_t, 160> samples{};
                    mixer.end(samples, level.outputLevel());
                    audible |= std::any_of(samples.begin(), samples.end(), [](auto sample) { return sample != 0; });
                }
                check(audible && playback.finished() && !binding && !owners.categories[1],
                    "Original door sound must produce PCM, complete, unlink, and clear its real binding.");
            }
            jaudio::PositionalSound* first = nullptr;
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
                check(playback.mix(mixer, error), error.c_str());
                services.start(0x180a, animation);
                check(animation && animation->flags() == 0x180a,
                    "Animation slot replacement binds the newly owned playback handle.");
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
            for (unsigned block = 0; block < 4000 && !persistent.finished(); ++block) {
                mixer.begin();
                check(persistent.mix(mixer, error), error.c_str());
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
