#include "platform/windows/startupaudio.h"
#include "jlib/jaudio/jaudioplayback.h"
#include "game/audio/jaudiopositionalplayback.h"

#include <windows.h>
#include <xaudio2.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
    Microsoft::WRL::ComPtr<IXAudio2> engine;
    IXAudio2MasteringVoice* master = nullptr;
    IXAudio2SourceVoice* source = nullptr;
    IXAudio2SourceVoice* music = nullptr;
    jaudio::Playback playback;
    jaudio::Playback titlePlayback;
    jaudio::Playback menuPlayback;
    jaudio::Playback startPlayback;
    jaudio::Playback movePlayback;
    jaudio::Playback cancelPlayback;
    jaudio::Playback* activeMusic = nullptr;
    jaudio::Playback* activeEffect = nullptr;
    jaudio::DspMixer sharedMixer;
    std::shared_ptr<jaudio::PositionalPlayback> positionalPlayback;
    std::mutex renderMutex;
    std::uint64_t submittedFrames = 0;
    std::uint64_t musicEnd = 0;
    std::uint64_t effectEnd = 0;
    std::atomic<bool> effectComplete{true};
    std::array<std::array<std::int16_t, 1120>, 3> musicBuffers{};
    unsigned nextBuffer = 0;
    bool prepared = false;
    std::vector<std::int16_t> logoBuffer;
    bool comInitialized = false;
    std::thread musicThread;
    std::atomic<bool> musicRunning{false};
    std::atomic<bool> musicComplete{true};
    std::mutex errorMutex;
    std::exception_ptr musicError;
    jaudio::StreamAudio streamAudio;

    void check(HRESULT result, const char* message) {
        if (FAILED(result)) throw std::runtime_error(message);
    }

    void createVoice(IXAudio2SourceVoice** voice) {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 2;
        format.nSamplesPerSec = 32000;
        format.wBitsPerSample = 16;
        format.nBlockAlign = 4;
        format.nAvgBytesPerSec = 128000;
        check(engine->CreateSourceVoice(voice, &format), "Could not create an audio voice.");
        check((*voice)->SetFrequencyRatio(32028.5f / 32000), "Could not set the original audio rate.");
    }

    void stopMusic() {
        musicRunning = false;
        if (musicThread.joinable()) musicThread.join();
        if (music) { music->DestroyVoice(); music = nullptr; }
        streamAudio = {};
        if (activeMusic) activeMusic->stop();
        if (activeEffect) activeEffect->stop();
        if (positionalPlayback) positionalPlayback->stop();
        activeMusic = nullptr;
        activeEffect = nullptr;
        sharedMixer.reset();
        submittedFrames = 0;
        musicEnd = 0;
        effectEnd = 0;
        musicComplete = true;
        effectComplete = true;
        std::lock_guard<std::mutex> lock(errorMutex);
        musicError = nullptr;
    }

    void queueMusic() {
        std::lock_guard<std::mutex> lock(renderMutex);
        XAUDIO2_VOICE_STATE state{};
        music->GetState(&state);
        musicComplete = (activeMusic == nullptr || activeMusic->voicesFinished()) && state.SamplesPlayed >= musicEnd;
        effectComplete = (activeEffect == nullptr || activeEffect->voicesFinished()) && state.SamplesPlayed >= effectEnd;
        while (state.BuffersQueued < musicBuffers.size()) {
            auto& buffer = musicBuffers[nextBuffer];
            std::string error;
            for (unsigned offset = 0; offset < buffer.size(); offset += 160) {
                std::array<std::int16_t, 160> samples{};
                const bool musicActive = !(activeMusic == nullptr || activeMusic->voicesFinished());
                const bool effectActive = !(activeEffect == nullptr || activeEffect->voicesFinished());
                sharedMixer.begin();
                if (musicActive && !activeMusic->mix(sharedMixer, error)) throw std::runtime_error(error);
                if (effectActive && !activeEffect->mix(sharedMixer, error)) throw std::runtime_error(error);
                if (positionalPlayback && !positionalPlayback->mix(sharedMixer, error)) throw std::runtime_error(error);
                sharedMixer.end(samples, titlePlayback.outputLevel());
                submittedFrames += 80;
                if (musicActive) musicEnd = submittedFrames;
                if (effectActive) effectEnd = submittedFrames;
                std::copy(samples.begin(), samples.end(), buffer.begin() + offset);
            }
            XAUDIO2_BUFFER audio{};
            audio.AudioBytes = static_cast<UINT32>(buffer.size() * sizeof(std::int16_t));
            audio.pAudioData = reinterpret_cast<const BYTE*>(buffer.data());
            check(music->SubmitSourceBuffer(&audio), "Could not queue game audio.");
            nextBuffer = (nextBuffer + 1) % static_cast<unsigned>(musicBuffers.size());
            ++state.BuffersQueued;
        }
    }

    void startMixer() {
        if (music) return;
        nextBuffer = 0;
        createVoice(&music);
        queueMusic();
        check(music->Start(), "Could not start game audio.");
        musicRunning = true;
        musicThread = std::thread([] {
            try {
                while (musicRunning) {
                    queueMusic();
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            } catch (...) {
                std::lock_guard<std::mutex> lock(errorMutex);
                musicError = std::current_exception();
            }
        });
    }

}

