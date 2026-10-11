#include "game/actors/actorposeblend.h"
#include "game/actors/actoreffectstate.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    constexpr actormodel::Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
}

int main() {
    try {
        auto resource = std::make_shared<actormodel::Resource>();
        auto& bytes = resource->bytes;
        bytes.resize(320);
        const auto half = [&](unsigned offset, unsigned value) {
            bytes.at(offset) = static_cast<std::uint8_t>(value >> 8);
            bytes.at(offset + 1) = static_cast<std::uint8_t>(value);
        };
        const auto word = [&](unsigned offset, unsigned value) {
            half(offset, value >> 16);
            half(offset + 2, value);
        };
        const auto number = [&](unsigned offset, float value) {
            std::uint32_t bits;
            std::memcpy(&bits, &value, 4);
            word(offset, bits);
        };
        half(8, 3);
        half(12, 1);
        word(48, 80);
        half(80, 0); half(82, 1); half(84, 2);
        half(96, 1); half(112, 2);
        word(56, 128); word(60, 272); word(64, 280); word(68, 284);
        for (unsigned joint = 0; joint < 3; ++joint)
            for (unsigned component = 0; component < 12; ++component) number(128 + joint * 48 + component * 4, identity[component]);
        number(272, 0.25f); number(276, 0.75f);
        half(280, 0); half(282, 1); bytes[284] = 2;
        actormodel::Instance instance(resource, 0);
        actoreffects::State effect;
        instance.attachment(0).object = &effect;
        effect.flags = 1;
        effect.field74 = 2;
        effect.field76 = 4;
        instance.blend = 0.25f;
        for (unsigned joint = 0; joint < 3; ++joint) instance.matrix(0, joint)[3] = 8;
        actormodel::blendPose(instance);
        require(instance.matrix(0, 0)[3] == 4 && instance.matrix(0, 1)[3] == 4 && instance.matrix(0, 2)[3] == 6,
            "Attachment blend overrides propagate to children while siblings retain the incoming weight.");
        require(effect.field74 == 1 && instance.matrix(3, 1) == instance.matrix(0, 1),
            "Blend clocks decrement once and publish the completed pose for the next frame.");
        effect.field74 = effect.field76 = 0;
        for (unsigned joint = 0; joint < 3; ++joint) instance.matrix(0, joint)[3] = 8;
        actormodel::blendPose(instance);
        require(instance.matrix(0, 0)[3] == 8 && instance.matrix(0, 1)[3] == 8 && instance.matrix(0, 2)[3] == 7.5f,
            "An active zero-duration override suppresses inherited blending without affecting siblings.");
        instance.matrix(0, 0)[3] = 4;
        instance.matrix(0, 1)[3] = 8;
        actormodel::updateEnvelopes(instance);
        require(instance.matrix(0, 3)[0] == 1 && instance.matrix(0, 3)[3] == 7,
            "Skin envelopes consume the blended joint bank with original influence order and inverse bind matrices.");
        auto first = identity, second = identity, output = identity;
        first[0] = first[5] = first[10] = 2;
        second[0] = second[5] = second[10] = 6;
        second[3] = 12;
        actormodel::blendTransform(0.25, first, second, output);
        require(output[0] == 3 && output[5] == 3 && output[10] == 3 && output[3] == 3,
            "Matrix blending interpolates axis lengths independently from normalized orientation.");
        second = {0, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, 0};
        actormodel::blendTransform(0.5, identity, second, output);
        require(std::abs(output[0] - std::sqrt(0.5f)) < 0.00001f && std::abs(output[2] - output[0]) < 0.00001f &&
            output[8] < 0 && output[10] > 0, "Pose blending retains the original axis and handedness reconstruction.");
        std::cout << "Actor pose blending verified.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
