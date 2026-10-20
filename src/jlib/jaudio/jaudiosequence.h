#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <optional>

namespace jaudio {
    struct Parameter {
        float value = 1;
        float target = 1;
        float remaining = 0;
        float step = 0;
    };

    struct Track {
        bool active = false;
        std::size_t position = 0;
        int parent = -1;
        unsigned wait = 0;
        unsigned bank = 0;
        unsigned program = 240;
        unsigned bendRange = 12;
        std::uint8_t lastNote = 0;
        std::uint8_t transpose = 0, combinedTranspose = 0;
        unsigned tempo = 48;
        unsigned timebase = 120;
        bool vibrato = false;
        unsigned oscillatorRoute = 0;
        unsigned envelopeRoute = 15;
        std::optional<std::array<std::uint16_t, 5>> adsr;
        std::array<int, 16> children{};
        std::array<Parameter, 18> parameters{};
        std::array<bool, 18> parameterWritten{};
        std::vector<std::size_t> returns;
        std::array<std::uint16_t, 16> loopCounts{};
        std::array<unsigned, 32> registers{};
        std::array<unsigned, 16> ports{};
        std::array<bool, 16> importedPorts{};
        std::array<bool, 16> exportedPorts{};
        std::array<unsigned, 6> buses{0x150, 0x210, 0x352, 0x412, 0, 0};
        unsigned outputFlags = 0;
        unsigned route = 0;
        bool externalControl = false;
        bool effectControl = false;
        std::array<float, 5> effectParameters{1, 0.5f, 1, 0, 0};
        std::array<unsigned, 3> externalBlend{2, 2, 2}, parentBlend{2, 2, 2};
        float externalWeight = 1, parentWeight = 16384.0f / 32767;
        std::array<std::size_t, 8> interrupts{};
        unsigned enabledInterrupts = 0, pendingInterrupts = 0;
        bool interruptsBlocked = false;
        std::size_t interruptReturn = 0;
        unsigned interruptWait = 0;
        void requestInterrupt(unsigned index);
        void enterInterrupt();
        Track();
    };

    enum class EventType { noteOn, noteOff, finish, sync };

    struct SequenceEvent {
        EventType type;
        unsigned track = 0;
        unsigned slot = 0;
        unsigned key = 0;
        unsigned velocity = 0;
        unsigned duration = 0;
        int targetKey = -1;
    };

    class Interpreter {
    public:
        using Listener = std::function<void(const SequenceEvent&)>;
        void start(const std::vector<std::uint8_t>& sequence, std::size_t offset = 0, unsigned tempo = 48, unsigned timebase = 120);
        void startEffect(const std::vector<std::uint8_t>& sequence, std::size_t offset);
        bool advance(const Listener& listener, std::string& error);
        void release(const Listener& listener);
        bool finished() const;
        const Track& track(unsigned index) const;
        std::uint64_t ticks() const;
        void writePort(unsigned track, unsigned port, std::uint16_t value);
        void bindEffectParameters(unsigned track, const std::array<float, 5>& values);
        void updateEffectParameters(unsigned channel, const std::array<float, 5>& values, unsigned mask);
        void configureSequenceBlend(unsigned track, unsigned flags, bool specialPan);
        bool writeRoutePort(unsigned route, unsigned port, std::uint16_t value);
        bool readRoutePort(unsigned route, unsigned port, std::uint16_t& value) const;
        bool requestRouteInterrupt(unsigned route, unsigned interrupt);
        void setTempoScale(float value);
        std::function<std::uint32_t()> random;
        std::function<bool(unsigned, unsigned)> noteStopped;

    private:
        unsigned take(Track& track, unsigned count);
        unsigned readRegister(unsigned track, unsigned reg) const;
        void update(unsigned index, const Listener& listener);
        void stop(unsigned index, const Listener& listener);
        void close(unsigned index, const Listener& listener);
        std::vector<std::uint8_t> data;
        std::array<Track, 65> tracks;
        float phase = 0;
        float tempoScale = 1;
        std::uint64_t tickCount = 0;
        bool effectDispatch = false;
        bool dispatchReturned = false;
    };
}
