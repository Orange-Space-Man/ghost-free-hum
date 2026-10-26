#include "jlib/j3d/j3dmaterialstate.h"
#include "platform/d3d11/j3dcombinerrenderer.h"

#include <iostream>
#include <stdexcept>

namespace {
    void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}

int main() {
    try {
        j3dmodel::MaterialState state;
        state.tev({j3dmodel::TevOperation::ColorRegister, 0, {1, 2, 3, 4}});
        state.tev({j3dmodel::TevOperation::ColorRegister, 1, {-1024, 1023, 2047, 2048}});
        check(state.combiner.colors[1] == std::array<int, 4>{-1024, 1023, -1, 0}, "Signed TEV colors retain the original eleven-bit register width.");
        state.tev({j3dmodel::TevOperation::ColorRegister, 4, {1, 2, 3, 4}});
        check(state.combiner.colors[0] == std::array<int, 4>{1, 2, 3, 4} &&
            state.combiner.overflowColorWrites[0] == (1U | (4U << 12)) &&
            state.combiner.overflowColorWrites[1] == (3U | (2U << 12)),
            "GX color ID four writes E8/E9 payloads without wrapping to the previous-color register.");
        std::array<float, 12> matrix{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        state.textureMatrix(30, matrix, 0);
        matrix.fill(17);
        state.textureMatrix(30, matrix, 1);
        check(state.matrixRows[30][0] == 17 && state.matrixRows[31][3] == 17 && state.matrixRows[32][0] == 9,
            "Two-row texture loads retain the previous third row.");
        state.textureMatrix(64, matrix, 0);
        check(state.postMatrixRows[0][0] == 17 && state.matrixRows[0][0] == 0,
            "Post matrices use a separate XF bank.");
        j3dmodel::MaterialLight light;
        light.direction = {1, -2, 3};
        state.light(2, light);
        light.direction.fill(99);
        check(state.lights[2]->direction == std::array<float, 3>{-1, 2, -3}, "Light loading negates the direction without normalization and snapshots the source.");
        state.ambientColor(1, {10, 20, 30, 40});
        state.cull(2);
        state.tev({j3dmodel::TevOperation::Texture, 7, {91}});
        state.tev({j3dmodel::TevOperation::ConstantColor, 0, {12}});
        j3dmodel::MaterialTevBlock block;
        block.capacity = block.count = 1;
        block.textures = {65535}; block.stages.resize(1);
        for (const auto& command : j3dmodel::buildTevCommands(block)) state.tev(command);
        j3dmodel::MaterialColorBlock color;
        j3dmodel::loadColorBlock(color, state);
        check(state.combiner.textures[7] == 91 && state.combiner.stages[0].colorSelection == 12 &&
            state.ambientColors[1] == std::array<std::uint8_t, 4>{10, 20, 30, 40} && state.cullMode == 2 && state.lights[2],
            "Omitted texture, constant, ambient, light and cull writes retain previous material state.");
        state.combiner.stages[0].order = {255, 255, 255};
        state.combiner.stages[0].colorOperation = {0, 0, 0, 1, 0};
        state.combiner.stages[0].alphaOperation = {0, 0, 0, 1, 0};
        state.combiner.stages[0].swap = {1, 2};
        state.combiner.swaps[1] = {3, 2, 1, 0};
        state.combiner.swaps[2] = {1, 2, 3, 0};
        auto program = j3drenderer::buildCombinerProgram(state);
        check(program.registers[0].rasterSwap == std::array<unsigned, 4>{3, 2, 1, 0} &&
            program.registers[0].textureSwap == std::array<unsigned, 4>{1, 2, 3, 0}, "Raster and texture swaps follow their distinct retained selectors.");
        state.combiner.stages[0].colorOperation[1] = 1;
        bool rejected = false;
        try { j3drenderer::buildCombinerProgram(state); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "Unsupported arithmetic must not silently render with a different equation.");
        std::cout << "Retained J3D material registers, matrix rows and light conversion verified.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
