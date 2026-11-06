#include "game/scene/gameplaycamerafollow.h"
#include "resources/dolresources.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
    constexpr unsigned mapTwoDescriptor = 0x802ff220, presetTable = 0x802ffc48;
    constexpr unsigned leadScaleAddress = 0x804d9ce0, leadDecayAddress = 0x804d9ce4;
    constexpr unsigned insetAddress = 0x804d0098, heightThresholdAddress = 0x804d9cec;
    constexpr unsigned followRateAddress = 0x804d9cf0, absentRoomRateAddress = 0x804d9d04;
    constexpr unsigned nearThresholdAddress = 0x804d9d00, farThresholdAddress = 0x804d9cfc;
    constexpr unsigned reactionHeightAddress = 0x804d9d08, reactionDepthAddress = 0x804d9d0c;
    constexpr unsigned quantizationAddress = 0x804d9a7c, inverseQuantizationAddress = 0x804d9a78;
    constexpr unsigned smoothingScaleAddress = 0x804d9a8c, fieldOfViewAddress = 0x804d009c;
    constexpr unsigned coincidentOffsetAddress = 0x804d9cb8, nearDelayAddress = 0x804d00a0, normalDelayAddress = 0x804d00a1;
    constexpr unsigned nearPlaneAddress = 0x804d0000, farPlaneAddress = 0x804d0004;
    constexpr unsigned upHorizontalAddress = 0x804d9cd0, upVerticalAddress = 0x804d9d10;

    unsigned word(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }

    float scalar(const std::vector<std::uint8_t>& executable, unsigned address) {
        const auto bits = word(executable, address);
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    std::int32_t integer(float value) {
        if (!std::isfinite(value) || value < -2147483648.0f || value >= 2147483648.0f)
            return std::numeric_limits<std::int32_t>::min();
        return static_cast<std::int32_t>(value);
    }
}

gameplaycamera::Follow::Follow(const std::vector<std::uint8_t>& executable) {
    const auto descriptor = word(executable, mapTwoDescriptor);
    if ((word(executable, descriptor) >> 24) <= 2) throw std::runtime_error("Camera follow requires the original foyer room.");
    const auto room = word(executable, descriptor + 20) + 2 * 48;
    roomId = word(executable, room);
    roomFlags = word(executable, room + 4);
    if (roomId != 0x02000102 || roomFlags != 0)
        throw std::runtime_error("Camera follow requires the original first-visit room descriptor.");
    for (unsigned axis = 0; axis < 3; ++axis) {
        const auto lower = word(executable, room + 8 + axis * 4);
        const auto upper = word(executable, room + 20 + axis * 4);
        minimum[axis] = static_cast<float>(static_cast<std::int32_t>(lower));
        extent[axis] = static_cast<float>(static_cast<std::int32_t>(upper - lower));
    }
    for (unsigned mode = 0; mode < presets.size(); ++mode)
        for (unsigned field = 0; field < 4; ++field) presets[mode][field] = scalar(executable, presetTable + mode * 16 + field * 4);
    parameters = presets[0];
    nearDelay = word(executable, nearDelayAddress) >> 24;
    normalDelay = word(executable, normalDelayAddress) >> 24;
    leadScale = scalar(executable, leadScaleAddress);
    leadDecay = scalar(executable, leadDecayAddress);
    inset = scalar(executable, insetAddress);
    heightThreshold = scalar(executable, heightThresholdAddress);
    followRate = scalar(executable, followRateAddress);
    absentRoomRate = scalar(executable, absentRoomRateAddress);
    nearThreshold = scalar(executable, nearThresholdAddress);
    farThreshold = scalar(executable, farThresholdAddress);
    reactionHeight = scalar(executable, reactionHeightAddress);
    reactionDepth = scalar(executable, reactionDepthAddress);
    quantization = scalar(executable, quantizationAddress);
    inverseQuantization = scalar(executable, inverseQuantizationAddress);
    smoothingScale = scalar(executable, smoothingScaleAddress);
    fieldOfView = scalar(executable, fieldOfViewAddress);
    coincidentOffset = scalar(executable, coincidentOffsetAddress);
    nearPlane = static_cast<float>(static_cast<std::int32_t>(word(executable, nearPlaneAddress)));
    farPlane = static_cast<float>(static_cast<std::int32_t>(word(executable, farPlaneAddress)));
    up = {scalar(executable, upHorizontalAddress), scalar(executable, upVerticalAddress), scalar(executable, upHorizontalAddress)};
}

