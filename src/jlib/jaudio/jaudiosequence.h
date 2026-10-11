#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

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
        unsigned tempo = 48;
        unsigned timebase = 120;
        bool vibrato = false;
        unsigned oscillatorRoute = 0;
        std::array<int, 16> children{};
        std::array<Parameter, 18> parameters{};
        std::array<bool, 18> parameterWritten{};
        std::vector<std::size_t> returns;
        std::array<unsigned, 32> registers{};
        std::array<unsigned, 16> ports{};
        std::array<unsigned, 6> buses{0x150, 0x210, 0x352, 0x412, 0, 0};
        unsigned outputFlags = 0;
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

    private:
        unsigned take(Track& track, unsigned count);
        void update(unsigned index, const Listener& listener);
        void stop(unsigned index, const Listener& listener);
        std::vector<std::uint8_t> data;
        std::array<Track, 65> tracks;
        float phase = 0;
        std::uint64_t tickCount = 0;
        bool effectDispatch = false;
        bool dispatchReturned = false;
    };
}
