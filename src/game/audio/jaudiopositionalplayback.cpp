#include "resources/dolresources.h"
#include "game/audio/jaudiopositionalplayback.h"
#include "game/audio/gameplaymusicowner.h"
#include "jlib/jaudio/jaudioeffectparameter.h"
#include "jlib/jaudio/jaudioeffectselection.h"
#include "jlib/jaudio/jaudioeffectrelease.h"
#include "jlib/jaudio/jaudioeffectvoice.h"
#include "jlib/jaudio/jaudiosequencerandom.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    constexpr std::array<unsigned, 228> soundIds{0x830, 0x831, 0x840, 0x3813, 0x380c, 0x380d, 0x107f, 0x3801, 0x3802, 0x1805, 0x80f, 0x288c, 0x84f, 0x81e, 0x1829, 0x839, 0x83d, 0x1809, 0x180a, 0x180b, 0x181e, 0x1824, 0x280f, 0x2810, 0x282b,
        0x186e, 0x1841, 0x1812, 0x18ba, 0x18bd, 0x41, 0x810, 0x811, 0x850, 0x4808, 0x4828, 0x828, 0x3b, 0x43, 0x801, 2, 0x805, 9,
        0x288a, 0x1028, 0x837, 0x84c, 0x813, 0x2814, 0x2805, 0x281c,
        0x1842, 0x182c, 0x1820, 0x1823, 0x1877, 0x1868, 0x18bb, 0x4820, 0x4822, 0x1815, 0x1816, 0x2828, 0x288e, 0x826, 0x2800, 0x827,
        0x182a, 0x1850, 0x1861, 0x1862, 0x1863, 0x1864, 0x103c, 0x1808, 0x187e, 0x4872, 0x1827, 0x4870, 0x4871, 0x380b, 0x1878,
        0x489b, 0x182b, 0x489a, 0x482d, 0x804, 0x4825, 0x4891, 0x4818, 0x4819, 0x282c, 0x186a, 0x481a, 0x4898, 0x489d, 0x4811, 0x2823, 0x4894, 0x480b, 0x287d, 0x2850, 0x2851, 0x2852, 0x2853, 0x2854, 0x183e,
        0x28f0, 0x28f1, 0x28fa, 0x2921, 0x2924, 0x293b, 0x294a, 0x2965, 0x2824, 0x1819, 0x1875, 0x1876, 0x1898, 0x1879, 0x18cb, 0x18b0, 0x18c8, 0x295f, 0x2913, 0x294e, 0x294f, 0x2966, 0x2955, 0x2953, 0x2914, 0x2918, 0x2948, 0x2962, 0x803, 0x286a, 0x81d, 0x2822, 0x4823, 0x4802, 0x481d, 0x408f, 0x481e, 0x4892, 0x4895,
        0x28b0, 0x28d0, 0x28f8, 0x2900, 0x2901, 0x290c, 0x2919, 0x291a, 0x291b, 0x2922, 0x2929, 0x292e, 0x292f, 0x2930, 0x2931, 0x2932, 0x2933, 0x2940, 0x2941, 0x2942, 0x2943, 0x2949, 0x294b, 0x294c, 0x2957, 0x2963, 0x2967,
        0x1039, 0x22, 0x80b, 0x1080, 0x1081, 0x1882, 0x1883, 0x282e, 0x489e, 0x2818, 0x287f, 0x1895, 0xe, 0x84a, 0x84b, 0x381b, 0x1033, 0x1035, 0x3a, 0x2809, 0x2821, 0x2877, 0x1810, 0x281b, 0x1800, 0x281d, 0x28f4, 0x28f5, 0x28f6, 0x28f7, 0x290f, 0x2939, 0x2945, 0x2946, 0x2947, 0x2950, 0x2952, 0x2954, 0x295b, 0x295d,
        0x2959, 0x293f, 0x2934, 0x2907, 0x2908, 0x2958, 0x293e, 0x28f9, 0x295a, 0x290b,
        0x284a, 0x284b, 0x284c, 0x284d, 0x284e};
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
    EffectParameterPool parameters;
    std::array<EffectMetadata, soundIds.size()> metadata{};
    std::array<SpatialParameters, soundIds.size()> defaults{};
    std::array<float, 5> furnitureSound;
    std::array<float, 3> thunderPosition;
    std::array<float, 7> thunderPan;
    std::uint8_t parameterFlags;
    std::function<std::uint32_t(const std::array<float, 3>&, bool)> sourceContext;
    std::vector<EffectReleaseSequence> sequences;
    std::uint32_t duckMask = 0;
    std::uint32_t playerContext = 0;
    struct Category {
        std::vector<EffectRecord> records;
        EffectPool pool;
        std::vector<EffectRecord*> channels;
        unsigned firstChannel, actorLimit;
        bool blocked = false;
        EffectReleaseContext releaseContext;
        std::vector<std::vector<EffectListenerDistance>> distances;
        Category(Runtime& runtime, unsigned recordCount, unsigned firstChannel, unsigned channelCount, unsigned actorLimit) :
            records(recordCount), channels(channelCount), firstChannel(firstChannel), actorLimit(actorLimit),
            releaseContext{pool, runtime.parameters, channels, runtime.master, runtime.duckMask, runtime.sequences},
            distances(recordCount, std::vector<EffectListenerDistance>(1)) {
            if (records.empty()) throw std::runtime_error("Original sound record pool is empty.");
            pool.free = records.data();
            for (unsigned index = 0; index + 1 < records.size(); ++index) records[index].next = &records[index + 1];
        }
    };
    std::array<std::unique_ptr<Category>, 5> categories;
    bool started = false;
    PositionalSound* vacuumSound = nullptr;
    const std::array<float, 3>* vacuumPosition = nullptr;
    float vacuumPitch = 0.5f;
    unsigned vacuumRequests = 0, vacuumMode = 0, vacuumFade = 0, vacuumAccent = 0;
    bool vacuumRunning = false, vacuumEngaged = false;
    std::array<float, 13> drainPitch{};
    unsigned drainStep = 0;
    std::uint64_t lastDrain = 0;
    PositionalSound* drainSound = nullptr;

    static unsigned count(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }

    Runtime(PositionalPlayback& owner, const std::vector<std::uint8_t>& executable, gameassets::Package& package, const Resources& resources) :
        owner(owner), parameters(count(executable, effectParameterCountAddress)), parameterFlags(resources.soundParameterFlags[0]) {
        for (unsigned i = 0; i < drainPitch.size(); ++i) drainPitch[i] = scalar(executable, 0x80217488 + i * 4);
        unsigned firstChannel = 0;
        for (unsigned index = 0; index < categories.size(); ++index) {
            const auto& limits = resources.effectCategories.at(0)[index];
            categories[index] = std::make_unique<Category>(*this, count(executable, effectRecordCountAddress), firstChannel, limits[0], limits[1]);
            firstChannel += limits[0];
        }
        std::string error;
        if (!master.loadEffects(package, resources, error)) throw std::runtime_error(error);
        constexpr unsigned furnitureSoundParameters = 0x804dd12c;
        for (unsigned i = 0; i < furnitureSound.size(); ++i)
            furnitureSound[i] = scalar(executable, furnitureSoundParameters + i * 4);
        for (unsigned index = 0; index < soundIds.size(); ++index) {
            const auto& info = *soundInfo(resources, soundIds[index]);
            metadata[index] = {owner.soundFlags[index], info[4]};
            const std::uint32_t pitchBits = (unsigned(info[8]) << 24) | (unsigned(info[9]) << 16) | (unsigned(info[10]) << 8) | info[11];
            std::memcpy(&defaults[index].pitch, &pitchBits, sizeof(pitchBits));
            defaults[index].volume = static_cast<float>(info[12]) / 127;
            defaults[index].fxmix = static_cast<float>(info[13]) / 127;
            if (metadata[index].flags & ~(soundIds[index] == 0x489e ? 0xf0078cd7u : 0xd0078cd7u)) throw std::runtime_error("First-area effect " + std::to_string(soundIds[index]) +
                " requires additional metadata hooks: " + std::to_string(metadata[index].flags));
        }
        master.start();
        started = true;
    }

    void stop(EffectRecord& record) override {
        auto* handle = &record;
        stopEffect(&handle, 0, categories.at(record.soundId >> 12)->releaseContext);
    }
    void release(EffectRecord& record) override { releaseEffect(record, categories.at(record.soundId >> 12)->releaseContext); }
    EffectParameters* acquireParameters() override { return parameters.acquire(); }
    std::vector<EffectListenerDistance>& listenerDistances(EffectRecord& record) {
        auto& category = *categories.at(record.soundId >> 12);
        return category.distances.at(&record - category.records.data());
    }
    EffectRank prepareRank(EffectRecord& record) override {
        return prepareEffectRank(listenerDistances(record), {owner.listener}, record.position,
            record.listener, 1, record.priority(), record.metadataFlags(), owner.maximumDistance);
    }

    struct Voice : EffectSequenceVoiceServices, EffectSpatialServices {
        Runtime& runtime;
        EffectRecord& record;
        std::array<float, 3> relative;
        Voice(Runtime& runtime, EffectRecord& record, unsigned channel) :
            EffectSequenceVoiceServices(runtime.master, record, channel, 1, 1,
                runtime.categories.at(record.soundId >> 12)->releaseContext), runtime(runtime), record(record) {
            const auto& distance = runtime.listenerDistances(record)[0];
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
            const auto& distance = runtime.listenerDistances(record)[0];
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
        std::uint32_t random() override { return sequenceRandom(); }
        unsigned variant() override { return (record.context >> 16) & 15; }
        unsigned distanceVariant() override { throw std::runtime_error("Distance variant is outside the first-area sound set."); }
        unsigned roomSize() override { return (record.context >> 6) & 3; }
    };
};