unsigned gameplaycamera::Follow::preset() const { return selected; }

void gameplaycamera::Follow::select(unsigned value, unsigned delay) {
    if (value == selected) return;
    if (selectionTicks < delay) { ++selectionTicks; return; }
    parameters = presets.at(value);
    selected = value;
    selectionTicks = 0;
}

void gameplaycamera::Follow::approach(std::array<float, 3>& value, const std::array<float, 3>& target, float rate) const {
    for (unsigned axis = 0; axis < 3; ++axis) {
        const float delta = static_cast<float>(integer(quantization * (target[axis] - value[axis])));
        const float step = (smoothingScale * inverseQuantization) * delta;
        value[axis] += static_cast<float>(double(step) * double(rate));
    }
}

void gameplaycamera::Follow::update(CameraOwner& camera, const playeractor::View& player, unsigned room, unsigned playerIndex) {
    if (!player.position) throw std::runtime_error("Camera follow requires the published player position.");
    if (playerIndex >= leadX.size()) throw std::out_of_range("Camera player index exceeds the original subject table.");
    if (room != roomId && (room >> 24) != 255)
        throw std::runtime_error("Camera follow requires room-transition bindings outside the foyer.");
    if ((camera.flags & 5) || player.field895 || player.field894 || player.field8C9)
        throw std::runtime_error("Camera follow requires the selected original scanner, ladder or override callback.");
    camera.aspect = static_cast<float>(camera.viewport[2] - camera.viewport[0]) /
        static_cast<float>(camera.viewport[3] - camera.viewport[1]);
    camera.fields8[8] = 0;
    camera.fieldOfView = fieldOfView;
    camera.nearPlane = nearPlane;
    camera.farPlane = farPlane;
    camera.up = up;
    for (unsigned axis : {0u, 2u}) {
        auto& lead = axis ? leadZ[playerIndex] : leadX[playerIndex];
        if (!player.velocity) lead = 0;
        else {
            const float next = leadScale * (*player.velocity)[axis];
            lead = std::abs(next) < std::abs(lead) ? lead * leadDecay : next;
        }
    }
    if ((room >> 24) == 255) approach(camera.target, *player.position, absentRoomRate);
    else {
        const bool far = player.field89E && player.field8AC > farThreshold;
        if (player.field834 == 1) select(far ? 4 : 3, 0);
        else if (far) select(1, 0);
        else if (player.field89E && player.field8AC < nearThreshold && !(roomFlags & 3)) select(2, nearDelay);
        else select(0, normalDelay);
        const float minimumX = minimum[0] + parameters[1];
        const float maximumX = (minimum[0] + extent[0]) - parameters[1];
        float x = (*player.position)[0] + leadX[playerIndex];
        if (x < minimumX) x = minimumX;
        else if (maximumX < x) x = maximumX;
        float y = minimum[1] + parameters[2];
        if (player.field834 == 1) y = (parameters[2] + minimum[1]) + extent[1];
        else if (heightThreshold < (*player.position)[1] - y) y = (*player.position)[1] + parameters[2];
        const float back = minimum[2] + extent[2];
        float z = back - inset;
        if ((*player.position)[2] < z) z = (*player.position)[2];
        const std::array<float, 3> target{x, y, z};
        float eyeZ = back + parameters[0];
        if ((*player.position)[2] < back - inset) eyeZ -= (back - inset) - (*player.position)[2];
        const float height = player.field89C == 1 ? reactionHeight : 0;
        const float depth = player.field89C == 1 ? reactionDepth : 0;
        const std::array<float, 3> eye{x, (height + camera.target[1]) + parameters[3], depth + eyeZ};
        approach(camera.target, target, followRate);
        approach(camera.position, eye, followRate);
    }
    if (camera.target == camera.position) camera.target[2] = camera.position[2] + coincidentOffset;
}
