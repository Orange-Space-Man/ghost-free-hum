#include "jlib/jaudio/jaudioeffectvoice.h"
#include "jlib/jaudio/jaudioeffectadmission.h"
#include "jlib/jaudio/jaudioeffectselection.h"
#include "jlib/jaudio/jaudioeffectparameterpool.h"
#include "jlib/jaudio/jaudioeffectrelease.h"
#include "jlib/jaudio/jaudioplayback.h"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <memory>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    struct Admission : jaudio::EffectAdmissionServices {
        jaudio::EffectPool pool;
        std::array<jaudio::EffectRecord, 4> records{};
        unsigned id = 1, stops = 0, releases = 0;
        std::uint8_t priority = 10;
        bool exhausted = false;
        int actor = 0;
        jaudio::EffectParameterPool parameters{5};
        std::array<jaudio::EffectMetadata, 16> metadata{};
        std::unique_ptr<jaudio::Playback> playback = std::make_unique<jaudio::Playback>();
        std::vector<jaudio::EffectRecord*> channels{nullptr, nullptr};
        std::uint32_t duckMask = 0;
        std::vector<jaudio::EffectReleaseSequence> sequences;
        jaudio::EffectReleaseContext releaseContext{pool, parameters, channels, *playback, duckMask, sequences};
        Admission() {
            pool.free = records.data();
            for (unsigned i = 0; i + 1 < records.size(); ++i) records[i].next = &records[i + 1];
        }
        void stop(jaudio::EffectRecord& record) override {
            ++stops;
            auto* handle = &record;
            jaudio::stopEffect(&handle, 0, releaseContext);
        }
        void release(jaudio::EffectRecord& record) override {
            ++releases;
            jaudio::releaseEffect(record, releaseContext);
        }
        jaudio::EffectParameters* acquireParameters() override { return exhausted ? nullptr : parameters.acquire(); }
        void admit(jaudio::EffectRecord** binding, unsigned limit) {
            auto& info = metadata[id & 15];
            info.priority = priority;
            const jaudio::EffectActorSource source{&actor};
            const jaudio::EffectRequest request{id, 0, 0, &source, &info};
            jaudio::admitEffect(pool, binding, request, static_cast<std::uint8_t>(limit), *this);
        }
    };

    struct Category : jaudio::EffectCategoryServices {
        jaudio::EffectPool& pool;
        std::vector<jaudio::EffectRecord*>& channels;
        unsigned releases = 0, rankings = 0;
        Category(jaudio::EffectPool& pool, std::vector<jaudio::EffectRecord*>& channels) : pool(pool), channels(channels) {}
        jaudio::EffectRank prepareRank(jaudio::EffectRecord&) override { ++rankings; return {10, true}; }
        void release(jaudio::EffectRecord& record) override {
            ++releases;
            for (auto& channel : channels) if (channel == &record) channel = nullptr;
            record.state = 0;
            pool.recycle(record);
        }
    };

    struct Services : jaudio::EffectVoiceServices {
        std::vector<unsigned> calls;
        std::array<std::uint16_t, 16> output{}, published{};
        unsigned running = 1, observedState = 0, observedOffset = 0;
        float volume = -1;
        std::uint16_t readPort(unsigned port) override { require(port == 2, "Wrong completion port."); calls.push_back(1); return running; }
        void writePort(unsigned port, std::uint16_t value) override { calls.push_back(100 + port); output.at(port) = value; }
        void publishPort(unsigned port, std::uint16_t value) override { calls.push_back(200 + port); published.at(port) = value; }
        std::uint32_t random() override { calls.push_back(2); return 63; }
        void initializePosition() override { calls.push_back(3); }
        void updateParameters(jaudio::EffectControls& controls, unsigned state, std::uint8_t offset) override {
            calls.push_back(4); observedState = state; observedOffset = offset;
            controls.set(jaudio::EffectControl::volume, 4, 0.25f, state == 2 ? 0 : 3);
        }
        void initializeParameters(jaudio::EffectControls& controls) override {
            calls.push_back(5); controls.set(jaudio::EffectControl::volume, 5, 0.5f, 0);
        }
        void publishParameters(jaudio::EffectControls& controls, unsigned) override {
            calls.push_back(6); volume = controls.combined(jaudio::EffectControl::volume);
        }
        unsigned variant() override { calls.push_back(7); return 4; }
        unsigned distanceVariant() override { calls.push_back(8); return 9; }
        unsigned roomSize() override { calls.push_back(9); return 2; }
        void release() override { calls.push_back(10); }
    };

    struct OwnedServices : Services {
        Admission& owner;
        jaudio::EffectRecord& record;
        OwnedServices(Admission& owner, jaudio::EffectRecord& record) : owner(owner), record(record) {}
        void release() override { Services::release(); owner.release(record); }
    };
}