void jaudio::PositionalPlayback::Handle::stop(unsigned frames) {
    std::lock_guard<std::recursive_mutex> lock(owner.owners.mutex);
    if (!active || !record) return;
    auto* local = record;
    stopEffect(&local, frames, owner.runtime->categories.at(id >> 12)->releaseContext);
    if (!record) owner.remove(*this);
}

jaudio::PositionalPlayback::PositionalPlayback(PositionOwners& owners, const std::vector<std::uint8_t>& executable,
    gameassets::Package& package, const Resources& resources,
    std::function<std::uint32_t(const std::array<float, 3>&, bool)> sourceContext) : owners(owners),
    maximumDistance(scalar(executable, maximumDistanceAddress)), fullDistance(scalar(executable, fullDistanceAddress)),
    minimumVolume(scalar(executable, minimumVolumeAddress)) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    if (owners.categories.size() < 5) owners.categories.resize(5);
    soundFlags.resize(soundIds.size());
    for (unsigned i = 0; i < soundIds.size(); ++i) {
        const auto* info = soundInfo(resources, soundIds[i]);
        if (!info) throw std::runtime_error("Missing first-area positional sound metadata.");
        const auto& bytes = *info;
        soundFlags[i] = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }
    runtime = std::make_unique<Runtime>(*this, executable, package, resources);
    runtime->sourceContext = std::move(sourceContext);
    for (unsigned i = 0; i < runtime->thunderPosition.size(); ++i)
        runtime->thunderPosition[i] = scalar(executable, 0x8038e71c + i * 4);
    for (unsigned i = 0; i < runtime->thunderPan.size(); ++i)
        runtime->thunderPan[i] = scalar(executable, 0x804dd0a4 + i * 4);
}

