#include "platform/d3d11/modelrenderstate.h"
#include "game/models/modeldrawtraversal.h"

#include <stdexcept>
#include <utility>

namespace {
    D3D11_BLEND factor(unsigned value, bool source, bool alpha) {
        if (value > 7) throw std::runtime_error("Unsupported original blend factor.");
        const D3D11_BLEND factors[] = {D3D11_BLEND_ZERO, D3D11_BLEND_ONE,
            source ? D3D11_BLEND_DEST_COLOR : D3D11_BLEND_SRC_COLOR,
            source ? D3D11_BLEND_INV_DEST_COLOR : D3D11_BLEND_INV_SRC_COLOR,
            D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA};
        if (alpha && value == 2) return source ? D3D11_BLEND_DEST_ALPHA : D3D11_BLEND_SRC_ALPHA;
        if (alpha && value == 3) return source ? D3D11_BLEND_INV_DEST_ALPHA : D3D11_BLEND_INV_SRC_ALPHA;
        return factors[value];
    }
}

void modelrenderer::initializeModelState(PipelineState& state) {
    state.alpha = {4, 0, 1, 4, 0};
    state.depthEnabled = state.depthWrite = true;
    state.depthComparison = 3;
    state.earlyDepth = false;
    state.cull = 2;
    state.blendEnabled = true;
    state.subtract = false;
    state.sourceBlend = 4;
    state.destinationBlend = 5;
}

void modelrenderer::selectModelPass(PipelineState& state, unsigned pass) {
    state.earlyDepth = (pass & 255) == 0;
}

void modelrenderer::selectBatchCull(PipelineState& state, std::uint8_t batchCull, std::uint8_t mode) {
    auto value = static_cast<std::uint8_t>(state.cull);
    if (modeldraw::cullMode(batchCull, mode, value)) state.cull = value;
}

void modelrenderer::RenderState::load(ID3D11Device* device, const PipelineState& state) {
    if (!device || state.depthComparison > 7 || state.cull > 3 || state.alpha.first > 7 || state.alpha.second > 7 ||
        state.alpha.operation > 3 || state.alpha.firstReference > 255 || state.alpha.secondReference > 255 ||
        state.sourceBlend > 7 || state.destinationBlend > 7)
        throw std::runtime_error("Invalid original model raster state.");
    RenderState result;
    result.owner = device;
    result.visible = state.cull != 3;
    D3D11_DEPTH_STENCIL_DESC depthDescription{};
    depthDescription.DepthEnable = state.depthEnabled;
    depthDescription.DepthWriteMask = state.depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDescription.DepthFunc = static_cast<D3D11_COMPARISON_FUNC>(state.depthComparison + 1);
    if (FAILED(device->CreateDepthStencilState(&depthDescription, result.depth.GetAddressOf())))
        throw std::runtime_error("Cannot create model depth state.");
    D3D11_BLEND_DESC blendDescription{};
    auto& target = blendDescription.RenderTarget[0];
    const auto sourceFactor = !state.targetHasAlpha && state.sourceBlend >= 6 ? (state.sourceBlend == 6 ? 1U : 0U) : state.sourceBlend;
    const auto destinationFactor = !state.targetHasAlpha && state.destinationBlend >= 6 ? (state.destinationBlend == 6 ? 1U : 0U) : state.destinationBlend;
    target.BlendEnable = state.blendEnabled || state.subtract;
    target.SrcBlend = state.subtract ? D3D11_BLEND_ONE : factor(sourceFactor, true, false);
    target.DestBlend = state.subtract ? D3D11_BLEND_ONE : factor(destinationFactor, false, false);
    target.SrcBlendAlpha = state.subtract ? D3D11_BLEND_ONE : factor(sourceFactor, true, true);
    target.DestBlendAlpha = state.subtract ? D3D11_BLEND_ONE : factor(destinationFactor, false, true);
    target.BlendOp = target.BlendOpAlpha = state.subtract ? D3D11_BLEND_OP_REV_SUBTRACT : D3D11_BLEND_OP_ADD;
    target.RenderTargetWriteMask = (state.colorWrite ? 7 : 0) | (state.alphaWrite && state.targetHasAlpha ? 8 : 0);
    if (FAILED(device->CreateBlendState(&blendDescription, result.blend.GetAddressOf())))
        throw std::runtime_error("Cannot create model blend state.");
    D3D11_RASTERIZER_DESC rasterDescription{};
    rasterDescription.FillMode = D3D11_FILL_SOLID;
    rasterDescription.CullMode = state.cull == 1 ? D3D11_CULL_FRONT : state.cull == 2 ? D3D11_CULL_BACK : D3D11_CULL_NONE;
    rasterDescription.FrontCounterClockwise = false;
    rasterDescription.DepthClipEnable = true;
    rasterDescription.ScissorEnable = state.scissor;
    if (FAILED(device->CreateRasterizerState(&rasterDescription, result.raster.GetAddressOf())))
        throw std::runtime_error("Cannot create model cull state.");
    *this = std::move(result);
}

void modelrenderer::RenderState::bind(ID3D11DeviceContext* context) const {
    if (!context || !owner) throw std::runtime_error("Model raster state is not ready.");
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());
    if (device.Get() != owner.Get()) throw std::runtime_error("Model raster context belongs to another device.");
    context->OMSetDepthStencilState(depth.Get(), 0);
    context->OMSetBlendState(blend.Get(), nullptr, 0xffffffff);
    context->RSSetState(raster.Get());
}

bool modelrenderer::RenderState::draws() const { return visible; }