void startupaudio::bindPositionalPlayback(std::shared_ptr<jaudio::PositionalPlayback> value) {
    if (value && (!prepared || !engine || streamAudio.sampleRate)) {
        throw std::runtime_error("Positional playback requires the initialized gameplay audio mixer.");
    }
    {
        std::lock_guard<std::mutex> lock(renderMutex);
        if (positionalPlayback && positionalPlayback != value) positionalPlayback->stop();
        positionalPlayback = std::move(value);
    }
    if (positionalPlayback) startMixer();
}

void startupaudio::prepare(const std::filesystem::path& path, const jaudio::Resources& resources) {
    stopMusic();
    prepared = false;
    gameassets::Package package;
    std::string error;
    if (!package.open(path, error) || !playback.load(package, resources, error) ||
        !startPlayback.loadStartSound(package, resources, error)) throw std::runtime_error(error);
    playback.start();
    std::array<std::int16_t, 160> samples{};
    for (unsigned block = 0; block < 24000 && !playback.finished(); ++block) {
        if (!playback.render(samples, error)) throw std::runtime_error(error);
    }
    if (!playback.finished()) throw std::runtime_error("Title sequence exceeded its preparation limit.");
    playback.stop();
    titlePlayback = playback;
    if (!menuPlayback.loadMenu(package, resources, error)) throw std::runtime_error(error);
    menuPlayback.start();
    for (unsigned block = 0; block < 24000; ++block) {
        if (!menuPlayback.render(samples, error)) throw std::runtime_error(error);
    }
    menuPlayback.stop();
    startPlayback.start();
    for (unsigned block = 0; block < 4000 && !startPlayback.finished(); ++block) {
        if (!startPlayback.render(samples, error)) throw std::runtime_error(error);
    }
    if (!startPlayback.finished()) throw std::runtime_error("Start sound exceeded its preparation limit.");
    startPlayback.stop();
    if (!movePlayback.loadMenuEffect(package, resources, 0x3801, error) ||
        !cancelPlayback.loadMenuEffect(package, resources, 0x3803, error)) throw std::runtime_error(error);
    const std::array<jaudio::Playback*, 3> effects{&movePlayback, &startPlayback, &cancelPlayback};
    for (unsigned i = 0; i < effects.size(); ++i) {
        auto& sound = *effects[i];
        sound.start();
        for (unsigned block = 0; block < 4000 && !sound.finished(); ++block) {
            if (!sound.render(samples, error)) throw std::runtime_error(error);
        }
        if (!sound.finished()) throw std::runtime_error("Menu effect exceeded its preparation limit.");
        sound.stop();
    }
    if (!sharedMixer.configure(resources.effects, error)) throw std::runtime_error(error);
    prepared = true;
}

void startupaudio::playStartSound() {
    playMenuEffect(0x3802);
}

void startupaudio::playMenuEffect(unsigned id) {
    if (!prepared || !engine) throw std::runtime_error("Start audio is not initialized.");
    if (id < 0x3801 || id > 0x3803) throw std::runtime_error("Unsupported menu effect.");
    if (streamAudio.sampleRate) throw std::runtime_error("Menu effects are unavailable during opening audio.");
    {
        std::lock_guard<std::mutex> lock(renderMutex);
        const std::array<jaudio::Playback*, 3> effects{&movePlayback, &startPlayback, &cancelPlayback};
        if (activeEffect) activeEffect->stop();
        activeEffect = effects[id - 0x3801];
        activeEffect->start();
        effectComplete = false;
    }
    startMixer();
}

bool startupaudio::startSoundFinished() {
    return effectComplete;
}

