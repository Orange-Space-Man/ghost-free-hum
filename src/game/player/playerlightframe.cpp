#include "game/player/playerlightframe.h"
#include "game/player/playerorientationupdate.h"
#include "resources/dolresources.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    template<class T> T constant(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, sizeof(T), bytes, error)) throw std::runtime_error(error);
        std::reverse(bytes.begin(), bytes.end());
        T result;
        std::memcpy(&result, bytes.data(), sizeof(result));
        return result;
    }

    constexpr unsigned halfAngleAddress = 0x804dab90, angleUnitsAddress = 0x804dab94;
    constexpr unsigned degreesAddress = 0x804dab98, downwardLimitAddress = 0x804dab9c;
    constexpr unsigned downwardOffsetAddress = 0x804d2084, downwardScaleAddress = 0x804d2088;
    constexpr unsigned brightnessAddress = 0x804d208c, modelAxesAddress = 0x804d2090;
    constexpr unsigned coneSineAddress = 0x80218870, coneCosineAddress = 0x80219870;
    constexpr unsigned normalizationThresholdAddress = 0x804da8fc;
    constexpr unsigned beamAngleAddress = 0x804d2be4, beamDistanceAddress = 0x804d2be8;
    constexpr unsigned beamColorAddress = 0x804d2bec, alternateBeamColorAddress = 0x804d2bf0;

    void normalize(std::array<float, 3>& value, float epsilon) {
        const float magnitude = std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
        if (magnitude >= epsilon) {
            const float factor = float(1.0 / double(magnitude));
            for (auto& component : value) component *= factor;
        } else value.fill(0);
    }
}

playeractor::LightFrame::LightFrame(const std::vector<std::uint8_t>& executable) :
    half(constant<float>(executable, halfAngleAddress)), angleUnits(constant<float>(executable, angleUnitsAddress)),
    degrees(constant<float>(executable, degreesAddress)), downwardOffset(constant<float>(executable, downwardOffsetAddress)),
    downwardScale(constant<float>(executable, downwardScaleAddress)), downwardLimit(constant<float>(executable, downwardLimitAddress)),
    coneRatio(constant<float>(executable, coneSineAddress) / constant<float>(executable, coneCosineAddress)),
    epsilon(constant<float>(executable, normalizationThresholdAddress)),
    brightness(static_cast<std::uint16_t>(constant<std::uint32_t>(executable, brightnessAddress))),
    beamAngle(constant<float>(executable, beamAngleAddress)), beamDistance(constant<float>(executable, beamDistanceAddress)),
    colors{constant<std::uint32_t>(executable, beamColorAddress), constant<std::uint32_t>(executable, alternateBeamColorAddress)} {
    shadowHeight = constant<float>(executable, 0x804d2bdc);
    shadowDistance = constant<float>(executable, 0x804d2be0);
    alternateShadowScale = constant<float>(executable, 0x804db528);
    shadowVelocityScale = constant<float>(executable, 0x804db52c);
    shadowColor = constant<std::uint32_t>(executable, 0x80355148);
    shadowReference = static_cast<std::uint16_t>(constant<std::uint32_t>(executable, 0x8035514c));
    shadowBrightness = static_cast<std::uint16_t>(constant<std::uint32_t>(executable, 0x80355150));
    std::string error;
    if (!trig.read(executable, error)) throw std::runtime_error(error);
    std::vector<std::uint8_t> bytes;
    if (!dolresources::read(executable, modelAxesAddress, axes.size(), bytes, error)) throw std::runtime_error(error);
    std::copy(bytes.begin(), bytes.end(), axes.begin());
    for (unsigned i = 0; i < 3; ++i)
        if (axes[i * 2 + 1] < 'x' || axes[i * 2 + 1] > 'z')
            throw std::runtime_error("Invalid original flashlight model-axis mapping.");
}

void playeractor::LightFrame::update(Player& player, bool roomCleared, std::uint32_t frame, const FinishFrame& finish,
    const LightServices& services, const actormodel::InitialPose& pose) const {
    const auto& state = player.state;
    const auto& components = *player.components;
    const auto& controls = *components.controls;
    const bool enabled = state.field1180 == 5 && components.actions->query74968() && !state.flags1330[3] &&
        !state.flags1040[2] && !state.field1058 && components.attached->field2C0 != 1 && (state.flags792 & 12) &&
        !roomCleared && state.field119E && !state.flags1330[2];
    const float scale = finish.bodyScale(state);
    const auto& weapon = *components.weapon->first;
    LightInput input{enabled, state.flags1140[2] != 0, controls.settings.fields[28] == 0 || controls.flags24[4] != 0,
        controls.flags24[4] != 0, weapon.position, weapon.field610, beamAngle, float(double(beamDistance) * scale), scale,
        colors[state.flags1140[3] ? 1 : 0], frame, *state.camera};
    run(player, input, services, pose);
    if (state.field7A4 && state.field7A4->actorType == 7) state.field7A4->selectTexture(enabled ? 1 : 0, 0);
}

std::uint32_t playeractor::LightFrame::angle(float value) const {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(float(double(angleUnits) * value) / degrees));
}

