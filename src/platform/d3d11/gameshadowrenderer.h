#pragma once

#include "game/models/s3dshadow.h"
#include <d3d11.h>

namespace gameshadowrenderer {
    void open(ID3D11Device* device, ID3D11DeviceContext* context);
    void reserve(std::size_t vertices);
    void begin(const s3dshadow::Projection& projection, float aspect);
    void draw(const std::vector<s3dmodel::Batch>& batches);
    void finish();
    void bind(bool receiver, float alpha = 1);
    void close();
}
