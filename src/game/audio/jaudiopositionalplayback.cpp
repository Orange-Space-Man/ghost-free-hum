#include "resources/dolresources.h"
#include "game/audio/jaudiopositionalplayback.h"
#include "jlib/jaudio/jaudioeffectparameter.h"
#include "jlib/jaudio/jaudioeffectselection.h"
#include "jlib/jaudio/jaudioeffectrelease.h"
#include "jlib/jaudio/jaudioeffectvoice.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    constexpr std::array<unsigned, 4> soundIds{0x1809, 0x180a, 0x180b, 0x181e};
    constexpr unsigned effectRecordCountAddress = 0x804d78b8;
    constexpr unsigned effectParameterCountAddress = 0x804d78d4;
    constexpr unsigned maximumDistanceAddress = 0x804dd170;
    constexpr unsigned minimumVolumeAddress = 0x804dd174;
    constexpr unsigned fullDistanceAddress = 0x804dd178;

    float scalar(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
        float result;
        std::memcpy(&result, &bits, 4);
        return result;
    }

}

struct jaudio::PositionalPlayback::Handle : PositionalSound {
    Handle(PositionalPlayback& owner, unsigned id, std::uint32_t flags) : owner(owner), id(id), metadata(flags) {}

    void stop(unsigned frames) override;

    std::uint32_t flags() const override { return id; }
    std::uint32_t metadataFlags() const override { return metadata; }
    void clearBinding() override {
        PositionalSound::clearBinding();
        if (animationBinding) *animationBinding = nullptr;
        animationBinding = nullptr;
    }
    void retainPosition(const std::array<float, 3>& retained) override {
        PositionalSound::retainPosition(retained);
        record->actor = nullptr;
        record->position = &retained;
    }
    PositionalPlayback& owner;
    EffectRecord* record = nullptr;
    unsigned id;
    std::uint32_t metadata;
    actoranimation::SoundHandle** animationBinding = nullptr;
    bool active = true;
};

struct jaudio::PositionalPlayback::Runtime : EffectAdmissionServices, EffectCategoryServices {
    PositionalPlayback& owner;
    Playback master;
    std::vector<EffectRecord> records;
    EffectPool pool;
    EffectParameterPool parameters;
    std::vector<EffectRecord*> channels;
    std::array<EffectMetadata, 4> metadata{};
    std::array<SpatialParameters, 4> defaults{};
    std::uint8_t parameterFlags;
    std::function<std::uint32_t(const std::array<float, 3>&)> sourceContext;
    std::vector<EffectReleaseSequence> sequences;
    std::uint32_t duckMask = 0;
    unsigned firstChannel, actorLimit;
    EffectReleaseContext releaseContext;
    std::vector<std::vector<EffectListenerDistance>> distances;
    bool started = false;

    static unsigned count(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }

    Runtime(PositionalPlayback& owner, const std::vector<std::uint8_t>& executable, gameassets::Package& package, const Resources& resources) :
        owner(owner), records(count(executable, effectRecordCountAddress)), parameters(count(executable, effectParameterCountAddress)),
        channels(resources.effectCategories.at(0)[1][0]), parameterFlags(resources.soundParameterFlags[0]),
        firstChannel(resources.effectCategories.at(0)[0][0]), actorLimit(resources.effectCategories.at(0)[1][1]),
        releaseContext{pool, parameters, channels, master, duckMask, sequences}, distances(records.size(), std::vector<EffectListenerDistance>(1)) {
        if (records.empty()) throw std::runtime_error("Original sound record pool is empty.");
        pool.free = records.data();
        for (unsigned index = 0; index + 1 < records.size(); ++index) records[index].next = &records[index + 1];
        std::string error;
        if (!master.loadEffects(package, resources, error)) throw std::runtime_error(error);
        for (unsigned index = 0; index < soundIds.size(); ++index) {
            const auto& info = *soundInfo(resources, soundIds[index]);
            metadata[index] = {owner.soundFlags[index], info[4]};
            const std::uint32_t pitchBits = (unsigned(info[8]) << 24) | (unsigned(info[9]) << 16) | (unsigned(info[10]) << 8) | info[11];
            std::memcpy(&defaults[index].pitch, &pitchBits, sizeof(pitchBits));
            defaults[index].volume = static_cast<float>(info[12]) / 127;
            defaults[index].fxmix = static_cast<float>(info[13]) / 127;
            if (metadata[index].flags & ~0x8000u) throw std::runtime_error("First-area effect metadata requires additional runtime hooks.");
        }
        master.start();
        started = true;
    }

