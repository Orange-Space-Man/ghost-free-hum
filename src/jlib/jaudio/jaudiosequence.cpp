#include "jlib/jaudio/jaudiosequence.h"

#include <algorithm>
#include <stdexcept>

jaudio::Track::Track() {
    children.fill(-1);
    for (unsigned index : {1U, 2U, 4U, 5U, 13U, 14U, 15U}) {
        parameters[index].value = parameters[index].target = 0;
    }
    parameters[3].value = parameters[3].target = 0.5f;
}

void jaudio::Track::requestInterrupt(unsigned index) {
    pendingInterrupts |= enabledInterrupts & (1u << index);
}

void jaudio::Track::enterInterrupt() {
    if (interruptsBlocked) return;
    for (unsigned i = 0; i < interrupts.size(); ++i) {
        const unsigned bit = 1u << i;
        if (!(enabledInterrupts & pendingInterrupts & bit)) continue;
        interruptReturn = position;
        interruptWait = wait;
        position = interrupts[i];
        wait = 0;
        interruptsBlocked = true;
        pendingInterrupts ^= bit;
        return;
    }
}

void jaudio::Interpreter::start(const std::vector<std::uint8_t>& sequence, std::size_t offset, unsigned tempo, unsigned timebase) {
    data = sequence;
    tracks = {};
    tracks[0].active = !data.empty();
    tracks[0].position = offset;
    tracks[0].tempo = tempo;
    tracks[0].timebase = timebase;
    phase = 0;
    tempoScale = 1;
    tickCount = 0;
    effectDispatch = false;
    dispatchReturned = false;
}

void jaudio::Interpreter::startEffect(const std::vector<std::uint8_t>& sequence, std::size_t offset) {
    start(sequence, offset, 120, 48);
    effectDispatch = true;
}

unsigned jaudio::Interpreter::take(Track& current, unsigned count) {
    if (current.position > data.size() || count > data.size() - current.position) {
        throw std::runtime_error("Truncated JAudio sequence at " + std::to_string(current.position));
    }
    unsigned value = 0;
    while (count-- != 0) value = (value << 8) | data[current.position++];
    return value;
}

void jaudio::Interpreter::stop(unsigned index, const Listener& listener) {
    auto& current = tracks[index];
    if (!current.active) return;
    current.active = false;
    listener({EventType::finish, index});
    for (const auto child : current.children) if (child >= 0) stop(child, listener);
}

