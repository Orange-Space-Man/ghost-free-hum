#include "jlib/j3d/j3dmaterialstate.h"

#include <algorithm>
#include <stdexcept>

namespace {
    int signedColor(int value) {
        value &= 2047;
        return value < 1024 ? value : value - 2048;
    }
}

void j3dmodel::TevState::apply(const TevCommand& command) {
    const auto& values = command.values;
    const auto index = command.index;
    switch (command.operation) {
    case TevOperation::StageCount:
        if (values[0] < 1 || values[0] > 16) throw std::runtime_error("Invalid J3D combiner stage count.");
        count = values[0];
        return;
    case TevOperation::Texture:
        if (values[0] < 0 || values[0] >= 65535) throw std::runtime_error("Invalid J3D texture resource index.");
        textures.at(index) = values[0];
        return;
    case TevOperation::ColorRegister:
        if (index == 4) {
            overflowColorWrites[0] = (values[0] & 2047U) | ((values[3] & 2047U) << 12);
            overflowColorWrites[1] = (values[2] & 2047U) | ((values[1] & 2047U) << 12);
        } else {
            auto& color = colors.at(index);
            for (unsigned i = 0; i < 4; ++i) color[i] = signedColor(values[i]);
        }
        return;
    case TevOperation::ConstantRegister:
        for (unsigned i = 0; i < 4; ++i) constants.at(index)[i] = values[i] & 255;
        return;
    case TevOperation::SwapTable:
        std::copy_n(values.begin(), 4, swaps.at(index).begin());
        return;
    default:
        break;
    }
    auto& stage = stages.at(index);
    switch (command.operation) {
    case TevOperation::Order: std::copy_n(values.begin(), 3, stage.order.begin()); break;
    case TevOperation::ColorInputs: std::copy_n(values.begin(), 4, stage.colorInputs.begin()); break;
    case TevOperation::AlphaInputs: std::copy_n(values.begin(), 4, stage.alphaInputs.begin()); break;
    case TevOperation::ColorOperation: std::copy_n(values.begin(), 5, stage.colorOperation.begin()); break;
    case TevOperation::AlphaOperation: std::copy_n(values.begin(), 5, stage.alphaOperation.begin()); break;
    case TevOperation::Swap: std::copy_n(values.begin(), 2, stage.swap.begin()); break;
    case TevOperation::Indirect: stage.indirect = values; break;
    case TevOperation::ConstantColor: stage.colorSelection = values[0]; break;
    case TevOperation::ConstantAlpha: stage.alphaSelection = values[0]; break;
    default: throw std::runtime_error("Unknown J3D combiner command.");
    }
}

void j3dmodel::MaterialState::tev(const TevCommand& command) { combiner.apply(command); }
void j3dmodel::MaterialState::indirectCount(unsigned count) {
    if (count > 4) throw std::runtime_error("Invalid J3D indirect stage count.");
    indirectStages = count;
}
void j3dmodel::MaterialState::indirectMatrix(unsigned index, const std::array<float, 6>& matrix, int exponent) {
    indirectMatrices.at(index - 1) = {matrix, exponent};
}
void j3dmodel::MaterialState::indirectScale(unsigned index, const std::array<std::uint8_t, 2>& scale) { indirectScales.at(index) = scale; }
void j3dmodel::MaterialState::indirectOrder(unsigned index, const std::array<std::uint8_t, 2>& order) { indirectOrders.at(index) = order; }
void j3dmodel::MaterialState::pixel(const MaterialPixelBlock& block) { loadPixelState(block, pixels); }
void j3dmodel::MaterialState::materialColor(unsigned index, const std::array<std::uint8_t, 4>& color) { materialColors.at(index) = color; }
void j3dmodel::MaterialState::ambientColor(unsigned index, const std::array<std::uint8_t, 4>& color) { ambientColors.at(index) = color; }
void j3dmodel::MaterialState::channelCount(unsigned count) {
    if (count > 2) throw std::runtime_error("Invalid J3D color channel count.");
    colorChannels = count;
}
void j3dmodel::MaterialState::channel(unsigned index, const ChannelControl& control) { channels.at(index) = control; }
void j3dmodel::MaterialState::light(unsigned index, const MaterialLight& value) {
    auto converted = value;
    for (auto& component : converted.direction) component = -component;
    lights.at(index) = converted;
}
void j3dmodel::MaterialState::cull(unsigned value) {
    if (value > 3) throw std::runtime_error("Invalid J3D cull mode.");
    cullMode = value;
}
void j3dmodel::MaterialState::textureCount(unsigned count) {
    if (count > 8) throw std::runtime_error("Invalid J3D texture coordinate count.");
    textureCoordinates = count;
}
void j3dmodel::MaterialState::textureCoordinate(unsigned index, const std::array<std::uint8_t, 3>& info,
    unsigned normalize, unsigned postMatrix) {
    coordinates.at(index) = {info, normalize, postMatrix};
}
void j3dmodel::MaterialState::textureMatrix(unsigned index, const std::array<float, 12>& matrix, unsigned projection) {
    if (projection > 1) throw std::runtime_error("Invalid J3D texture projection.");
    auto& rows = index < 64 ? matrixRows : postMatrixRows;
    const unsigned first = index < 64 ? index : index - 64;
    const unsigned count = projection == 1 ? 2 : 3;
    if (first > rows.size() - count) throw std::runtime_error("J3D texture matrix exceeds XF storage.");
    for (unsigned row = 0; row < count; ++row) std::copy_n(matrix.begin() + row * 4, 4, rows[first + row].begin());
}
void j3dmodel::MaterialState::materialMode(std::uint32_t value) { mode = value; }
void j3dmodel::MaterialState::normalScale(const std::array<std::uint8_t, 16>* scale) { nbtScale = scale; }
