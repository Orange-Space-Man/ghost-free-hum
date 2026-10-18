#include "jlib/jaudio/jaudiosequence.h"
#include "jlib/jaudio/jaudiosequencerandom.h"

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

unsigned jaudio::Interpreter::readRegister(unsigned index, unsigned reg) const {
    const auto& current = tracks.at(index);
    if (reg == 6) return (current.bank << 8) | current.program;
    if (reg == 7) return current.bendRange;
    if (reg < current.registers.size()) return current.registers[reg];
    if (reg == 0x20) return current.bank;
    if (reg == 0x21) return current.program;
    if (reg == 0x22) return (current.registers[0] << 8) | current.registers[1];
    if (reg == 0x2d) {
        if (!noteStopped) throw std::runtime_error("JAudio note-state service is not connected.");
        unsigned stopped = 0;
        for (unsigned slot = 0; slot < 8; ++slot) if (noteStopped(index, slot)) stopped |= 1u << slot;
        return stopped;
    }
    throw std::runtime_error("Unported JAudio register read: " + std::to_string(reg));
}

void jaudio::Interpreter::stop(unsigned index, const Listener& listener) {
    auto& current = tracks[index];
    if (!current.active) return;
    current.active = false;
    listener({EventType::finish, index});
    for (const auto child : current.children) if (child >= 0) stop(child, listener);
}

void jaudio::Interpreter::close(unsigned index, const Listener& listener) {
    stop(index, listener);
    auto& current = tracks[index];
    for (const auto child : current.children) if (child >= 0) close(child, listener);
    current.children.fill(-1);
    current.parent = -1;
}

