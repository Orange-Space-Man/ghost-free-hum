#pragma once

#include "game/startup/startupscene.h"
#include "game/models/s3dmodel.h"
#include "game/models/s3denvironment.h"
#include <windows.h>
#include <filesystem>
#include <d3d11.h>

namespace gamerenderer {
    void open(HWND window, const std::vector<roommaterials::Texture>& textures);
    void setInterfaceTextures(const std::vector<roommaterials::Texture>& textures);
    void cacheScene(const std::string& name);
    bool useScene(const std::string& name);
    void resize(int width, int height);
    void drawInterface(const std::vector<startupscene::Picture>& pictures, bool present);
    void capture(const std::filesystem::path& path);
    void waitForFrame();
    ID3D11Device* modelDevice();
    ID3D11DeviceContext* modelContext();
    void bindSceneTarget(bool clear);
    std::vector<std::size_t> loadPreviews(const std::vector<s3dmodel::Model>& models, unsigned width = 148, unsigned height = 116);
    void drawPreview(std::size_t index, const s3dmodel::Model& model, const std::vector<s3dmodel::Batch>& batches,
        const s3denvironment::Lighting& lighting, const s3denvironment::Camera* camera = nullptr, bool clear = true, unsigned passes = 3, bool receiver = false);
    void close();
}