jaudio::PositionalPlayback::~PositionalPlayback() { stop(); }

void jaudio::PositionalPlayback::remove(Handle& handle) {
    auto** link = &owners.categories.at(handle.id >> 12);
    while (*link && *link != &handle) link = &(*link)->next;
    if (*link) *link = handle.next;
    handle.clearBinding();
    handle.next = nullptr;
    handle.ownerPosition = handle.position = nullptr;
    handle.active = false;
}

jaudio::PositionalSound* jaudio::PositionalPlayback::play(unsigned id, const std::array<float, 3>& position, PositionalSound** binding) {
    return playAt(id, &position, binding);
}

bool jaudio::PositionalPlayback::admitted(unsigned id) {
    return std::find(soundIds.begin(), soundIds.end(), id) != soundIds.end();
}

jaudio::PositionalSound* jaudio::PositionalPlayback::playGlobal(unsigned id, PositionalSound** binding) {
    return playAt(id, nullptr, binding);
}

void jaudio::PositionalPlayback::stopSound(unsigned id) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    const auto category = (id >> 12) & 255;
    if (category >= owners.categories.size()) return;
    auto* sound = owners.categories[category];
    while (sound) {
        auto* next = sound->next;
        if (sound->flags() == id) sound->stop(0);
        sound = next;
    }
}