void playeractor::LightFrame::run(Player& player, const LightInput& input, const LightServices& services,
    const actormodel::InitialPose& pose) const {
    auto& light = *player.components->light;
    if (!input.enabled) {
        if (light.state) services.sound(0x84c, player.position);
        light.state = 0;
        return;
    }
    const float halfAngle = half * input.angle;
    if (!light.state) services.sound(0x837, player.position);
    light.state = 1;
    light.owner = &player;
    light.camera = &input.camera;
    light.fieldC = input.position;
    light.field24 = input.direction;
    const auto index = angle(halfAngle);
    const float cosine = trig.cosineAngle(index), tangent = trig.sineAngle(index) / cosine;
    light.field3C = cosine;
    light.field40 = input.distance * tangent;
    light.field44 = {cosine, light.field40};
    light.field4C = input.distance;
    light.field50 = input.distance * input.distance;
    if (input.illuminate && !input.scanner) services.illuminate(player, light);
    light.field18 = input.position;
    light.field30 = input.direction;
    if (input.scanner) {
        light.field18 = input.camera.position;
        for (unsigned i = 0; i < 3; ++i) light.field30[i] = input.camera.target[i] - input.camera.position[i];
        normalize(light.field30, epsilon);
    } else if (input.direction[1] < 0) {
        light.field18[1] -= input.direction[1] * downwardOffset;
        const float y = -input.direction[1];
        float inclination = float(pose.angle(y, std::sqrt(1.0f - float(double(y) * y)))) * (360.0f / 65536.0f);
        if (inclination >= 180) inclination -= 360;
        const float corrected = std::min(float(double(downwardScale) * inclination), downwardLimit);
        light.field30[1] = -trig.sineAngle(angle(corrected));
        normalize(light.field30, epsilon);
    }
    const auto color = input.charged ? 0xff1f1fffu : input.color;
    if (input.charged) services.pulse.start(input.position, color, 500, 0x5c, 10);
    modellighting::RoomLight room;
    room.position = light.field18;
    room.direction = light.field30;
    for (unsigned i = 0; i < 4; ++i) room.color[i] = static_cast<std::uint8_t>(color >> (24 - i * 8));
    room.cutoff = static_cast<std::uint8_t>(static_cast<int>(halfAngle));
    room.referenceDistance = static_cast<std::uint16_t>(static_cast<int>(input.distance));
    room.brightnessPercent = brightness;
    services.lights.add(8, room);
    auto& animation = *services.models.animation(2);
    const auto duration = static_cast<std::uint16_t>(animation.frameCount);
    if (!duration) throw std::runtime_error("Flashlight texture animation has zero duration.");
    animation.frame = static_cast<float>(input.frame % duration);
    const float lateral = input.scale * (tangent / coneRatio);
    light.instance->scale = {input.scale, lateral, lateral};
    const auto basis = playerorientation::headingMatrix(input.direction, false);
    for (unsigned row = 0; row < 3; ++row) {
        for (unsigned column = 0; column < 3; ++column) {
            const float component = basis[row * 4 + axes[column * 2 + 1] - 'x'];
            light.instance->matrix[row * 4 + column] = axes[column * 2] == '-' ? -component : component;
        }
        light.instance->matrix[row * 4 + 3] = input.position[row];
    }
}

void playeractor::LightFrame::publishShadow(Player& player, roomlighting::Pool& lights, const FinishFrame& finish) const {
    auto& state = player.state;
    if (state.flags1330[2]) return;
    if (state.field1324 & 0x10) {
        modellighting::RoomLight light;
        for (unsigned i = 0; i < 4; ++i) light.color[i] = static_cast<std::uint8_t>(shadowColor >> (24 - i * 8));
        light.referenceDistance = shadowReference;
        light.brightnessPercent = shadowBrightness;
        if (state.fieldF34 == 0) {
            for (unsigned i = 0; i < 3; ++i) light.position[i] = -shadowDistance * state.direction[i];
            light.position[1] += state.flags1330[1] ? -shadowHeight : shadowHeight;
            for (unsigned i = 0; i < 3; ++i) light.position[i] += player.position[i];
            state.fieldFF0 = state.direction;
        } else {
            const float scale = finish.bodyScale(state);
            const float distance = alternateShadowScale * float(-double(shadowDistance) * scale);
            for (unsigned i = 0; i < 3; ++i) {
                light.position[i] = player.position[i] + distance * state.direction[i];
                light.direction[i] = player.position[i] - light.position[i];
            }
            light.direction[1] += float(double(shadowHeight) * scale);
            normalize(light.direction, epsilon);
            light.cutoff = 40;
            light.brightnessPercent = 99;
            state.fieldFF0 = light.direction;
        }
        lights.add(8, light);
        state.fieldFE4 = light.position;
    }
    if ((state.field1180 == 5 || state.field1180 == 1) && player.components->actions->query74968()) {
        const auto& weapon = *player.components->weapon->first;
        const auto& velocity = player.velocity;
        const float speed = std::sqrt((velocity[0] * velocity[0] + velocity[1] * velocity[1]) + velocity[2] * velocity[2]);
        const float offset = float(double(shadowVelocityScale) * speed);
        for (unsigned i = 0; i < 3; ++i) state.fieldFE4[i] = weapon.position[i] + offset * weapon.field610[i];
        state.fieldFF0 = weapon.field610;
    }
}
