#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/world/doorstate.h"
#include "game/world/doorpresentation.h"
#include "game/startup/startupresources.h"

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace {
    unsigned word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
            (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
    }
    std::int32_t signedWord(std::uint32_t bits) {
        return static_cast<std::int32_t>(bits < 0x80000000U ? static_cast<std::int64_t>(bits) :
            static_cast<std::int64_t>(bits) - 0x100000000LL);
    }
    int frameInteger(float value) {
        if (!std::isfinite(value) || static_cast<double>(value) < -2147483648.0 || static_cast<double>(value) >= 2147483648.0)
            throw std::runtime_error("Unsupported door frame conversion.");
        return static_cast<int>(value);
    }
    float real(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        const auto bits = word(bytes, offset);
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }
    doors::CollisionBox collisionBox(const std::vector<std::uint8_t>& bytes, const dooranimation::Matrix& matrix) {
        doors::CollisionBox result;
        const float x = real(bytes, 0), y = real(bytes, 4), z = real(bytes, 8);
        for (unsigned row = 0; row < 3; ++row) {
            const float first = matrix[row * 4] * x, second = matrix[row * 4 + 1] * y;
            const float a = std::fma(matrix[row * 4 + 2], z, first);
            const float b = std::fma(matrix[row * 4 + 3], 1.0f, second);
            result.center[row] = a + b;
            for (unsigned axis = 0; axis < 3; ++axis) result.axes[axis][row] = matrix[row * 4 + 2 - axis];
            result.extents[row] = real(bytes, 20 - row * 4);
        }
        return result;
    }
}