void startupaudio::playLogo(const std::vector<std::int16_t>& samples) {
    close();
    try {
        if (samples.size() != 57 * 560 * 2) throw std::runtime_error("Unexpected boot sample count.");
        const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (SUCCEEDED(initialized)) comInitialized = true;
        else if (initialized != RPC_E_CHANGED_MODE) check(initialized, "Could not initialize audio services.");
        check(XAudio2Create(&engine), "Could not initialize XAudio2.");
        check(engine->CreateMasteringVoice(&master), "Could not open the audio output device.");
        createVoice(&source);
        logoBuffer = samples;
        XAUDIO2_BUFFER audio{};
        audio.Flags = XAUDIO2_END_OF_STREAM;
        audio.AudioBytes = static_cast<UINT32>(logoBuffer.size() * sizeof(std::int16_t));
        audio.pAudioData = reinterpret_cast<const BYTE*>(logoBuffer.data());
        check(source->SubmitSourceBuffer(&audio), "Could not queue the boot sound.");
        check(source->Start(), "Could not start the boot sound.");
    } catch (...) {
        close();
        throw;
    }
}

void startupaudio::playTitle() {
    if (!prepared || !engine) throw std::runtime_error("Title audio is not initialized.");
    if (streamAudio.sampleRate) stopMusic();
    {
        std::lock_guard<std::mutex> lock(renderMutex);
        if (activeMusic) activeMusic->stop();
        activeMusic = &titlePlayback;
        activeMusic->start();
        musicComplete = false;
    }
    startMixer();
}

void startupaudio::playMenu() {
    if (!prepared || !engine) throw std::runtime_error("Menu audio is not initialized.");
    if (streamAudio.sampleRate) stopMusic();
    {
        std::lock_guard<std::mutex> lock(renderMutex);
        if (activeMusic) activeMusic->stop();
        activeMusic = &menuPlayback;
        activeMusic->start();
        musicComplete = false;
    }
    startMixer();
}

void startupaudio::playStream(const jaudio::StreamAudio& audio) {
    if (!prepared || !engine || audio.samples.empty() || !audio.sampleRate) throw std::runtime_error("Opening audio is not ready.");
    stopMusic();
    streamAudio = audio;
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = audio.sampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 4;
    format.nAvgBytesPerSec = audio.sampleRate * 4;
    check(engine->CreateSourceVoice(&music, &format), "Could not create opening audio voice.");
    if (audio.playbackRate) check(music->SetFrequencyRatio(static_cast<float>(audio.playbackRate / audio.sampleRate)),
        "Could not set opening DSP rate.");
    XAUDIO2_BUFFER buffer{};
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes = static_cast<UINT32>(streamAudio.samples.size() * sizeof(std::int16_t));
    buffer.pAudioData = reinterpret_cast<const BYTE*>(streamAudio.samples.data());
    check(music->SubmitSourceBuffer(&buffer), "Could not queue opening audio.");
    check(music->Start(), "Could not start opening audio.");
    musicComplete = false;
}

unsigned startupaudio::streamFrame() {
    return static_cast<unsigned>(streamPosition());
}

double startupaudio::streamPosition() {
    if (!music || !streamAudio.sampleRate) return 0;
    XAUDIO2_VOICE_STATE state{};
    music->GetState(&state);
    return double(state.SamplesPlayed) * (streamAudio.framesPerSample ? streamAudio.framesPerSample :
        double(streamAudio.frameRate) / streamAudio.sampleRate);
}

void startupaudio::update() {
    std::lock_guard<std::mutex> lock(errorMutex);
    if (musicError) std::rethrow_exception(musicError);
    if (music && streamAudio.sampleRate) {
        XAUDIO2_VOICE_STATE state{};
        music->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        musicComplete = state.BuffersQueued == 0;
    }
}

bool startupaudio::titleFinished() {
    return musicComplete;
}

bool startupaudio::finished() {
    if (!source) return true;
    XAUDIO2_VOICE_STATE state{};
    source->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    return state.BuffersQueued == 0;
}

void startupaudio::close() {
    stopMusic();
    positionalPlayback.reset();
    startPlayback.stop();
    if (source) { source->DestroyVoice(); source = nullptr; }
    if (master) { master->DestroyVoice(); master = nullptr; }
    engine.Reset();
    logoBuffer.clear();
    if (comInitialized) { CoUninitialize(); comInitialized = false; }
}

void startupaudio::stop() { stopMusic(); }