void jaudio::PositionalPlayback::setPlayerContext(std::uint32_t context) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    runtime->playerContext = context;
}

jaudio::PositionalSound* jaudio::PositionalPlayback::playAt(unsigned id, const std::array<float, 3>* position, PositionalSound** binding) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    const auto found = std::find(soundIds.begin(), soundIds.end(), id);
    if (found == soundIds.end()) throw std::runtime_error("Positional sound " + std::to_string(id) + " is outside the recovered first-area sound scope.");
    auto& category = *runtime->categories.at(id >> 12);
    if (category.blocked) {
        if (binding) {
            for (auto& handle : handles) if (handle->binding == binding) handle->binding = nullptr;
            *binding = nullptr;
        }
        return nullptr;
    }
    if (!runtime->started) { runtime->master.start(); runtime->started = true; }
    if (binding && *binding && ((*binding)->flags() != id || (*binding)->ownerPosition != position)) (*binding)->stop(0);
    for (auto& existing : handles) {
        if (!existing->active || existing->id != id || existing->ownerPosition != position) continue;
        if (!(id & 0x800) && existing->record && existing->record->state == 5) {
            existing->record->state = 4;
            return existing.get();
        }
    }
    for (auto& existing : handles) if (existing->active && existing->id == id && existing->ownerPosition == position) existing->stop(0);
    const unsigned index = static_cast<unsigned>(found - soundIds.begin());
    auto handle = std::make_unique<Handle>(*this, id, soundFlags[index]);
    handle->ownerPosition = handle->position = position;
    handle->binding = binding;
    const bool queryFloor = (id & 0xff000) == 0 || (id & 0xff000) == 0x2000 || (soundFlags[index] & 0x80000000);
    const EffectActorSource source{position, position, nullptr,
        position && runtime->sourceContext ? runtime->sourceContext(*position, queryFloor) : runtime->playerContext};
    const EffectRequest request{id, 0, 4, &source, &runtime->metadata[index]};
    admitEffect(category.pool, &handle->record, request, static_cast<std::uint8_t>(category.actorLimit), *runtime);
    for (auto& existing : handles) if (existing->active && !existing->record) remove(*existing);
    if (!handle->record) return nullptr;
    handle->next = owners.categories[id >> 12];
    owners.categories[id >> 12] = handle.get();
    if (binding) *binding = handle.get();
    auto* result = handle.get();
    const auto reusable = std::find_if(handles.begin(), handles.end(), [](const auto& entry) {
        return !entry->active;
    });
    if (reusable == handles.end()) handles.push_back(std::move(handle));
    else *reusable = std::move(handle);
    return result;
}

void jaudio::PositionalPlayback::setPort(PositionalSound& sound, unsigned port, std::uint16_t value) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    auto& handle = owned(sound);
    if (port >= 16) throw std::out_of_range("Effect port is outside the original parameter block.");
    auto& record = *handle.record;
    if (!record.parameters) return;
    if (record.state == 1) {
        record.parameters->ports[port] = value;
        record.parameters->pendingPorts |= static_cast<std::uint16_t>(1u << port);
    } else {
        if (record.parameters->ports[port] == value) return;
        runtime->master.publishEffectPort(record.channel, port, value);
        record.parameters->ports[port] = value;
    }
}

void jaudio::PositionalPlayback::playThunder() {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    const auto context = runtime->sourceContext ? runtime->sourceContext(runtime->thunderPosition, false) : 0;
    auto* sound = playGlobal(0x1805);
    if (!sound) return;
    auto& controls = owned(*sound).record->parameters->controls;
    if (context & 0x200) controls.set(EffectControl::fxmix, 7, 0, 0);
    else {
        const auto& bounds = runtime->thunderPan;
        const float x = std::clamp(runtime->thunderPosition[0], bounds[0], bounds[1]);
        const float z = std::clamp(runtime->thunderPosition[2], bounds[3], bounds[4]);
        controls.set(EffectControl::fxmix, 7, (bounds[5] + z) / bounds[6], 0);
        controls.set(EffectControl::pan, 7, (bounds[1] + x) / bounds[2], 0);
    }
}

void jaudio::PositionalPlayback::requestVacuum(float strength, const std::array<float, 3>& position, unsigned mode) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    runtime->vacuumPosition = &position;
    runtime->vacuumPitch = 0.5f + 0.5f * strength;
    runtime->vacuumMode = mode;
    runtime->vacuumFade = 0;
    runtime->vacuumRunning = true;
    ++runtime->vacuumRequests;
}