void jaudio::Interpreter::update(unsigned index, const Listener& listener) {
    auto& current = tracks[index];
    if (!current.active) return;
    if (index == 0 && effectDispatch && dispatchReturned) {
        for (const auto child : current.children) if (child >= 0) update(child, listener);
        return;
    }
    current.requestInterrupt(7);
    current.enterInterrupt();
    if (current.wait != 0) --current.wait;
    if (current.wait == 0) {
        bool yield = false;
        for (unsigned commands = 0; !yield; ++commands) {
            if (commands == 4096) throw std::runtime_error("JAudio sequence did not yield.");
            const auto position = current.position;
            const auto opcode = take(current, 1);
            if (opcode < 0x80) {
                const auto slot = take(current, 1);
                const auto velocity = take(current, 1);
                if (velocity >= 128 || slot == 0 || (slot > 8 && slot != 16)) throw std::runtime_error("Unported JAudio note mode.");
                if (slot == 8 || slot == 16) {
                    if (take(current, 1) != 100) throw std::runtime_error("Unported JAudio note gate.");
                    current.wait = take(current, slot >> 3);
                    if (current.wait == 0 || (slot == 8 && current.wait >= 128)) throw std::runtime_error("Unported JAudio note duration.");
                    const auto clock = (static_cast<float>(tracks[0].tempo * tracks[0].timebase) / ((60.0f * 32028.5f) / 80.0f)) * tempoScale;
                    listener({EventType::noteOn, index, 0, opcode, velocity, static_cast<unsigned>(current.wait / clock)});
                    yield = true;
                } else {
                    listener({EventType::noteOn, index, slot, opcode, velocity});
                }
            } else if (opcode == 0x80 || opcode == 0x88) {
                current.wait = take(current, opcode == 0x80 ? 1 : 2);
                yield = true;
            } else if (opcode >= 0x81 && opcode <= 0x87) {
                listener({EventType::noteOff, index, opcode & 7});
            } else if (opcode == 0x98 || opcode == 0x9a || opcode == 0x9c || opcode == 0x9e) {
                const auto parameter = take(current, 1);
                const auto value = take(current, opcode <= 0x9a ? 1 : 2) << (opcode <= 0x9a ? 8 : 0);
                if (parameter >= current.parameters.size()) throw std::runtime_error("Invalid JAudio parameter.");
                auto& target = current.parameters[parameter];
                current.parameterWritten[parameter] = true;
                target.target = static_cast<std::int16_t>(value) / 32768.0f;
                if (opcode == 0x9a || opcode == 0x9e) target.remaining = static_cast<float>(take(current, 1));
                if (target.remaining > 0) {
                    target.step = (target.target - target.value) / target.remaining;
                } else {
                    target.value = target.target;
                    target.step = 0;
                    target.remaining = 1;
                }
            } else if (opcode == 0xa4 || opcode == 0xac || opcode == 0xad || opcode == 0xaf) {
                const auto reg = take(current, 1);
                auto value = take(current, opcode == 0xa4 ? 1 : 2);
                if (opcode == 0xad || opcode == 0xaf) {
                    if (reg >= current.registers.size()) throw std::runtime_error("Unported JAudio arithmetic register.");
                    if (opcode == 0xaf) {
                        current.registers[3] = static_cast<std::uint16_t>(current.registers[reg] - value);
                        continue;
                    }
                    value = static_cast<std::uint16_t>(current.registers[reg] + value);
                }
                if (reg < 3) current.registers[reg] = value & 255;
                else if (reg == 3) current.registers[reg] = value;
                else if (reg == 6) { current.bank = value >> 8; current.program = value & 255; }
                else if (reg == 32) current.bank = value;
                else if (reg == 33) current.program = value;
                else if (reg == 7) current.bendRange = value;
                else if ((reg >= 8 && reg <= 12) || reg == 0x2e) {
                    current.registers[reg == 0x2e ? 13 : reg] = value;
                }
                else throw std::runtime_error("Unported JAudio register write.");
                current.registers[3] = reg < 3 ? static_cast<std::uint16_t>(static_cast<std::int8_t>(value)) : value;
                if (reg == 6 || reg == 32 || reg == 33) {
                    current.vibrato = false;
                    current.oscillatorRoute = 0;
                }
            } else if (opcode == 0xc1) {
                const auto slot = take(current, 1);
                const auto offset = take(current, 3);
                if (slot >= 16 || offset >= data.size()) throw std::runtime_error("Invalid JAudio child track.");
                if (current.children[slot] >= 0) stop(current.children[slot], listener);
                const auto found = current.children[slot] >= 0 ? tracks.begin() + current.children[slot] :
                    std::find_if(tracks.begin() + 1, tracks.end(), [](const auto& entry) { return entry.parent == -1; });
                if (found == tracks.end()) throw std::runtime_error("JAudio track capacity exceeded.");
                const auto child = static_cast<int>(found - tracks.begin());
                *found = Track{};
                found->active = true;
                found->parent = static_cast<int>(index);
                found->route = ((current.route & 0x00ffffff) << 4) | slot | ((current.route & 0xf0000000) + 0x10000000);
                found->position = offset;
                found->bank = current.bank;
                found->program = current.program;
                found->bendRange = current.bendRange;
                found->timebase = current.timebase;
                found->tempo = current.tempo;
                current.children[slot] = child;
            } else if (opcode == 0xc4) {
                if (take(current, 1) != 0) throw std::runtime_error("Unported JAudio call condition.");
                const auto offset = take(current, 3);
                if (offset >= data.size() || current.returns.size() >= 16) throw std::runtime_error("Invalid JAudio call.");
                current.returns.push_back(current.position);
                current.position = offset;
            } else if (opcode == 0xc6) {
                if (take(current, 1) != 0) throw std::runtime_error("Invalid JAudio return.");
                if (current.returns.empty()) {
                    if (!effectDispatch || index != 0) throw std::runtime_error("Invalid JAudio return.");
                    dispatchReturned = true;
                    yield = true;
                } else {
                    current.position = current.returns.back();
                    current.returns.pop_back();
                }
            } else if (opcode == 0xc8) {
                const auto condition = take(current, 1);
                const auto offset = take(current, 3);
                const auto value = current.registers[3];
                if (condition > 5 || offset >= data.size()) throw std::runtime_error("Unported JAudio jump.");
                if (condition == 0 || (condition == 1 && value == 0) || (condition == 2 && value != 0) ||
                    (condition == 3 && value == 1) || (condition == 4 && value >= 0x8000) || (condition == 5 && value < 0x8000)) {
                    current.position = offset;
                }
            } else if (opcode == 0xcb) {
                const auto port = take(current, 1);
                const auto reg = take(current, 1);
                if (port >= current.ports.size() || reg > 2) throw std::runtime_error("Unported JAudio port read.");
                current.importedPorts[port] = false;
                current.registers[reg] = current.ports[port] & 255;
                current.registers[3] = static_cast<std::uint16_t>(static_cast<std::int8_t>(current.ports[port]));
            } else if (opcode == 0xcc || opcode == 0xd2) {
                const auto destination = take(current, 1);
                const auto source = take(current, 1);
                unsigned value;
                if (source >= 0x40 && source < 0x50) value = current.ports[source - 0x40];
                else if (source < current.registers.size()) value = current.registers[source];
                else throw std::runtime_error("Unported JAudio child-port source register.");
                const int child = current.children[destination >> 4];
                if (opcode == 0xcc) {
                    if (destination >= current.ports.size()) throw std::runtime_error("Invalid JAudio port write.");
                    current.ports[destination] = value & 0xffff;
                    current.exportedPorts[destination] = true;
                } else if (child >= 0) {
                    tracks[child].ports[destination & 15] = value & 0xffff;
                    tracks[child].importedPorts[destination & 15] = true;
                    if ((destination & 15) < 2) tracks[child].requestInterrupt(3 + (destination & 15));
                }
            } else if (opcode == 0xdf) {
                const auto interrupt = take(current, 1);
                const auto offset = take(current, 3);
                if (interrupt >= current.interrupts.size() || offset >= data.size()) throw std::runtime_error("Invalid JAudio interrupt.");
                current.enabledInterrupts |= 1u << interrupt;
                current.interrupts[interrupt] = offset;
            } else if (opcode == 0xe0) {
                const auto interrupt = take(current, 1);
                if (interrupt >= current.interrupts.size()) throw std::runtime_error("Invalid JAudio interrupt disable.");
                current.enabledInterrupts &= ~(1u << interrupt);
            } else if (opcode == 0xe1 || opcode == 0xe2) {
                current.interruptsBlocked = opcode == 0xe2;
            } else if (opcode == 0xe3) {
                current.position = current.interruptReturn;
                current.wait = current.interruptWait;
                current.interruptsBlocked = false;
                current.enterInterrupt();
                if (current.wait) --current.wait;
                yield = current.wait != 0;
            } else if (opcode == 0xdb) {
                current.outputFlags = take(current, 1);
                if (current.outputFlags != 0) throw std::runtime_error("Unported JAudio output routing.");
            } else if (opcode == 0xdd) {
                const auto bus = take(current, 1);
                const auto routing = take(current, 2);
                if (bus >= current.buses.size()) throw std::runtime_error("Invalid JAudio bus.");
                current.buses[bus] = routing;
            } else if (opcode == 0xd6) {
                if (take(current, 1) != 0) throw std::runtime_error("Unported JAudio simple oscillator.");
                current.vibrato = true;
            } else if (opcode == 0xe7) {
                const auto sync = take(current, 2);
                if (sync == 4) current.ports[15] = current.parent >= 0 ? tracks[current.parent].ports[7] : 0;
                else if (sync != 0 && sync != 127) throw std::runtime_error("Unported JAudio sync callback: " + std::to_string(sync));
                if (sync == 0 || sync == 127) current.externalControl = true;
                listener({EventType::sync, index, sync});
            } else if (opcode == 0xf0) {
                current.oscillatorRoute = take(current, 1);
                if (current.oscillatorRoute != 0x11) throw std::runtime_error("Unported JAudio oscillator routing.");
            } else if (opcode == 0xfd || opcode == 0xfe) {
                const auto value = take(current, 2);
                if (index != 0 || value == 0) throw std::runtime_error("Unported JAudio track clock.");
                if (opcode == 0xfd) current.tempo = value;
                else current.timebase = value;
            } else if (opcode == 0xff) {
                stop(index, listener);
                yield = true;
            } else {
                throw std::runtime_error("Unported JAudio opcode " + std::to_string(opcode) + " at " + std::to_string(position));
            }
        }
    }
    for (auto& parameter : current.parameters) {
        if (parameter.remaining > 0) {
            parameter.value += parameter.step;
            --parameter.remaining;
        }
    }
    for (const auto child : current.children) if (child >= 0) update(child, listener);
}

