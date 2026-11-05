#include "playeritempresentation.h"
#include "resources/dolresources.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    void read(const std::vector<std::uint8_t>& executable, unsigned address, float* output, unsigned count) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, count * 4, bytes, error)) throw std::runtime_error(error);
        for (unsigned i = 0; i < count; ++i) {
            const std::uint32_t bits = (unsigned(bytes[i * 4]) << 24) | (unsigned(bytes[i * 4 + 1]) << 16) |
                (unsigned(bytes[i * 4 + 2]) << 8) | bytes[i * 4 + 3];
            std::memcpy(output + i, &bits, 4);
        }
    }

    void scale(actormodel::Matrix& matrix, float value) {
        for (unsigned row = 0; row < 3; ++row) for (unsigned column = 0; column < 3; ++column)
            matrix[row * 4 + column] *= value;
    }

    enum Constant : unsigned { zero, attachFrame, one, specialScale, effectFrame, releaseFrame, shrinkDuration,
        facingScale, angleUnits, fullCircle };
    constexpr unsigned presentationConstants = 0x804da9c8;
    constexpr unsigned presentationPlacement = 0x80351f30;
    constexpr unsigned presentationTurnBlend = 0x804d2ba4;
}

playerpresentation::Frame::Frame(const std::vector<std::uint8_t>& executable) : poses(executable) {
    read(executable, presentationConstants, constants.data(), static_cast<unsigned>(constants.size()));
    read(executable, presentationPlacement, placement.data(), static_cast<unsigned>(placement.size()));
    read(executable, presentationTurnBlend, &turnBlend, 1);
}

actormodel::Matrix playerpresentation::Frame::transform(unsigned actorType) const {
    if (actorType) return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    std::array<std::int16_t, 3> angles;
    for (unsigned i = 0; i < 3; ++i)
        angles[i] = static_cast<std::int16_t>(static_cast<std::int32_t>((constants[angleUnits] * placement[i + 3]) / constants[fullCircle]));
    auto result = poses.rotation(angles);
    for (unsigned i = 0; i < 3; ++i) result[i * 4 + 3] = placement[i];
    return result;
}

bool playerpresentation::Frame::run(playeractor::Player& player, Services& services) const {
    auto& state = *player.components->state80;
    auto& actions = *player.components->actions;
    auto* item = player.state.field1064;
    bool presenting = player.state.field1060 != 0;
    bool sceneRequested = false;
    const float frame = player.animationFrame;
    if (actions.field310 == 2) {
        if (actions.field314 != 0x22) return false;
        const auto stage = actions.fields31C[3];
        if (stage == 3) {
            if (player.flags & 0x20000) {
                if (state.field7C) --state.field7C;
                if (!state.field7C) actions.request(0x20);
            }
        } else if (stage == 2) {
            if (!state.field10 && constants[attachFrame] <= frame) {
                state.field14 = transform(player.actorType);
                state.field44 = player.actorType ? 0 : 0x13;
                state.field78 = constants[one];
                if (item) {
                    auto matrix = state.field14;
                    if (item->actorType == 0xcc) scale(matrix, constants[specialScale]);
                    player.attachChild(*item, state.field44, &matrix, 1);
                    item->selectAnimation("ITEMGET");
                }
                state.field10 = 1;
            }
            if (!state.field11 && constants[effectFrame] <= frame) {
                state.field7C = 1;
                if (item) {
                    bool special = false;
                    if (item->actorType >= 0x57 && item->actorType <= 0x5a) {
                        const unsigned first = 0x3e + (item->actorType - 0x57) * 2;
                        services.effect(first, item->position);
                        services.effect(first + 1, item->position);
                        state.field7C = 2;
                        special = true;
                    } else if (item->actorType == 0xcc) {
                        for (unsigned effect = 0x59; effect <= 0x5b; ++effect) services.effect(effect, item->position);
                        state.field7C = 3;
                        special = true;
                    }
                    services.music(services.specialItem(item->actorType) && special ? 0x80000035 : 0x8000000e);
                }
                state.field11 = 1;
            }
        } else if (stage == 4) {
            if (state.field10) {
                if (frame < constants[releaseFrame]) {
                    state.field14 = transform(player.actorType);
                    state.field44 = player.actorType ? 0 : 0x13;
                    float value = (frame - constants[releaseFrame]) / constants[shrinkDuration];
                    if (value < constants[zero]) value = constants[zero];
                    if (constants[one] < value) value = constants[one];
                    state.field78 = value;
                    if (item) {
                        auto matrix = state.field14;
                        scale(matrix, value);
                        if (item->actorType == 0xcc) scale(matrix, constants[specialScale]);
                        player.updateChildAttachment(*item, state.field44, &matrix, 1);
                    }
                } else {
                    if (item) {
                        player.detachChild(*item);
                        services.collected(*item);
                        if (item->actorType == 0xcc) {
                            services.setFlag(0x42);
                            if (!services.equipmentOwned()) services.equipmentEvent(0, 0x15, 0xdd);
                        }
                        services.sound(0x84f, player.position);
                    }
                    state.field10 = 0;
                }
            }
            if (!state.field10) presenting = false;
            if ((player.flags & 0x40000) && player.state.field1061) {
                services.scene(1, player.state.field1068);
                sceneRequested = true;
            }
        }
    } else {
        if (actions.field310 != 1 || !presenting) return false;
        actions.request(0x0e);
        state.fieldC = 0;
        state.field10 = state.field11 = 0;
    }
    for (unsigned i = 0; i < 3; ++i) state.field0[i] = constants[facingScale] * player.state.fieldF80[i];
    player.state.field1070 = 7;
    auto direction = state.field0;
    direction[1] = 0;
    const float length = std::sqrt((direction[0] * direction[0] + direction[1] * direction[1]) + direction[2] * direction[2]);
    if (length >= 0.0001f) for (auto& axis : direction) axis *= 1 / length;
    else direction.fill(0);
    player.state.field1090 = direction;
    player.state.field109C = turnBlend;
    player.state.field1060 = presenting;
    if (!presenting) player.state.field1064 = nullptr;
    if (sceneRequested && player.state.field1061) {
        player.state.field1061 = 0;
        player.state.field1068 = -1;
    }
    return true;
}