bool doors::State::initialize(const doordata::Table& descriptors, std::shared_ptr<const Resources> source,
    const std::vector<std::uint8_t>& executable, const SavedFlags& savedFlags, std::string& error) {
    error.clear();
    try {
        if (!source || !source->ready()) throw std::runtime_error("Door resources must be ready before initialization.");
        State result;
        result.resources = std::move(source);
        result.leaves.reserve(descriptors.liveCount);
        for (auto& pair : result.descriptorPairs) pair.fill(0xff);
        for (auto& pair : result.savedPairs) pair.fill(0xff);
        for (const auto& descriptor : descriptors.entries) {
            if (descriptor.identity == 0xff) continue;
            const unsigned type = (descriptor.identity >> 8) & 255;
            std::vector<std::uint8_t> configuration;
            if (!dolresources::read(executable, glme01::doorConfigurations + type * 20, 20, configuration, error)) return false;
            const unsigned count = configuration[0] == 2 ? 2 : 1;
            for (unsigned side = 0; side < count; ++side) {
                if (result.leaves.size() >= 128) throw std::runtime_error("Unsupported signed-byte door initialization overflow.");
                Leaf value;
                value.savedId = static_cast<std::uint8_t>(descriptor.identity >> 16);
                value.type = static_cast<std::uint8_t>(type);
                value.references.fill(0xff);
                value.flags = 8;
                if (savedFlags[value.savedId >> 5] & (0x80000000U >> (value.savedId & 31))) value.flags |= 0x4400;
                value.rooms = descriptor.rooms;
                const bool rotated = !doordata::traversalEnabled(descriptor);
                if (rotated) { value.flags |= 0x100; value.angles[1] = -0x4000; }
                if (side) {
                    value.angles[1] = rotated ? 0x4000 : -0x8000;
                    std::swap(value.rooms[0], value.rooms[1]);
                }
                for (unsigned axis = 0; axis < 3; ++axis) {
                    const unsigned offsetAxis = rotated && axis != 1 ? 2 - axis : axis;
                    const float offset = static_cast<float>(signedWord(word(configuration, 4 + offsetAxis * 4)));
                    float center = static_cast<float>(descriptor.center[axis]);
                    if (axis == 1) center -= static_cast<float>(descriptor.dimensions[1]) * 0.5f;
                    value.position[axis] = side ? center - offset : center + offset;
                }
                value.transform = result.resources->poseTables().world(value.position, value.angles, {1, 1, 1});
                if (type) {
                    std::vector<std::uint8_t> box;
                    if (!dolresources::read(executable, glme01::doorCollisionBoxes + configuration[16] * 24, 24, box, error)) return false;
                    value.collision = collisionBox(box, value.transform);
                }
                const auto index = static_cast<std::uint8_t>(result.leaves.size());
                result.descriptorPairs[descriptor.identity & 255][side] = index;
                result.savedPairs[value.savedId][side] = index;
                result.leaves.push_back(std::move(value));
            }
        }
        if (result.leaves.size() != descriptors.liveCount) throw std::runtime_error("Door allocation count differs from descriptor traversal.");
        for (auto& value : result.leaves) {
            std::vector<std::uint8_t> configuration;
            if (!dolresources::read(executable, glme01::doorConfigurationFlags + value.type * 20, 2, configuration, error)) return false;
            value.pose.bind(result.resources);
            value.pose.selectModel((unsigned(configuration[0]) << 8) | configuration[1]);
            value.pose.initializeMatrices();
            value.pose.update(value.frame, value.transform, value.animation);
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}

unsigned doors::State::size() const { return static_cast<unsigned>(leaves.size()); }
const doors::Leaf& doors::State::leaf(unsigned index) const { return leaves.at(index); }

std::uint32_t doors::State::action(unsigned index, int direction, std::int8_t command, bool forced) {
    if (leaves.empty()) return 0;
    auto& value = leaves.at(index);
    if (!command) return value.flags;
    if (forced) value.flags |= 0x200;
    else if (value.flags & 0x200) return value.flags;
    const auto setDirection = [&] { value.flags = direction == 1 ? value.flags | 0x800 : value.flags & ~0x800U; };
    if (command == 1 || command == 3) {
        if (value.flags & 8) {
            value.flags |= command == 1 ? 0x20 : 0x40;
            value.flags &= ~8U;
            setDirection();
            value.frame = 0;
        }
    } else if (command == 2) {
        value.flags = (value.flags & ~0x50U) | 0x80;
        setDirection();
        value.animation = direction == 1 ? 2 : 4;
        value.frame = static_cast<float>(signedWord(resources->animation(value.animation).headerValue()));
    } else if (command == 4) {
        value.flags = (value.flags | 0x1000) & ~0x50U;
        setDirection();
        value.frame = 0;
    }
    return value.flags;
}

void doors::State::advanceAnimation(unsigned index, const SoundEmitter& emit) {
    if (!emit) throw std::runtime_error("Door animation requires a sound dispatcher.");
    auto& value = leaves.at(index);
    const auto play = [&](unsigned id) { emit(id, value.position); };
    const auto end = [&] { return signedWord(resources->animation(value.animation).headerValue()); };
    if (value.flags & 0x20) {
        value.animation = value.flags & 0x800 ? 0 : 1;
        const int frame = frameInteger(value.frame);
        if (!(value.flags & 0x800) && frame == 25) play(0x1808);
        if ((value.flags & 0x800) && frame == 35) play(0x180b);
        if (frame == 12) play(0x1809);
        if (frame == signedWord(static_cast<std::uint32_t>(end()) - 2)) play(0x180a);
        value.frame += 1;
        if (end() < frameInteger(value.frame)) {
            value.flags = ((value.flags & ~0x220U) | 8) & ~0x800U;
            value.frame = 0;
        }
    } else if (value.flags & 0x40) {
        value.animation = value.flags & 0x800 ? 2 : 4;
        if (frameInteger(value.frame) == 12) play(0x181e);
        value.frame += 1;
        if (end() < frameInteger(value.frame)) value.flags = (value.flags & ~0x40U) | 0x10;
    } else if (value.flags & 0x80) {
        value.frame -= 1;
        if (frameInteger(value.frame) < 0) {
            value.flags = (value.flags | 8) & ~0x280U;
            play(0x180a);
        }
    } else if (value.flags & 0x1000) {
        value.animation = value.flags & 0x800 ? 3 : 5;
        value.frame += 1;
        if (end() < frameInteger(value.frame)) {
            value.flags = (value.flags | 8) & ~0x1200U;
            play(0x180a);
        }
    }
}

void doors::State::refreshPose(unsigned index) {
    auto& value = leaves.at(index);
    value.pose.update(value.frame, value.transform, value.animation);
}

bool doors::State::blocked(std::uint32_t descriptorId) const {
    if (descriptorId == 0xff || leaves.empty()) return false;
    const auto& pair = descriptorPairs[descriptorId & 255];
    const bool first = (leaves.at(pair[0]).flags & 8) != 0;
    return pair[1] == 0xff ? first : first && (leaves.at(pair[1]).flags & 8) != 0;
}

std::array<std::uint8_t, 2> doors::State::savedPair(std::uint8_t id) const { return savedPairs[id]; }

void doors::State::setSavedFlag(std::uint8_t id, bool enabled, SavedFlags& savedFlags) {
    if (leaves.empty()) return;
    const auto& pair = savedPairs[id];
    for (unsigned side = 0; side < 2; ++side) {
        if (side && pair[side] == 0xff) break;
        auto& value = leaves.at(pair[side]);
        value.flags = enabled ? value.flags | 0x4400 : value.flags & ~0x4400U;
        const auto mask = 0x80000000U >> (value.savedId & 31);
        auto& saved = savedFlags[value.savedId >> 5];
        saved = enabled ? saved | mask : saved & ~mask;
    }
}

void doors::State::setEventBlocked(std::uint8_t id, bool enabled) {
    if (leaves.empty()) return;
    for (auto index : savedPairs[id]) {
        if (index == 0xff) break;
        auto& value = leaves.at(index);
        value.flags = enabled ? value.flags | 0x10000 : value.flags & ~0x10000U;
    }
}

std::uint32_t doors::State::markUsed(unsigned index, std::uint8_t& savedId, std::uint8_t& type) {
    auto& value = leaves.at(index);
    savedId = value.savedId;
    type = value.type;
    value.flags |= 0x8000;
    return value.flags & 0x400;
}

void doors::State::setCollisionActive(std::uint8_t id, bool enabled) {
    for (auto index : savedPairs[id])
        if (index != 0xff) leaves.at(index).collisionActive = enabled ? 1 : 0;
}

void doors::State::updateVisibility(unsigned buffer, const std::vector<RoomVisit>& visits,
    const std::array<std::uint32_t, 256>& roomIds, std::uint32_t playerRoom) {
    if (buffer >= 8) throw std::out_of_range("Door visibility buffer.");
    for (auto& value : leaves) value.flags &= ~0x2003U;
    for (const auto& visit : visits) {
        for (auto id : visit.doorIds) {
            if (id == 0xff || leaves.empty()) continue;
            for (auto index : descriptorPairs[id & 255]) {
                if (index == 0xff) break;
                auto& value = leaves.at(index);
                value.flags |= 1;
                if (visit.marker < 100) {
                    value.flags |= 0x2000;
                    value.references[buffer] = visit.marker;
                } else {
                    const unsigned playerArea = (playerRoom >> 8) & 255;
                    if ((((roomIds[value.rooms[0]] >> 8) & 255) < playerArea ||
                        ((roomIds[value.rooms[1]] >> 8) & 255) < playerArea) && !(value.flags & 0x100)) value.flags |= 2;
                    else if (!(value.flags & 0x1088) && (!(value.flags & 0x20) || value.frame <= 20)) value.flags |= 2;
                    else value.flags &= ~2U;
                }
            }
        }
    }
}

void doors::State::update(unsigned map, std::uint32_t frameCounter, gameflags::State& flags,
    SavedFlags& savedFlags, const UpdateServices& services) {
    if (!services.positionalSound || !services.globalSound || !services.playerInfluence || !services.influenceSound)
        throw std::runtime_error("Door update requires its original audio and player query services.");
    active = -1;
    for (unsigned index = 0; index < leaves.size(); ++index) {
        auto& value = leaves[index];
        if (!value.type || !(value.flags & 1)) continue;
        if (value.flags & 0x200) advanceAnimation(index, services.positionalSound);
        if (value.flags & 0x200) active = static_cast<int>(index);
        const int strength = services.playerInfluence(200, value.position);
        if (!strength) {
            if (shakeFrame < 5) {
                const unsigned angle = shakeFrame * 0x9dd0;
                value.angles[0] = frameInteger(500 * resources->poseTables().sineAngle(angle + 0x4000));
                value.angles[2] = frameInteger(500 * resources->poseTables().sineAngle(angle));
                if (!shakeFrame) services.positionalSound(0x187e, value.position);
                ++shakeFrame;
            } else value.angles[0] = value.angles[2] = 0;
        } else {
            const float magnitude = static_cast<float>(signedWord(static_cast<std::uint32_t>(strength) * 5));
            const unsigned angle = (frameCounter & 255) * 0x9000;
            value.angles[0] = frameInteger(magnitude * resources->poseTables().sineAngle(angle + 0x4000));
            value.angles[2] = frameInteger(magnitude * resources->poseTables().sineAngle(angle));
            if (!(frameCounter & 3)) services.influenceSound(magnitude, 2, value.position);
        }
        value.transform = resources->poseTables().world(value.position, value.angles, {1, 1, 1});
        refreshPose(index);
    }
    if (map != 2) return;
    const auto combined = [&](std::uint8_t id) {
        std::uint32_t value = 0;
        if (!leaves.empty()) for (auto index : savedPairs[id]) {
            if (index == 0xff) break;
            value |= leaves.at(index).flags;
        }
        return value;
    };
    if (!flags.value(3) && (combined(0x22) & 0x8000)) {
        flags.setOn(3);
        services.globalSound(0x4872);
    }
    if (!flags.value(0x38)) {
        if (combined(0x14) & 0x8000) {
            flags.setOn(0xaf);
            for (auto index : savedPairs[0x14]) {
                if (index == 0xff) break;
                leaves.at(index).flags &= ~0x8000U;
            }
        }
    } else if (combined(0x14) & 0x400) setSavedFlag(0x14, false, savedFlags);
}

void doors::State::updatePresentation(unsigned buffer, const std::vector<RoomVisit>& visits,
    const std::array<std::uint32_t, 256>& roomIds, std::uint32_t playerRoom,
    const std::array<float, 3>& playerPosition) {
    updateVisibility(buffer, visits, roomIds, playerRoom);
    for (auto& value : leaves) {
        const bool controlled = (value.flags & 2) != 0;
        const auto target = controlled ? distanceOpacity(value.position, playerPosition) : 0;
        value.references[buffer] = advanceOpacity(value.references[buffer], controlled, target);
    }
}

void doors::State::submitOpaque(unsigned buffer, bool overrideOpacity, const ModelSubmitter& submit) const {
    if (buffer >= 8 || !submit) throw std::runtime_error("Door drawing requires a valid buffer and model renderer.");
    for (const auto& value : leaves) {
        if (!(value.flags & 1) || (!overrideOpacity && value.references[buffer] != 255)) continue;
        const auto draw = [&](unsigned model) {
            submit(value.pose.model(model), value.pose, 0, 255, false);
            submit(value.pose.model(model), value.pose, 1, 255, false);
        };
        draw(value.pose.selectedModel());
        if (barrier || (value.flags & 0x10000)) draw(0);
    }
}

void doors::State::submitTranslucent(unsigned buffer, bool overrideOpacity, const ModelSubmitter& submit) const {
    if (buffer >= 8 || !submit) throw std::runtime_error("Door drawing requires a valid buffer and model renderer.");
    for (const auto& value : leaves) {
        const auto opacity = overrideOpacity ? std::uint8_t(0) : value.references[buffer];
        if (!(value.flags & 1) || opacity == 255 || opacity == 0) continue;
        const auto draw = [&](unsigned model) {
            submit(value.pose.model(model), value.pose, 2, opacity, opacity < 110);
        };
        draw(value.pose.selectedModel());
        if (barrier || (value.flags & 0x10000)) draw(0);
    }
}

int doors::State::activeDoor() const { return active; }

void doors::State::setGlobalBarrier(bool enabled, const std::function<void(unsigned, unsigned, unsigned)>& notify) {
    if (!notify) throw std::runtime_error("Door barrier change requires the original actor message dispatcher.");
    barrier = enabled;
    shakeFrame = 0;
    barrierLevel = enabled ? 0xff : 0;
    notify(0, 0x12, 1);
}

void doors::State::resetGlobalBarrier() {
    barrier = false;
    shakeFrame = 10;
    barrierLevel = 0;
}

std::uint32_t doors::State::eventBlocked(std::uint32_t index) const {
    if (barrier) return 1;
    if (!leaves.empty() && (index & 255) != 255) return leaves.at(index & 255).flags & 0x10000;
    return 0;
}

bool doors::State::globalBarrier() const { return barrier; }
std::uint8_t doors::State::barrierValue() const { return barrierLevel; }