void jaudio::PositionalPlayback::playCaptureDrain() {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    const auto tick = runtime->master.ticks();
    if (tick - runtime->lastDrain > 6) runtime->drainStep = 0;
    if (auto* sound = playGlobal(0x381b, &runtime->drainSound))
        owned(*sound).record->parameters->controls.set(EffectControl::pitch, 0, runtime->drainPitch[runtime->drainStep], 0);
    runtime->drainStep = std::min(runtime->drainStep + 1, 12u);
    runtime->lastDrain = tick;
}

void jaudio::PositionalPlayback::playDeathEffects(const std::array<float, 3>& position) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    play(0x280f, position);
    for (unsigned index : {1u, 4u}) {
        auto& category = *runtime->categories[index];
        category.blocked = true;
        for (auto* record = category.pool.active; record; record = record->next) {
            if (record->parameters) record->parameters->controls.set(EffectControl::volume, 7, 0, 3);
        }
    }
}

float jaudio::PositionalPlayback::interactionPitch(PositionalSound& sound) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    return owned(sound).record->parameters->controls.value(EffectControl::pitch, 0);
}

void jaudio::PositionalPlayback::setInteractionPitch(PositionalSound& sound, float value) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    owned(sound).record->parameters->controls.set(EffectControl::pitch, 0, value, 0);
}

void jaudio::PositionalPlayback::setInteractionVolume(PositionalSound& sound, float value) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    owned(sound).record->parameters->controls.set(EffectControl::volume, 0, value, 0);
}

void jaudio::PositionalPlayback::updateFurnitureSwing(unsigned id, const std::array<float, 3>& position,
    const std::array<float, 2>& velocity, PositionalSound*& binding) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    if (!play(id, position, &binding)) return;
    const auto& constants = runtime->furnitureSound;
    double speed = std::abs(double(velocity[1]));
    if (speed < std::abs(double(velocity[0]))) speed = std::abs(double(velocity[0]));
    if (constants[0] < speed) speed = constants[0];
    speed = static_cast<float>(speed / double(constants[0]));
    if (speed <= constants[1]) binding->stop(1);
    else {
        const float pitch = static_cast<float>(double(constants[2]) +
            double(static_cast<float>(double(constants[2]) * speed)));
        if (constants[3] < std::abs(static_cast<float>(double(interactionPitch(*binding)) - pitch)))
            setInteractionPitch(*binding, pitch);
        setInteractionVolume(*binding, constants[4] * static_cast<float>(speed - double(constants[1])));
    }
}

jaudio::PositionalPlayback::Handle& jaudio::PositionalPlayback::owned(actoranimation::SoundHandle& sound) {
    for (auto& handle : handles) if (handle.get() == &sound && handle->active) return *handle;
    throw std::runtime_error("Animation sound parameter requires a live handle owned by its playback manager.");
}

actoranimation::SoundServices jaudio::PositionalPlayback::animationServices(const std::array<float, 3>& position) {
    return animationServicesAt(&position);
}

actoranimation::SoundServices jaudio::PositionalPlayback::animationServices() {
    return animationServicesAt(nullptr);
}