    void stop(EffectRecord& record) override {
        auto* handle = &record;
        stopEffect(&handle, 0, releaseContext);
    }
    void release(EffectRecord& record) override { releaseEffect(record, releaseContext); }
    EffectParameters* acquireParameters() override { return parameters.acquire(); }
    EffectRank prepareRank(EffectRecord& record) override {
        return prepareEffectRank(distances.at(&record - records.data()), {owner.listener}, record.position,
            record.listener, 1, record.priority(), record.metadataFlags(), owner.maximumDistance);
    }

    struct Voice : EffectSequenceVoiceServices, EffectSpatialServices {
        Runtime& runtime;
        EffectRecord& record;
        std::array<float, 3> relative;
        Voice(Runtime& runtime, EffectRecord& record, unsigned channel) :
            EffectSequenceVoiceServices(runtime.master, record, channel, 1, 1, runtime.releaseContext), runtime(runtime), record(record) {
            const auto& distance = runtime.distances.at(&record - runtime.records.data())[0];
            relative = {distance.x, distance.y, distance.z};
        }
        void initializePosition() override { throw std::runtime_error("Ducking effect initialization is outside the first-area sound set."); }
        void initializeParameters(EffectControls& controls) override {
            const auto& values = runtime.defaults.at(record.metadataSource - runtime.metadata.data());
            if (runtime.parameterFlags & 4) controls.set(EffectControl::volume, 1, values.volume, 0);
            if (runtime.parameterFlags & 8) controls.set(EffectControl::fxmix, 1, values.fxmix, 0);
            if (runtime.parameterFlags & 2) controls.set(EffectControl::pitch, 1, values.pitch, 0);
        }
        void updateParameters(EffectControls& controls, unsigned state, std::uint8_t offset) override {
            const auto& distance = runtime.distances.at(&record - runtime.records.data())[0];
            EffectSpatialState spatial;
            spatial.flags = record.metadataFlags(); spatial.soundState = state; spatial.pitchOffset = offset;
            spatial.distance = std::sqrt(distance.squared); spatial.maximumDistance = runtime.owner.maximumDistance;
            spatial.relativeDepth = relative[2];
            controls.updateSpatial(spatial, *this);
        }
        float distanceVolume(unsigned curve) override { return runtime.owner.spatial(relative, curve << 16).volume; }
        float pan() override { return runtime.owner.spatial(relative, 0).pan; }
        float dopplerPitch() override { throw std::runtime_error("Doppler effect is outside the first-area sound set."); }
        float roomFxmix() override { return static_cast<float>((record.context & 63) * 2) / 127; }
        std::uint32_t random() override { throw std::runtime_error("Randomized effect is outside the first-area sound set."); }
        unsigned variant() override { return (record.context >> 16) & 15; }
        unsigned distanceVariant() override { throw std::runtime_error("Distance variant is outside the first-area sound set."); }
        unsigned roomSize() override { return (record.context >> 6) & 3; }
    };
};

void jaudio::PositionalPlayback::Handle::stop(unsigned frames) {
    std::lock_guard<std::recursive_mutex> lock(owner.owners.mutex);
    if (!active || !record) return;
    auto* local = record;
    stopEffect(&local, frames, owner.runtime->releaseContext);
    if (!record) owner.remove(*this);
}

