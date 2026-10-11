#include "resources/dolresources.h"
#include "game/audio/jaudiopositionalplayback.h"
#include "jlib/jaudio/jaudioeffectparameter.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    constexpr std::array<unsigned, 4> soundIds{0x1809, 0x180a, 0x180b, 0x181e};

    float scalar(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
        float result;
        std::memcpy(&result, &bits, 4);
        return result;
    }

    std::array<float, 3> transform(const std::array<float, 12>& matrix, const std::array<float, 3>& position) {
        std::array<float, 3> result;
        for (unsigned i = 0; i < 3; ++i) result[i] = std::fma(matrix[i * 4 + 2], position[2],
            std::fma(matrix[i * 4 + 1], position[1], matrix[i * 4] * position[0])) + matrix[i * 4 + 3];
        return result;
    }
}

struct jaudio::PositionalPlayback::Handle : PositionalSound {
    Handle(PositionalPlayback& owner, const Playback& prepared, unsigned id, std::uint32_t flags) :
        owner(owner), playback(prepared), id(id), metadata(flags) {}

    void stop(unsigned frames) override {
        std::lock_guard<std::recursive_mutex> lock(owner.owners.mutex);
        if (!active) return;
        if (frames) {
            stopping = true;
            fade.set(0, frames);
            return;
        }
        playback.release();
        owner.remove(*this);
    }

    std::uint32_t flags() const override { return id; }
    std::uint32_t metadataFlags() const override { return metadata; }
    void clearBinding() override {
        PositionalSound::clearBinding();
        if (animationBinding) *animationBinding = nullptr;
        animationBinding = nullptr;
    }
    PositionalPlayback& owner;
    Playback playback;
    unsigned id;
    std::uint32_t metadata;
    SpatialParameters current, target;
    SpatialParameters animation;
    actoranimation::SoundHandle** animationBinding = nullptr;
    EffectParameter fade{1};
    float fadeVolume = 1;
    bool stopping = false;
    bool active = true;
};

jaudio::PositionalPlayback::PositionalPlayback(PositionOwners& owners, const std::vector<std::uint8_t>& executable,
    gameassets::Package& package, const Resources& resources) : owners(owners),
    maximumDistance(scalar(executable, 0x804dd170)), fullDistance(scalar(executable, 0x804dd178)),
    minimumVolume(scalar(executable, 0x804dd174)) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    if (owners.categories.size() < 5) owners.categories.resize(5);
    for (unsigned i = 0; i < soundIds.size(); ++i) {
        std::string error;
        if (!prepared[i].loadPositionalEffect(package, resources, soundIds[i], error)) throw std::runtime_error(error);
        const auto& bytes = *soundInfo(resources, soundIds[i]);
        soundFlags[i] = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }
}

jaudio::PositionalPlayback::~PositionalPlayback() { stop(); }

void jaudio::PositionalPlayback::remove(Handle& handle) {
    auto** link = &owners.categories.at(1);
    while (*link && *link != &handle) link = &(*link)->next;
    if (*link) *link = handle.next;
    handle.clearBinding();
    handle.next = nullptr;
    handle.ownerPosition = handle.position = nullptr;
    handle.active = false;
}

jaudio::PositionalSound* jaudio::PositionalPlayback::play(unsigned id, const std::array<float, 3>& position, PositionalSound** binding) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    const auto found = std::find(soundIds.begin(), soundIds.end(), id);
    if (found == soundIds.end()) throw std::runtime_error("Positional sound is outside the recovered first-area door scope.");
    if (binding && *binding) (*binding)->stop(0);
    for (auto& existing : handles) if (existing->active && existing->id == id && existing->ownerPosition == &position) existing->stop(0);
    const unsigned index = static_cast<unsigned>(found - soundIds.begin());
    auto handle = std::make_unique<Handle>(*this, prepared[index], id, soundFlags[index]);
    handle->ownerPosition = handle->position = &position;
    handle->binding = binding;
    handle->current = handle->target = spatial(transform(listener, position), handle->metadata);
    handle->playback.start();
    handle->next = owners.categories[1];
    owners.categories[1] = handle.get();
    if (binding) *binding = handle.get();
    auto* result = handle.get();
    const auto reusable = std::find_if(handles.begin(), handles.end(), [](const auto& entry) {
        return !entry->active && entry->playback.voicesFinished();
    });
    if (reusable == handles.end()) handles.push_back(std::move(handle));
    else *reusable = std::move(handle);
    return result;
}

jaudio::PositionalPlayback::Handle& jaudio::PositionalPlayback::owned(actoranimation::SoundHandle& sound) {
    for (auto& handle : handles) if (handle.get() == &sound && handle->active) return *handle;
    throw std::runtime_error("Animation sound parameter requires a live handle owned by its playback manager.");
}

