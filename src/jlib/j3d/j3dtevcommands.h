#pragma once

#include "jlib/j3d/j3dmaterialfactory.h"

namespace j3dmodel {
    enum class TevOperation {
        StageCount,
        Texture,
        Order,
        ColorRegister,
        ConstantRegister,
        ColorInputs,
        ColorOperation,
        AlphaInputs,
        AlphaOperation,
        Swap,
        Indirect,
        ConstantColor,
        ConstantAlpha,
        SwapTable
    };

    struct TevCommand {
        TevOperation operation;
        unsigned index;
        std::array<int, 9> values{};
    };

    std::vector<TevCommand> buildTevCommands(const MaterialTevBlock& block);
}