jaudio::PositionalPlayback::PositionalPlayback(PositionOwners& owners, const std::vector<std::uint8_t>& executable,
    gameassets::Package& package, const Resources& resources,
    std::function<std::uint32_t(const std::array<float, 3>&)> sourceContext) : owners(owners),
    maximumDistance(scalar(executable, maximumDistanceAddress)), fullDistance(scalar(executable, fullDistanceAddress)),
    minimumVolume(scalar(executable, minimumVolumeAddress)) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    if (owners.categories.size() < 5) owners.categories.resize(5);
    for (unsigned i = 0; i < soundIds.size(); ++i) {
        const auto* info = soundInfo(resources, soundIds[i]);
        if (!info) throw std::runtime_error("Missing first-area positional sound metadata.");
        const auto& bytes = *info;
        soundFlags[i] = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }
    runtime = std::make_unique<Runtime>(*this, executable, package, resources);
    runtime->sourceContext = std::move(sourceContext);
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
    if (!runtime->started) { runtime->master.start(); runtime->started = true; }
    if (binding && *binding) (*binding)->stop(0);
    for (auto& existing : handles) if (existing->active && existing->id == id && existing->ownerPosition == &position) existing->stop(0);
    const unsigned index = static_cast<unsigned>(found - soundIds.begin());
    auto handle = std::make_unique<Handle>(*this, id, soundFlags[index]);
    handle->ownerPosition = handle->position = &position;
    handle->binding = binding;
    const EffectActorSource source{&position, &position, nullptr, runtime->sourceContext ? runtime->sourceContext(position) : 0};
    const EffectRequest request{id, 0, 4, &source, &runtime->metadata[index]};
    admitEffect(runtime->pool, &handle->record, request, static_cast<std::uint8_t>(runtime->actorLimit), *runtime);
    for (auto& existing : handles) if (existing->active && !existing->record) remove(*existing);
    if (!handle->record) return nullptr;
    handle->next = owners.categories[1];
    owners.categories[1] = handle.get();
    if (binding) *binding = handle.get();
    auto* result = handle.get();
    const auto reusable = std::find_if(handles.begin(), handles.end(), [](const auto& entry) {
        return !entry->active;
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
            auto* sound = play(id, position);
            if (!sound) { binding = nullptr; return; }
            auto& handle = owned(*sound);
            handle.animationBinding = &binding;
            binding = &handle;
        },
        [this](actoranimation::SoundHandle& sound, float value) {
            std::lock_guard<std::recursive_mutex> lock(owners.mutex);
            owned(sound).record->parameters->controls.set(EffectControl::pitch, 0, value, 0);
        },
        [this](actoranimation::SoundHandle& sound, float value) {
            std::lock_guard<std::recursive_mutex> lock(owners.mutex);
            owned(sound).record->parameters->controls.set(EffectControl::volume, 0, value, 0);
        },
        [this](actoranimation::SoundHandle& sound, float value) {
            std::lock_guard<std::recursive_mutex> lock(owners.mutex);
            owned(sound).record->parameters->controls.set(EffectControl::pan, 0, value, 0);
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
    for (auto& handle : handles) if (handle->record) handle->record->position = handle->position;
    std::uint16_t port = 0;
    if (!runtime->master.readEffectPort(runtime->firstChannel, 2, port)) return;
    updateEffectCategory(runtime->pool, runtime->channels, *runtime);
    for (unsigned channel = 0; channel < runtime->channels.size(); ++channel) {
        auto* record = runtime->channels[channel];
        if (!record) continue;
        Runtime::Voice voice(*runtime, *record, runtime->firstChannel + channel);
        updateEffectVoice(*record, 1, voice);
    }
    for (auto* record = runtime->pool.active; record; record = record->next) {
        if (record->parameters) record->parameters->controls.advance(false);
    }
    runtime->master.updateSequenceControls();
    for (auto& handle : handles) if (handle->active && !handle->record) remove(*handle);
}

bool jaudio::PositionalPlayback::mix(DspMixer& mixer, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    return runtime->master.mix(mixer, error);
}

void jaudio::PositionalPlayback::stop() {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    for (auto& handle : handles) handle->stop(0);
    runtime->master.stop();
    runtime->started = false;
}

bool jaudio::PositionalPlayback::finished() const {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    return !runtime->pool.active && !runtime->master.hasVoices();
}
