#include "jlib/j3d/j3dtevcommands.h"

#include <algorithm>
#include <stdexcept>

namespace {
    using j3dmodel::TevCommand;
    using j3dmodel::TevOperation;

    void operation(std::vector<TevCommand>& commands, TevOperation kind, unsigned index, std::uint8_t bits) {
        const int mode = (bits & 3) == 3 ? ((bits >> 2) & 1) + ((bits >> 3) & 6) + 8 : ((bits >> 2) & 1);
        commands.push_back({kind, index, {mode, bits & 3, (bits >> 4) & 3, (bits >> 3) & 1, bits >> 6}});
    }

    void stage(std::vector<TevCommand>& commands, const j3dmodel::MaterialTevStage& stage, unsigned index) {
        const auto& r = stage.registers;
        commands.push_back({TevOperation::ColorInputs, index, {r[1] >> 4, r[1] & 15, r[2] >> 4, r[2] & 15}});
        operation(commands, TevOperation::ColorOperation, index, r[0]);
        commands.push_back({TevOperation::AlphaInputs, index, {r[5] >> 5, (r[5] >> 2) & 7,
            (r[6] >> 7) | ((r[5] & 3) << 1), (r[6] >> 4) & 7}});
        operation(commands, TevOperation::AlphaOperation, index, r[4]);
        commands.push_back({TevOperation::Swap, index, {r[6] & 3, (r[6] >> 2) & 3}});
        TevCommand indirect{TevOperation::Indirect, index};
        std::copy(stage.indirect.begin(), stage.indirect.end(), indirect.values.begin());
        commands.push_back(indirect);
    }
}

std::vector<j3dmodel::TevCommand> j3dmodel::buildTevCommands(const MaterialTevBlock& block) {
    if (block.capacity != 1 && block.capacity != 2 && block.capacity != 4 && block.capacity != 16)
        throw std::runtime_error("Unknown original J3D texture-combiner block.");
    if (block.count > block.capacity || block.stages.size() < block.capacity ||
        block.textures.size() < std::min(block.capacity, 8U))
        throw std::runtime_error("J3D texture-combiner block exceeds its owned storage.");
    std::vector<TevCommand> commands;
    if (block.capacity != 16) commands.push_back({TevOperation::StageCount, 0, {block.count}});
    for (unsigned i = 0; i < std::min(block.capacity, 8U); ++i)
        if (block.textures[i] != 65535) commands.push_back({TevOperation::Texture, i, {block.textures[i]}});
    if (block.capacity == 16) commands.push_back({TevOperation::StageCount, 0, {block.count}});
    for (unsigned i = 0; i < block.count; ++i) {
        const auto& order = block.stages[i].order;
        commands.push_back({TevOperation::Order, i, {order[0], order[1], order[2]}});
    }
    if (block.capacity > 1) {
        for (unsigned i = 0; i < 4; ++i) {
            TevCommand color{TevOperation::ColorRegister, i + 1};
            for (unsigned channel = 0; channel < 4; ++channel) {
                const auto value = block.colors[i][channel];
                color.values[channel] = value < 32768 ? int(value) : int(value) - 65536;
            }
            commands.push_back(color);
        }
        for (unsigned i = 0; i < 4; ++i) {
            TevCommand color{TevOperation::ConstantRegister, i};
            std::copy(block.constants[i].begin(), block.constants[i].end(), color.values.begin());
            commands.push_back(color);
        }
    }
    for (unsigned i = 0; i < block.count; ++i) stage(commands, block.stages[i], i);
    if (block.capacity > 1) {
        for (unsigned i = 0; i < block.count; ++i)
            if (block.stages[i].colorSelection != 255)
                commands.push_back({TevOperation::ConstantColor, i, {block.stages[i].colorSelection}});
        for (unsigned i = 0; i < block.count; ++i)
            if (block.stages[i].alphaSelection != 255)
                commands.push_back({TevOperation::ConstantAlpha, i, {block.stages[i].alphaSelection}});
        for (unsigned i = 0; i < 4; ++i) {
            const auto value = block.swaps[i];
            commands.push_back({TevOperation::SwapTable, i, {value >> 6, (value >> 4) & 3, (value >> 2) & 3, value & 3}});
        }
    }
    return commands;
}