void jaudio::Interpreter::update(unsigned index, const Listener& listener) {
    auto& current = tracks[index];
    if (!current.active) return;
    current.combinedTranspose = static_cast<std::uint8_t>(current.transpose + (current.parent >= 0 ? tracks[current.parent].combinedTranspose : 0));
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
                auto key = static_cast<std::uint8_t>(opcode + current.combinedTranspose);
                const auto mode = take(current, 1);
                if (mode & 0x80) key = static_cast<std::uint8_t>(readRegister(index, key) + current.combinedTranspose);
                const auto slot = mode & 0x3f;
                const auto velocity = take(current, 1);
                if (velocity >= 128 || slot == 0 || (slot > 8 && slot != 16) || ((mode & 0x40) && slot != 8 && slot != 16))
                    throw std::runtime_error("Unported JAudio note mode " + std::to_string(mode) + " at " + std::to_string(position));
                if (slot == 8 || slot == 16) {
                    const auto gate = take(current, 1);
                    if (!gate || gate >= 128) throw std::runtime_error("Unported JAudio note gate: " + std::to_string(gate));
                    current.wait = take(current, slot >> 3);
                    if (current.wait == 0 || (slot == 8 && current.wait >= 128)) throw std::runtime_error("Unported JAudio note duration.");
                    const auto clock = (static_cast<float>(tracks[0].tempo * tracks[0].timebase) / ((60.0f * 32028.5f) / 80.0f)) * tempoScale;
                    listener({EventType::noteOn, index, 0, mode & 0x40 ? current.lastNote : key, velocity,
                        static_cast<unsigned>(((static_cast<float>(current.wait) * gate) / 100.0f) / clock),
                        mode & 0x40 ? static_cast<int>(static_cast<std::uint8_t>(key + current.combinedTranspose)) : -1});
                    yield = true;
                } else {
                    listener({EventType::noteOn, index, slot, key, velocity});
                }
                current.lastNote = key;
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
            } else if (opcode == 0xa0 || opcode == 0xa1 || opcode == 0xa3 || opcode == 0xa4 || opcode == 0xa5 || opcode == 0xa7 || opcode == 0xa9 || opcode == 0xac || opcode == 0xad || opcode == 0xaf) {
                const auto operation = opcode == 0xa9 ? take(current, 1) : opcode & 15;
                const auto reg = take(current, 1);
                const auto operandType = operation & 12;
                if (operandType == 8) throw std::runtime_error("Unported JAudio register operand.");
                auto value = operandType == 12 ? take(current, 2) : take(current, 1);
                if (operandType == 0) value = readRegister(index, value);
                if (opcode == 0xa9) {
                    if ((operation & 0xf0) == 0x90) {
                        if (!(value & 0xffff)) throw std::runtime_error("Invalid JAudio random range.");
                        value = static_cast<std::uint16_t>((random ? random() : sequenceRandom()) % (value & 0xffff));
                    } else if ((operation & 0xf0) == 0x30) value &= readRegister(index, reg);
                    else throw std::runtime_error("Unported JAudio extended register operation.");
                } else if (opcode == 0xad || opcode == 0xaf || opcode == 0xa7 || opcode == 0xa3 || opcode == 0xa5 || opcode == 0xa1) {
                    unsigned source = 0;
                    if (reg == 0x2c) {
                        for (unsigned child = 0; child < current.children.size(); ++child) {
                            if (current.children[child] >= 0 && tracks[current.children[child]].active) source |= 1u << child;
                        }
                    } else {
                        if (reg >= current.registers.size()) throw std::runtime_error("Unported JAudio arithmetic register: " + std::to_string(reg));
                        source = current.registers[reg];
                    }
                    if (opcode == 0xaf || opcode == 0xa7 || opcode == 0xa3) {
                        current.registers[3] = static_cast<std::uint16_t>(source - value);
                        continue;
                    }
                    if (opcode == 0xa5) value = static_cast<std::uint16_t>(static_cast<std::int8_t>(value));
                    value = static_cast<std::uint16_t>(source + value);
                }
                if (reg < 3) current.registers[reg] = value & 255;
                else if (reg >= 3 && reg <= 5) current.registers[reg] = value;
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
                    current.envelopeRoute = 15;
                    current.adsr.reset();
                }
            } else if (opcode == 0xc1) {
                const auto slot = take(current, 1);
                const auto offset = take(current, 3);
                if (slot >= 16 || offset >= data.size()) throw std::runtime_error("Invalid JAudio child track.");
                if (current.children[slot] >= 0) close(current.children[slot], listener);
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
                found->externalBlend = current.externalBlend;
                found->parentBlend = current.parentBlend;
                found->externalWeight = current.externalWeight;
                found->parentWeight = current.parentWeight;
                current.children[slot] = child;
            } else if (opcode == 0xc4) {
                const auto flags = take(current, 1);
                auto offset = flags & 0x80 ? current.registers.at(take(current, 1)) & 0xffff : take(current, 3);
                if ((flags & 0xc0) == 0xc0) {
                    const auto table = flags & 0x20 ? current.registers.at(take(current, 1)) & 0xffff : take(current, 3);
                    const auto entry = table + offset * 3;
                    if (entry + 3 > data.size()) throw std::runtime_error("Truncated JAudio call table.");
                    offset = (unsigned(data[entry]) << 16) | (unsigned(data[entry + 1]) << 8) | data[entry + 2];
                }
                const auto condition = flags & 15;
                const auto value = current.registers[3];
                if (condition > 5) throw std::runtime_error("Unported JAudio call condition.");
                const bool taken = condition == 0 || (condition == 1 && value == 0) || (condition == 2 && value != 0) ||
                    (condition == 3 && value == 1) || (condition == 4 && value >= 0x8000) || (condition == 5 && value < 0x8000);
                if (!taken) continue;
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
                if (port >= current.ports.size() || (reg >= 32 && reg != 34)) throw std::runtime_error("Unported JAudio port read.");
                current.importedPorts[port] = false;
                const auto value = current.ports[port] & 0xffff;
                if (reg == 34) {
                    current.registers[0] = value >> 8;
                    current.registers[1] = value & 255;
                } else current.registers[reg] = reg < 3 ? value & 255 : value;
                current.registers[3] = reg < 3 ? static_cast<std::uint16_t>(static_cast<std::int8_t>(value)) : value;
                if (reg == 6) { current.bank = value >> 8; current.program = value & 255; }
                if (reg == 7) current.bendRange = value;
                if (reg == 11) current.externalWeight = value / 32767.0f;
                if (reg == 12) current.parentWeight = value / 32767.0f;
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
            } else if (opcode == 0xd4) {
                current.lastNote = static_cast<std::uint8_t>(take(current, 1) + current.combinedTranspose);
            } else if (opcode == 0xd8) {
                std::array<std::uint16_t, 5> values;
                for (auto& value : values) value = static_cast<std::uint16_t>(take(current, 2));
                current.adsr = values;
            } else if (opcode == 0xd9) {
                current.transpose = static_cast<std::uint8_t>(take(current, 1));
                current.combinedTranspose = static_cast<std::uint8_t>(current.transpose + (current.parent >= 0 ? tracks[current.parent].transpose : 0));
            } else if (opcode == 0xda) {
                const auto slot = take(current, 1);
                if (slot >= 16) throw std::runtime_error("Invalid JAudio close-track slot.");
                if (current.children[slot] >= 0) {
                    close(current.children[slot], listener);
                    current.children[slot] = -1;
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
                else if (sync != 0 && sync != 1 && sync != 127) throw std::runtime_error("Unported JAudio sync callback: " + std::to_string(sync));
                if (sync == 0 || sync == 127) current.externalControl = true;
                listener({EventType::sync, index, sync});
            } else if (opcode == 0xf0) {
                const auto route = take(current, 1);
                if (route == 0) current.envelopeRoute = 0;
                else if (route == 0x11) current.oscillatorRoute = route;
                else throw std::runtime_error("Unported JAudio oscillator routing: " + std::to_string(route));
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

void jaudio::Interpreter::bindEffectParameters(unsigned track, const std::array<float, 5>& values) {
    auto& current = tracks.at(track);
    current.effectParameters = values;
    current.effectControl = true;
}

void jaudio::Interpreter::updateEffectParameters(unsigned channel, const std::array<float, 5>& values, unsigned mask) {
    for (auto& current : tracks) {
        if (!current.active || !current.effectControl || (current.route & 255) != channel) continue;
        for (unsigned parameter = 0; parameter < values.size(); ++parameter) {
            if (mask & (1u << parameter)) current.effectParameters[parameter] = values[parameter];
        }
    }
}

void jaudio::Interpreter::configureSequenceBlend(unsigned index, unsigned flags, bool specialPan) {
    auto& current = tracks.at(index);
    if ((current.route >> 28) == 0) return;
    for (unsigned field = 0; field < 3; ++field) {
        const unsigned flag = field == 0 ? 8 : field == 1 ? 4 : 16;
        if (!(flags & flag) || (field == 0 && specialPan)) {
            current.externalBlend[field] = current.parentBlend[field] = 0;
        }
    }
}

bool jaudio::Interpreter::writeRoutePort(unsigned route, unsigned port, std::uint16_t value) {
    for (unsigned index = 0; index < tracks.size(); ++index) {
        if (tracks[index].active && tracks[index].route == route) {
            writePort(index, port, value);
            return true;
        }
    }
    return false;
}

bool jaudio::Interpreter::requestRouteInterrupt(unsigned route, unsigned interrupt) {
    if (interrupt >= 8) throw std::out_of_range("Sequence interrupt index exceeds its original table.");
    for (auto& current : tracks) {
        if (!current.active || current.route != route) continue;
        current.requestInterrupt(interrupt);
        return true;
    }
    return false;
}

bool jaudio::Interpreter::readRoutePort(unsigned route, unsigned port, std::uint16_t& value) const {
    for (const auto& current : tracks) {
        if (current.active && current.route == route) {
            value = static_cast<std::uint16_t>(current.ports.at(port));
            return true;
        }
    }
    return false;
}