bool jaudio::Interpreter::advance(const Listener& listener, std::string& error) {
    error.clear();
    if (finished()) return true;
    try {
        phase += (static_cast<float>(tracks[0].tempo * tracks[0].timebase) / ((60.0f * 32028.5f) / 80.0f)) * tempoScale;
        unsigned iterations = 0;
        while (phase >= 1 && !finished()) {
            if (++iterations > 4096) throw std::runtime_error("JAudio clock exceeds the supported tick rate.");
            phase -= 1;
            ++tickCount;
            update(0, listener);
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        tracks[0].active = false;
        return false;
    }
}

bool jaudio::Interpreter::finished() const {
    if (!tracks[0].active) return true;
    if (effectDispatch && dispatchReturned) {
        return std::none_of(tracks.begin() + 1, tracks.end(), [](const auto& track) { return track.active; });
    }
    return !tracks[0].active;
}
void jaudio::Interpreter::release(const Listener& listener) { stop(0, listener); }
const jaudio::Track& jaudio::Interpreter::track(unsigned index) const { return tracks.at(index); }
std::uint64_t jaudio::Interpreter::ticks() const { return tickCount; }

void jaudio::Interpreter::setTempoScale(float value) { tempoScale = value; }

void jaudio::Interpreter::writePort(unsigned track, unsigned port, std::uint16_t value) {
    auto& current = tracks.at(track);
    current.ports.at(port) = value;
    current.importedPorts.at(port) = true;
    if (port < 2) current.requestInterrupt(port + 3);
}
