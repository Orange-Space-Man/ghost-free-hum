#include "game/startup/openingresources.h"
#include "game/models/s3danimation.h"
#include "game/models/s3dmodel.h"
#include "game/models/s3dmorph.h"
#include "game/models/s3dshadow.h"
#include <cmath>
#include "game/models/s3denvironment.h"
#include "game/startup/openingtimeline.h"
#include "jlib/jaudio/jaudiostream.h"

#include <iostream>
#include <fstream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        gameassets::Package package;
        openingscene::Resources resources;
        resources.cutscene = 123;
        std::string error;
        std::vector<std::uint8_t> encoded(50);
        encoded[3] = 18;
        encoded[7] = 16;
        encoded[8] = 0xac;
        encoded[9] = 0x44;
        encoded[11] = 4;
        encoded[13] = 16;
        encoded[15] = 30;
        encoded[32] = 0xf0;
        encoded[33] = 0x78;
        encoded[42] = 0x21;
        jaudio::StreamAudio decodedStream;
        std::array<std::int16_t, 32> coefficients{};
        if (!jaudio::decodeStream(encoded, coefficients, decodedStream, error) ||
            decodedStream.samples[0] != -32768 || decodedStream.samples[1] != 2 ||
            decodedStream.samples[2] != 0 || decodedStream.samples[3] != 1)
            throw std::runtime_error("Stream channel order or original signed wrapping.");
        encoded.pop_back();
        if (jaudio::decodeStream(encoded, coefficients, decodedStream, error) || decodedStream.samples.size() != 32)
            throw std::runtime_error("Truncated stream accepted or prior audio changed.");
        if (openingscene::read(package, 1, resources, error) || resources.cutscene != 123)
            throw std::runtime_error("Invalid opening ID was accepted or changed existing resources.");
        openingscene::Resources synthetic;
        openingscene::Segment segment;
        segment.id = 23;
        segment.scene.data = {0,2};
        segment.settings.data = {0x3f,0,0,0};
        segment.models.resize(1);
        synthetic.segments = {segment, segment};
        synthetic.segments[1].id = 24;
        synthetic.events = {{{0,0,0,0,0,0,0,23,0,0,0,9}}, {{0,0,0,0,0,0,0,24,0,0,0,8}}};
        openingscene::Timeline timeline;
        timeline.reset(synthetic);
        timeline.update(synthetic, false);
        if (timeline.commands.size() != 1 || timeline.commands[0].opcode != 9 || timeline.drawFrame != 0 || timeline.frame != 0.5f)
            throw std::runtime_error("Opening event dispatch or update/draw ordering.");
        for (unsigned i = 0; i < 3; ++i) { timeline.afterDraw(synthetic); timeline.update(synthetic, false); }
        if (timeline.segment != 0) throw std::runtime_error("Opening advanced before drawing last frame.");
        timeline.afterDraw(synthetic);
        timeline.update(synthetic, false);
        if (timeline.segment != 1 || !timeline.resetAnimations || timeline.commands.size() != 1 || timeline.commands[0].opcode != 8)
            throw std::runtime_error("Opening segment reset or event cursor.");
        for (unsigned i = 0; i < 3; ++i) { timeline.afterDraw(synthetic); timeline.update(synthetic, false); }
        if (!timeline.finished) throw std::runtime_error("Opening completion boundary.");
        synthetic.events.resize(1);
        synthetic.events[0][11] = 5;
        timeline.reset(synthetic);
        timeline.update(synthetic, false);
        if (timeline.frame != 0 || timeline.animationStep != 0 || timeline.event != 0)
            throw std::runtime_error("Opening wait must retry and pause animation.");
        timeline.update(synthetic, true);
        if (timeline.frame != 0.5f || timeline.event != 1 || timeline.animationStep != 0.5f)
            throw std::runtime_error("Opening wait did not release on flag 55.");
        synthetic.events[0][11] = 10;
        timeline.reset(synthetic);
        timeline.update(synthetic, false);
        if (!(timeline.flags & 8) || (timeline.flags & 16)) throw std::runtime_error("Opening hide command was applied too early.");
        timeline.update(synthetic, false);
        if (!(timeline.flags & 16)) throw std::runtime_error("Opening hide command was not latched next update.");
        synthetic.events[0][11] = 6;
        timeline.reset(synthetic);
        for (unsigned i = 0; i < 4; ++i) timeline.update(synthetic, false);
        if (timeline.frame != 0 || timeline.event != 0 || !timeline.resetAnimations || !timeline.clearFlag55)
            throw std::runtime_error("Opening loop did not rewind events and animations.");
        if (argc >= 2) {
            if (!package.open(argv[1], error)) throw std::runtime_error(error);
            for (const auto* path : {"files/AudioRes/Stream/TMOpen.afc", "files/AudioRes/Stream/TMansion.afc"}) {
                jaudio::StreamAudio stream;
                if (!jaudio::readStream(package, path, stream, error)) throw std::runtime_error(error);
                if (stream.sampleRate != 32000 || stream.playbackRate != 32028.5 ||
                    std::abs(stream.framesPerSample - 5644.0 * 30 / (4096 * 44100)) > 1e-12)
                    throw std::runtime_error("Opening stream DSP rate or source-frame clock changed.");
                std::cout << path << ": " << stream.samples.size() / 2 << " stereo frames, " << stream.playbackRate << " Hz, " << stream.frameRate << " fps\n";
                if (argc == 3) {
                    const auto output = std::filesystem::path(argv[2]) / (std::filesystem::path(path).stem().string() + ".pcm");
                    std::ofstream file(output, std::ios::binary);
                    file.write(reinterpret_cast<const char*>(stream.samples.data()), stream.samples.size() * sizeof(std::int16_t));
                    if (!file) throw std::runtime_error("Could not write stream comparison samples.");
                }
            }
            for (unsigned cutscene : {0x4cU,0U}) {
                if (!openingscene::read(package, cutscene, resources, error)) throw std::runtime_error(error);
                const std::vector<unsigned> expected = cutscene == 0x4c ? std::vector<unsigned>{23,27,24,25,26,28,29} : std::vector<unsigned>{30,31};
                if (resources.mode != 2 || resources.segments.size() != expected.size()) throw std::runtime_error("Original opening table changed.");
                unsigned models = 0, animations = 0, renderable = 0;
                for (unsigned i = 0; i < resources.segments.size(); ++i) {
                    const auto& segment = resources.segments[i];
                    if (segment.id != expected[i] || segment.scene.data.empty() || segment.settings.data.empty())
                        throw std::runtime_error("Invalid opening segment resources.");
                    s3denvironment::CameraAnimation camera;
                    if (!camera.read(segment.scene.data, segment.settings.data, error)) throw std::runtime_error(segment.archive + ": " + error);
                    const unsigned frames = (segment.scene.data[0] << 8) | segment.scene.data[1];
                    s3dshadow::Animation shadow;
                    if (!shadow.read(segment.scene.data, segment.settings.data, error)) throw std::runtime_error(error);
                    for (unsigned frame = 0; frame <= frames * 2; ++frame) {
                        const auto projection = shadow.sample(frame * 0.5f);
                        if (!std::isfinite(projection.fieldOfView)) throw std::runtime_error("Invalid shadow projection sample.");
                    }
                    for (unsigned frame = 0; frame <= frames; ++frame) camera.sample(float(frame));
                    s3denvironment::Environment environment;
                    if (!environment.read(segment.scene.data, segment.settings.data, error)) throw std::runtime_error(error);
                    for (unsigned frame = 0; frame <= frames; ++frame) environment.sampleFog(float(frame));
                    const auto fog = environment.sampleFog(0);
                    if (fog.type) std::cout << "Fog " << fog.type << " range " << fog.rangeEnabled << " start " << fog.start << " end " << fog.end << '\n';
                    for (const auto& model : segment.models) {
                        if (model.model.data.empty()) throw std::runtime_error("Missing opening model.");
                        ++models;
                        s3dmodel::Model decodedModel;
                        if (!decodedModel.read(model.model.data, error, (model.flags & 2) != 0)) throw std::runtime_error(model.model.name + ": " + error);
                        ++renderable;
                        if (model.animations[2].size() != model.animations[3].size()) throw std::runtime_error("Mismatched morph resources.");
                        for (unsigned morph = 0; morph < model.animations[2].size(); ++morph) {
                            s3dmorph::Animation decoded;
                            if (!decoded.read(model.animations[2][morph].data, model.animations[3][morph].data,
                                decodedModel.positions.size(), error)) throw std::runtime_error(error);
                            float time = 0;
                            for (unsigned frame = 0; frame <= frames * 2; ++frame) {
                                decoded.apply(time, decodedModel.positions);
                                for (const auto& position : decodedModel.positions) for (float value : position)
                                    if (!std::isfinite(value)) throw std::runtime_error("Nonfinite morphed position.");
                                time = decoded.advance(time, 0.5f);
                            }
                        }
                        for (const auto& effect : model.animations[1]) {
                            s3denvironment::ModelEffect decoded;
                            if (!decoded.read(effect.data, decodedModel, error)) throw std::runtime_error(effect.name + ": " + error);
                            float time = 0;
                            for (unsigned frame = 0; frame <= frames; ++frame) {
                                decoded.apply(time, decodedModel);
                                time = decoded.advance(time, 1);
                            }
                        }
                        for (const auto& animation : model.animations[0]) {
                            s3danimation::Animation decoded;
                            if (!decoded.read(animation.data, error)) throw std::runtime_error(animation.name + ": " + error);
                            ++animations;
                        }
                    }
                }
                std::cout << "Opening " << cutscene << ": " << resources.segments.size() << " segments, " << models << " models, "
                    << animations << " skeletal animations, " << resources.events.size() << " events, " << renderable << " models accepted by current renderer\n";
                timeline.reset(resources);
                unsigned updates = 0, commands = 0;
                while (!timeline.finished && timeline.requestedBranch < 0 && updates < 30000) {
                    timeline.update(resources, false);
                    commands += unsigned(timeline.commands.size());
                    timeline.afterDraw(resources);
                    ++updates;
                }
                if (!timeline.finished) throw std::runtime_error("Opening timeline did not finish.");
                std::cout << "Timeline: " << updates << " updates, " << commands << " model commands\n";
            }
        }
        std::cout << "Opening resource checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