int main() {
    try {
        jaudio::EffectMetadata requestMetadata{0x1848, 17};
        std::array<float, 3> requestPosition{1, 2, 3}, secondarySource{4, 5, 6};
        int requestActor = 0;
        jaudio::EffectActorSource requestSource{&requestActor, &requestPosition, &secondarySource, 0x12345678};
        jaudio::EffectRequest request{0x828, 27, 4, &requestSource, &requestMetadata};
        jaudio::EffectRecord initialized, sentinel;
        jaudio::EffectParameters retainedParameters;
        initialized.parameters = &retainedParameters;
        initialized.previous = &sentinel;
        initialized.next = &sentinel;
        initialized.state = 3;
        initialized.pitchOffset = 19;
        jaudio::EffectRecord* requestBinding = nullptr;
        jaudio::initializeEffectRequest(initialized, &requestBinding, request);
        require(requestBinding == &initialized && initialized.binding == &requestBinding && initialized.soundId == 0x828 &&
            initialized.actor == &requestActor && initialized.position == &requestPosition &&
            initialized.secondarySource == &secondarySource && initialized.context == 0x12345678 &&
            initialized.stopFrames == 27 && initialized.listener == 4 && initialized.lifetime == 10 &&
            initialized.metadataSource == &requestMetadata && initialized.metadataFlags() == 0x1848 && initialized.priority() == 17,
            "Request initializer omitted original source, binding, metadata or timing fields.");
        require(initialized.parameters == &retainedParameters && initialized.previous == &sentinel && initialized.next == &sentinel &&
            initialized.state == 3 && initialized.pitchOffset == 19,
            "Request initialization reset fields owned by allocation or voice processing.");
        requestPosition[0] = 99;
        requestMetadata.flags = 8; requestMetadata.priority = 23;
        require((*initialized.position)[0] == 99 && initialized.metadataFlags() == 8 && initialized.priority() == 23,
            "Request source and metadata must remain borrowed, not copied snapshots.");
        requestSource.actor = nullptr;
        jaudio::initializeEffectRequest(initialized, nullptr, request);
        require(!initialized.actor && !initialized.position && !initialized.secondarySource && initialized.context == 0 &&
            !initialized.binding && requestBinding == &initialized,
            "Null actor must clear source fields without clearing a previous external binding.");
        request.source = nullptr;
        request.listener = 255; request.stopFrames = 0xffffffff;
        jaudio::initializeEffectRequest(initialized, &requestBinding, request);
        require(!initialized.actor && !initialized.position && !initialized.secondarySource && !initialized.context &&
            initialized.listener == 255 && initialized.stopFrames == 0xffffffff && initialized.lifetime == 10,
            "Source-free request must preserve byte and frame values without normalization.");

        Admission lifecycle;
        lifecycle.id = 0x828;
        jaudio::EffectRecord* retained = nullptr;
        lifecycle.admit(&retained, 3);
        auto* releasedRecord = retained;
        auto* releasedBank = retained->parameters;
        lifecycle.channels[0] = retained;
        jaudio::stopEffect(&retained, 30, lifecycle.releaseContext);
        require(!retained && releasedRecord->state == 0 && !lifecycle.channels[0] &&
            lifecycle.pool.free == releasedRecord && !releasedRecord->parameters,
            "Unstarted sound must release immediately even with a fade duration.");
        lifecycle.admit(&retained, 3);
        require(retained == releasedRecord && retained->parameters == releasedBank, "Release did not recycle both owned objects.");
        retained->state = 4;
        lifecycle.channels[0] = retained;
        auto* localHandle = retained;
        jaudio::stopEffect(&localHandle, 3, lifecycle.releaseContext);
        require(!localHandle && retained == releasedRecord && retained->state == 4 && retained->stopFrames == 3 &&
            retained->parameters->controls.parameter(jaudio::EffectControl::volume, 6).remaining == 3,
            "Fade stop must clear its supplied handle while preserving the retained actor binding and pool ownership.");
        retained->parameters->controls.advance(false);
        retained->parameters->controls.advance(false);
        retained->parameters->controls.advance(false);
        jaudio::SequenceVolume restored, excluded;
        jaudio::EffectMetadata ordinary{}, ducking{8, 0};
        lifecycle.sequences = {{&ordinary, &excluded}, {&ordinary, &restored}, {&ducking, &excluded}};
        lifecycle.metadata[lifecycle.id & 15].flags = 8;
        lifecycle.duckMask = 1;
        OwnedServices completed(lifecycle, *retained);
        jaudio::updateEffectVoice(*retained, 1, completed);
        require(completed.calls == std::vector<unsigned>{1, 10}, "Fade completion did not release before further parameter publication.");
        require(!retained && restored.target == 127 && restored.remaining == 3 && excluded.remaining == 0 &&
            !lifecycle.duckMask && !lifecycle.channels[0] && lifecycle.pool.free == releasedRecord,
            "Completion release failed to clear actor ownership, restore eligible music or recycle the sound.");
        lifecycle.admit(&retained, 3);
        lifecycle.duckMask = 1;
        lifecycle.sequences.clear();
        lifecycle.release(*retained);
        require(lifecycle.duckMask == 1, "Original duck mask must remain set when no eligible sequence visits its branch.");
        lifecycle.admit(&retained, 3);
        retained->state = 4;
        lifecycle.channels[0] = retained;
        OwnedServices ended(lifecycle, *retained);
        ended.running = 0;
        jaudio::updateEffectVoice(*retained, 1, ended);
        require(!retained && !lifecycle.channels[0] && ended.calls == std::vector<unsigned>{1, 10},
            "Natural sequence completion failed to release retained actor ownership.");
        bool duplicateReleaseRejected = false;
        try { jaudio::releaseEffect(*releasedRecord, lifecycle.releaseContext); }
        catch (const std::runtime_error&) { duplicateReleaseRejected = true; }
        require(duplicateReleaseRejected, "Repeated native release must not corrupt the original pool links.");
        jaudio::stopEffect(nullptr, 0, lifecycle.releaseContext);
        jaudio::stopEffect(&retained, 0, lifecycle.releaseContext);

        jaudio::EffectParameterPool parameterPool(4);
        auto* oldest = parameterPool.acquire();
        auto* middle = parameterPool.acquire();
        auto* newest = parameterPool.acquire();
        require(oldest && middle && newest && newest->next == middle && middle->next == oldest,
            "Parameter pool did not keep independent active ownership.");
        middle->controls.set(jaudio::EffectControl::volume, 4, 0, 3);
        middle->ports[7] = 123;
        middle->pendingPorts = 0x80;
        float boundVolume = 0.25f;
        middle->controls.bind(jaudio::EffectControl::volume, &boundVolume);
        parameterPool.release(*middle);
        require(newest->next == oldest && oldest->previous == newest, "Middle parameter release broke active links.");
        auto* reused = parameterPool.acquire();
        require(reused == middle && reused->controls.combined(jaudio::EffectControl::volume) == 1 &&
            reused->controls.parameter(jaudio::EffectControl::volume, 4).remaining == 0 &&
            reused->ports[7] == 123 && reused->pendingPorts == 0,
            "Parameter reuse must reset controls and pending bits while retaining original port storage.");
        parameterPool.release(*oldest);
        parameterPool.release(*reused);
        parameterPool.release(*newest);
        jaudio::EffectParameterPool exhaustedPool(1);
        auto* finalBlock = exhaustedPool.acquire();
        require(finalBlock && !exhaustedPool.acquire(), "Parameter pool must permit its final block, unlike the sound pool.");
        bool rejectedRelease = false;
        try { exhaustedPool.release(*finalBlock); } catch (const std::runtime_error&) { rejectedRelease = true; }
        require(rejectedRelease, "Exhausted original free-list release must not be silently repaired.");
        const std::array<float, 12> identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        auto cancellation = identity;
        cancellation[1] = 1; cancellation[2] = 1; cancellation[3] = -16777216;
        require(jaudio::transformEffectPosition(cancellation, {16777216, 1, 1})[0] == 1,
            "Listener transform lost the original paired-single arithmetic grouping.");
        std::vector<std::array<float, 12>> matrices{identity, identity};
        matrices[1][3] = 10;
        std::array<float, 3> position{3, 4, 0};
        std::vector<jaudio::EffectListenerDistance> prepared(2);
        prepared[0].x = 99;
        auto preparedRank = jaudio::prepareEffectRank(prepared, matrices, &position, 1, 2, 255, 0, 100);
        require(prepared[0].x == 99 && prepared[1].x == 13 && preparedRank.score == 185,
            "Single-listener preparation touched another listener or omitted translation.");
        position = {1, 2, 3};
        jaudio::prepareEffectRank(prepared, matrices, &position, 4, 2, 255, 0, 100);
        require(prepared[0].previous[0] == 99 && prepared[1].previous == std::array<float, 3>{13, 4, 0} && prepared[1].x == 11,
            "Listener history was not retained before position preparation.");
        preparedRank = jaudio::prepareEffectRank(prepared, {}, nullptr, 4, 2, 255, 0, 100);
        require(preparedRank.score == 0 && prepared[0].x == 0 && prepared[1].x == 0 && prepared[1].previous[0] == 11,
            "Unpositioned sound must use the original origin without applying listener matrices.");
        std::vector<jaudio::EffectListenerDistance> listeners{{3, 4, 0}, {0, 0, 10}};
        auto rank = jaudio::rankEffect(listeners, 0, 2, 255, 0x20, 5);
        require(rank.score == 25 && rank.eligible && listeners[0].squared == 25, "Distance cutoff must include equality.");
        rank = jaudio::rankEffect(listeners, 1, 2, 254, 0x20, 5);
        require(rank.score == 100 + 5776 + 60 && !rank.eligible, "Priority or positive-depth penalty changed.");
        rank = jaudio::rankEffect(listeners, 4, 2, 255, 0x20, 5);
        require(rank.score == 80 && rank.eligible, "Multi-listener scoring must retain last score and nearest eligibility.");
        listeners[0] = {50000, 0, 0};
        rank = jaudio::rankEffect(listeners, 0, 1, 255, 0, 5);
        require(rank.score == 1879048192u && rank.eligible, "Original large-distance substitution changed.");
        listeners[0] = {0, 0, -10};
        require(jaudio::rankEffect(listeners, 0, 1, 255, 0, 1).score == 100, "Negative depth must not add a penalty.");
        Admission categoryPool;
        jaudio::EffectRecord* expired = nullptr;
        categoryPool.admit(&expired, 3);
        expired->soundId = 0x828; expired->lifetime = 1;
        categoryPool.id = 2;
        jaudio::EffectRecord* pending = nullptr;
        categoryPool.admit(&pending, 3);
        pending->soundId = 0x829; pending->lifetime = 0;
        std::vector<jaudio::EffectRecord*> categoryChannels{nullptr};
        Category category(categoryPool.pool, categoryChannels);
        jaudio::updateEffectCategory(categoryPool.pool, categoryChannels, category);
        require(category.releases == 1 && category.rankings == 1 && pending->lifetime == 255 &&
            categoryChannels[0] == pending && pending->state == 2,
            "Category traversal lost byte underflow, release-safe iteration, or selector connection.");
        jaudio::EffectRecord a, b, c;
        a.state = 4; a.soundId = 0x828;
        b.state = 1;
        c.state = 1;
        jaudio::EffectSelection selection(2);
        selection.consider(a, 20);
        selection.consider(b, 10);
        std::vector<jaudio::EffectRecord*> channels{&a, nullptr};
        unsigned released = 0;
        auto release = [&](jaudio::EffectRecord& record) { ++released; record.state = 0; };
        selection.assign(channels, release);
        require(channels[0] == &a && channels[1] == &b && a.state == 3 && b.state == 2 && released == 0,
            "Selected continuing sound must retain its channel while a new sound starts.");
        a.state = 4;
        jaudio::EffectSelection replacement(2);
        replacement.consider(c, 0);
        replacement.assign(channels, release);
        require(released == 1 && channels[0] == &c && channels[1] == &b, "Unselected continuing sound failed to release its channel.");
        a.state = 4; a.soundId = 1; a.lifetime = 10;
        channels = {&a};
        jaudio::EffectSelection empty(1);
        empty.assign(channels, release);
        require(!channels[0] && a.state == 1 && a.lifetime == 0 && released == 1,
            "Unselected one-shot must expire without the continuing-sound release path.");
        a.state = 1; b.state = 1;
        channels = {nullptr, &a};
        jaudio::EffectSelection duplicate(2);
        duplicate.consider(b, 20);
        duplicate.consider(a, 10);
        duplicate.assign(channels, release);
        require(!channels[0] && channels[1] == &a && b.state == 2,
            "Duplicate candidate must retain the original vacant-channel suppression.");
        a.state = 4; b.state = 1; c.state = 5;
        channels = {nullptr, nullptr, nullptr};
        jaudio::EffectSelection shifted(3);
        shifted.consider(a, 30);
        shifted.consider(b, 10);
        shifted.consider(c, 20);
        shifted.assign(channels, release);
        require(channels[0] == &b && channels[1] == &c && !channels[2] && a.state == 3,
            "Candidate state-byte shift quirk was replaced with ordinary sorting.");
        a.state = 1; b.state = 1;
        channels = {nullptr};
        jaudio::EffectSelection tied(1);
        tied.consider(a, 10); tied.consider(b, 10);
        tied.assign(channels, release);
        require(channels[0] == &b && a.state == 1, "Later equal-score candidate must win.");
        Admission admission;
        jaudio::EffectRecord* first = nullptr;
        admission.admit(&first, 2);
        require(first == &admission.records[0] && first->state == 1 && first->parameters,
            "Admission failed to acquire both sound and parameter ownership.");
        first->state = 5;
        admission.admit(&first, 2);
        require(first->state == 4 && admission.stops == 0, "Matching active effect must refresh instead of restart.");
        admission.id = 2;
        jaudio::EffectRecord* second = nullptr;
        admission.admit(&second, 2);
        require(admission.pool.active == second && second->next == first && first->previous == second,
            "Active effect list insertion changed.");
        first->state = 5;
        admission.id = 3;
        jaudio::EffectRecord* third = nullptr;
        admission.admit(&third, 2);
        require(!third && admission.releases == 0, "Equal-priority state-five sound must reject replacement.");
        admission.priority = 11;
        admission.admit(&third, 2);
        require(third && !first && admission.releases == 1 && admission.pool.active == third && third->next == second,
            "Replacement must release and reuse the original weakest candidate.");
        admission.id = 4;
        jaudio::EffectRecord* fourth = nullptr;
        admission.admit(&fourth, 3);
        admission.id = 5;
        jaudio::EffectRecord* fifth = nullptr;
        admission.admit(&fifth, 4);
        require(fourth && !fifth && admission.pool.free == &admission.records[3], "Final free pool entry must remain reserved.");
        admission.release(*second);
        require(!second && admission.pool.free->next == &admission.records[3], "Tail release failed to preserve the free chain.");
        admission.exhausted = true;
        admission.admit(&fifth, 4);
        require(!fifth && !admission.pool.active->parameters, "Parameter exhaustion must retain the acquired record's original ownership.");

        Admission ownedVoice;
        ownedVoice.id = 0x828;
        jaudio::EffectRecord* admitted = nullptr;
        ownedVoice.admit(&admitted, 3);
        auto* admittedParameters = admitted->parameters;
        admittedParameters->ports[7] = 42;
        admittedParameters->pendingPorts = 1u << 7;
        jaudio::EffectSelection ownedSelection(1);
        ownedSelection.consider(*admitted, 0);
        std::vector<jaudio::EffectRecord*> ownedChannels{nullptr};
        ownedSelection.assign(ownedChannels, [&](jaudio::EffectRecord& record) { ownedVoice.release(record); });
        Services ownedServices;
        jaudio::updateEffectVoice(*ownedChannels[0], 1, ownedServices);
        require(ownedChannels[0] == admitted && admitted->state == 4 && admitted->parameters == admittedParameters &&
            admittedParameters->pendingPorts == 0 && ownedServices.published[7] == 42 &&
            admittedParameters->controls.value(jaudio::EffectControl::volume, 4) == 0.25f && ownedServices.volume == 0.125f,
            "Assigned voice must update the admitted record and its owned parameter bank directly.");
        ownedVoice.release(*admitted);
        ownedVoice.admit(&admitted, 3);
        require(admitted->parameters == admittedParameters && admittedParameters->controls.combined(jaudio::EffectControl::volume) == 1,
            "Voice release and readmission must reuse and reset the same owned bank.");
        jaudio::EffectRecord unbound;
        bool rejectedUnbound = false;
        try { jaudio::updateEffectVoice(unbound, 1, ownedServices); } catch (const std::runtime_error&) { rejectedUnbound = true; }
        require(rejectedUnbound, "Unbound voice must not create a substitute parameter bank.");

        jaudio::EffectMetadata voiceMetadata;
        jaudio::EffectParameters voiceParameters;
        jaudio::EffectRecord voice;
        voice.parameters = &voiceParameters; voice.metadataSource = &voiceMetadata; voice.state = 2;
        unsigned listenerCount = 1;
        Services services;
        voice.soundId = 0x828;
        voiceMetadata.flags = 8 | 0x40 | 0x800 | 0x1000;
        voice.parameters->pendingPorts = (1u << 1) | (1u << 15);
        voice.parameters->ports[1] = 123; voice.parameters->ports[15] = 0xffff;
        jaudio::updateEffectVoice(voice, listenerCount, services);
        require(services.calls == std::vector<unsigned>{1, 3, 2, 201, 215, 4, 5, 6, 7, 8, 103, 9, 106, 104, 100},
            "Original assigned-sound startup order changed.");
        require(voice.state == 4 && services.observedState == 2 && services.observedOffset == 15 && voice.parameters->pendingPorts == 0,
            "Sound startup did not retain pitch offset or clear published ports.");
        require(services.output[4] == 44 && services.output[3] == 9 && services.output[6] == 2 && services.output[0] == 1 &&
            services.published[15] == 0xffff && services.volume == 0.125f, "Sound startup publication changed.");
        services.calls.clear();
        jaudio::updateEffectVoice(voice, listenerCount, services);
        require(services.calls == std::vector<unsigned>{1}, "Stable state four must not refresh targets unconditionally.");
        voice.state = 3;
        services.calls.clear();
        jaudio::updateEffectVoice(voice, listenerCount, services);
        require(services.calls == std::vector<unsigned>{1, 4, 6} && voice.state == 4, "State three refresh path changed.");
        voice.stopFrames = 3;
        voice.parameters->controls.set(jaudio::EffectControl::volume, 6, 0, 3);
        voice.parameters->controls.advance(false);
        services.calls.clear();
        jaudio::updateEffectVoice(voice, listenerCount, services);
        require(services.calls == std::vector<unsigned>{1, 4, 6}, "Pending fade released before slot six reached zero.");
        voice.parameters->controls.advance(false); voice.parameters->controls.advance(false);
        services.calls.clear();
        jaudio::updateEffectVoice(voice, listenerCount, services);
        require(services.calls == std::vector<unsigned>{1, 10}, "Completed fade failed to release through its owner.");
        voice = {}; voice.parameters = &voiceParameters; voice.metadataSource = &voiceMetadata; voice.state = 2; voiceParameters.controls.reset(); voiceParameters.pendingPorts = 0;
        services = {};
        voice.stopFrames = 4;
        jaudio::updateEffectVoice(voice, listenerCount, services);
        require(voice.state == 5 && voice.stopFrames == 0 && services.volume == 0 &&
            voice.parameters->controls.parameter(jaudio::EffectControl::volume, 6).target == 127,
            "Original startup fade target or publication order changed.");
        voice.parameters->controls.advance(false);
        require(voice.parameters->controls.value(jaudio::EffectControl::volume, 6) == 31.75f, "Startup fade was normalized instead of preserved.");
        services.running = 0; services.calls.clear();
        jaudio::updateEffectVoice(voice, listenerCount, services);
        require(services.calls == std::vector<unsigned>{1, 10}, "Sequence completion must release before updating parameters.");
        for (unsigned flags : {0u, 0x40u, 0x80u, 0xc0u}) {
            voice = {}; voice.parameters = &voiceParameters; voice.metadataSource = &voiceMetadata; voice.state = 2; voiceParameters.controls.reset(); voiceParameters.pendingPorts = 0; services = {};
            voiceMetadata.flags = flags | 0x1000;
            listenerCount = 2;
            jaudio::updateEffectVoice(voice, listenerCount, services);
            require(voice.pitchOffset == (flags == 0 ? 0 : flags == 0x40 ? 15 : flags == 0x80 ? 31 : 63) && services.output[3] == 0,
                "Pitch mask or multiple-listener distance gating changed.");
        }
        std::cout << "Effect voice lifecycle passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