actoranimation::SoundServices jaudio::PositionalPlayback::animationServicesAt(const std::array<float, 3>* position) {
    return {
        [this, position](unsigned id, actoranimation::SoundHandle*& binding) {
            std::lock_guard<std::recursive_mutex> lock(owners.mutex);
            if (binding && (binding->flags() != id || owned(*binding).ownerPosition != position)) owned(*binding).stop(0);
            auto* sound = playAt(id, position, nullptr);
            if (!sound) { binding = nullptr; return; }
            auto& handle = owned(*sound);
            if (handle.animationBinding && handle.animationBinding != &binding) *handle.animationBinding = nullptr;
            handle.animationBinding = &binding;
            binding = &handle;
            if (!position) setPort(handle, 15, 0);
        },
        [this](actoranimation::SoundHandle& sound, float value) {
            std::lock_guard<std::recursive_mutex> lock(owners.mutex);
            owned(sound).record->parameters->controls.set(EffectControl::pitch, 5, value, 0);
        },
        [this](actoranimation::SoundHandle& sound, float value) {
            std::lock_guard<std::recursive_mutex> lock(owners.mutex);
            owned(sound).record->parameters->controls.set(EffectControl::volume, 5, value, 0);
        },
        [this](actoranimation::SoundHandle& sound, float value) {
            std::lock_guard<std::recursive_mutex> lock(owners.mutex);
            owned(sound).record->parameters->controls.set(EffectControl::pan, 5, value, 0);
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
    if (music) music->updateSequenceControls();
    listener = matrix;
    if (runtime->vacuumRunning) {
        auto& loop = *runtime;
        const auto set = [&](EffectControl control, unsigned slot, float value, unsigned frames) {
            if (loop.vacuumSound) owned(*loop.vacuumSound).record->parameters->controls.set(control, slot, value, frames);
        };
        if (loop.vacuumMode == 8) {
            play(0x22, *loop.vacuumPosition);
            loop.vacuumMode = 0;
            loop.vacuumEngaged = false;
        } else {
            if (loop.vacuumAccent && !--loop.vacuumAccent) set(EffectControl::pitch, 3, 1, 10);
            if (!loop.vacuumFade) {
                if (!loop.vacuumRequests) {
                    loop.vacuumEngaged = false;
                    loop.vacuumFade = 30;
                    set(EffectControl::pitch, 0, 0.4f, 30);
                    set(EffectControl::volume, 0, 0, 30);
                }
            } else if (!--loop.vacuumFade || !loop.vacuumSound) loop.vacuumRunning = false;
            if (loop.vacuumRunning) {
                play(0xe, *loop.vacuumPosition, &loop.vacuumSound);
                if (loop.vacuumMode) {
                    if (!loop.vacuumEngaged) {
                        set(EffectControl::pitch, 0, 2, 20);
                        set(EffectControl::volume, 0, 1, 10);
                        loop.vacuumEngaged = true;
                    }
                    if (loop.vacuumMode & 6) {
                        const bool held = (loop.vacuumMode & 4) != 0;
                        play(held ? 0x84b : 0x84a, *loop.vacuumPosition);
                        loop.vacuumAccent = 20;
                        set(EffectControl::pitch, 3, held ? 1.5f : 1.2f, 5);
                    }
                    loop.vacuumMode = 0;
                }
                set(EffectControl::pitch, 5, loop.vacuumPitch, 3);
            }
        }
        loop.vacuumRequests = 0;
    }

    owners.update();
    for (auto& handle : handles) if (handle->record) handle->record->position = handle->position;
    for (auto& category : runtime->categories) {
        std::uint16_t port = 0;
        if (!runtime->master.readEffectPort(category->firstChannel, 2, port)) continue;
        updateEffectCategory(category->pool, category->channels, *runtime);
        for (unsigned channel = 0; channel < category->channels.size(); ++channel) {
            auto* record = category->channels[channel];
            if (!record) continue;
            Runtime::Voice voice(*runtime, *record, category->firstChannel + channel);
            updateEffectVoice(*record, 1, voice);
        }
        for (auto* record = category->pool.active; record; record = record->next) {
            if (record->parameters) record->parameters->controls.advance(false);
        }
    }
    runtime->master.updateSequenceControls();
    for (auto& handle : handles) if (handle->active && !handle->record) remove(*handle);
}

bool jaudio::PositionalPlayback::mix(DspMixer& mixer, std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    if (music && !music->mix(mixer, error)) return false;
    return runtime->master.mix(mixer, error);
}

void jaudio::PositionalPlayback::bindMusic(gameplaymusic::Owner& owner) {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    music = &owner;
}

void jaudio::PositionalPlayback::stop() {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    if (music) music->stop();
    for (auto& handle : handles) handle->stop(0);
    runtime->master.stop();
    runtime->started = false;
    runtime->vacuumRunning = runtime->vacuumEngaged = false;
    runtime->vacuumRequests = runtime->vacuumMode = runtime->vacuumFade = runtime->vacuumAccent = 0;
    runtime->vacuumPosition = nullptr;
}

bool jaudio::PositionalPlayback::finished() const {
    std::lock_guard<std::recursive_mutex> lock(owners.mutex);
    if (music && music->currentSound()) return false;
    for (const auto& category : runtime->categories) if (category->pool.active) return false;
    return !runtime->master.hasVoices();
}