actoranimation::SoundServices jaudio::PositionalPlayback::animationServices(const std::array<float, 3>& position) {
    return {
        [this, &position](unsigned id, actoranimation::SoundHandle*& binding) {
            std::lock_guard<std::recursive_mutex> lock(owners.mutex);
            if (binding) owned(*binding).stop(0);
            auto& handle = owned(*play(id, position));
            handle.animationBinding = &binding;
            binding = &handle;
        },
        [this](actoranimation::SoundHandle& sound, float value) {
            std::lock_guard<std::recursive_mutex> lock(owners.mutex);
            owned(sound).animation.pitch = value;
        },
        [this](actoranimation::SoundHandle& sound, float value) {
            std::lock_guard<std::recursive_mutex> lock(owners.mutex);
            owned(sound).animation.volume = value;
        },
        [this](actoranimation::SoundHandle& sound, float value) {
            std::lock_guard<std::recursive_mutex> lock(owners.mutex);
            owned(sound).animation.pan = value;
        }
    };
}

jaudio::SpatialParameters jaudio::PositionalPlayback::spatial(const std::array<float, 3>& relative, std::uint32_t flags) const {
    SpatialParameters result;
    const float distance = std::sqrt((relative[0] * relative[0] + relative[1] * relative[1]) + relative[2] * relative[2]);
    if (!(flags & 2) && distance >= fullDistance) {
        const unsigned curve = (flags >> 16) & 7;
        float range = maximumDistance - fullDistance;
        range = curve < 4 ? range * static_cast<float>(1u << curve) : range / static_cast<float>(1u << ((curve & 3) + 1));
        const float floor = curve < 4 ? minimumVolume : 0;
        result.volume = distance - fullDistance >= range ? floor : floor + (1 - floor) * (1 - (distance - fullDistance) / range);
    }
    float x = std::abs(relative[0]), z = std::abs(relative[2]);
    if (x >= 1 || z >= 1) {
        x = std::min(x, 500.0f);
        z = std::min(z, 500.0f);
        if (relative[0] > 0 && x >= z) result.pan = 1 - (1000 - x) / (12 * (1000 - z));
        else if (relative[0] <= 0 && x >= z) result.pan = (1000 - x) / (12 * (1000 - z));
        else result.pan = 0.5f + relative[0] / (2.4f * z);
    }
    if (!(flags & 4)) result.fxmix = static_cast<float>(static_cast<unsigned>(distance >= maximumDistance ? 80 : 80 * (distance / maximumDistance))) / 127;
    float dolby = 0;
    if (relative[2] >= -300) {
        if (relative[2] >= 0) dolby = relative[2] < 1000 ? 30 + 97 * (relative[2] / 1000) : 127;
        else dolby = (30 * (-300 - relative[2])) / -300;
    }
    result.dolby = static_cast<float>(static_cast<unsigned>(dolby) & 255) / 127;
    return result;
}

void jaudio::PositionalPlayback::update(const std::array<float, 12>& matrix) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    listener = matrix;
    owners.update();
    for (auto& handle : handles) {
        if (!handle->active) continue;
        if (handle->stopping && handle->fade.current == 0) {
            handle->fadeVolume = 0;
            handle->stop(0);
            continue;
        }
        handle->target = spatial(transform(listener, *handle->position), handle->metadata);
        auto& current = handle->current;
        const auto& target = handle->target;
        current.volume += (target.volume - current.volume) / 3;
        current.pan += (target.pan - current.pan) / 3;
        current.fxmix += (target.fxmix - current.fxmix) / 3;
        current.dolby += (target.dolby - current.dolby) / 3;
        handle->fadeVolume = handle->fade.current;
        handle->fade.advance();
    }
}

bool jaudio::PositionalPlayback::mix(DspMixer& mixer, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    for (auto& handle : handles) {
        if (!handle->active && handle->playback.voicesFinished()) continue;
        auto parameters = handle->current;
        parameters.volume *= handle->animation.volume;
        parameters.volume *= handle->fadeVolume;
        parameters.pitch *= handle->animation.pitch;
        parameters.pan = std::clamp((parameters.pan - 0.5f) + (handle->animation.pan - 0.5f) + 0.5f, 0.0f, 1.0f);
        if (!handle->playback.mix(mixer, error, parameters)) { handle->stop(0); return false; }
        if (handle->active && handle->playback.voicesFinished()) remove(*handle);
    }
    return true;
}

void jaudio::PositionalPlayback::stop() {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    for (auto& handle : handles) {
        handle->stop(0);
        handle->playback.stop();
    }
}

bool jaudio::PositionalPlayback::finished() const {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    return std::none_of(handles.begin(), handles.end(), [](const auto& handle) {
        return handle->active || !handle->playback.voicesFinished();
    });
}
