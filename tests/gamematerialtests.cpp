#include "platform/d3d11/gamerenderer.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    std::array<unsigned char, 3> center(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        BITMAPFILEHEADER header{};
        BITMAPINFOHEADER info{};
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        file.read(reinterpret_cast<char*>(&info), sizeof(info));
        require(info.biWidth > 0 && info.biHeight > 0 && info.biBitCount == 24, "Invalid capture.");
        const auto stride = (info.biWidth * 3 + 3) & ~3;
        file.seekg(header.bfOffBits + (info.biHeight / 2) * stride + (info.biWidth / 2) * 3);
        std::array<unsigned char, 3> color{};
        file.read(reinterpret_cast<char*>(color.data()), color.size());
        require(bool(file), "Cannot read capture pixel.");
        return color;
    }
}

int main() {
    HWND window = nullptr;
    const auto path = std::filesystem::temp_directory_path() / (L"polterport-material-test-" + std::to_wstring(GetTickCount64()) + L".bmp");
    try {
        window = CreateWindowExW(0, L"STATIC", L"Material test", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr, nullptr, nullptr);
        require(window != nullptr, "Cannot create test window.");
        gamerenderer::open(window, {});
        s3dmodel::Model model;
        model.textures.push_back({1, 1, {255, 255, 255, 255}});
        model.materials.resize(2);
        model.materials[0].vertexAlpha = true;
        model.materials[1].color = {1, 0, 0, 1};
        for (unsigned i = 0; i < 2; ++i) {
            s3dmodel::Batch batch;
            batch.material = i;
            for (const auto& position : std::array<std::array<float, 3>, 3>{{{-2,-2,-float(i)}, {2,-2,-float(i)}, {0,2,-float(i)}}}) {
                s3dmodel::Vertex vertex;
                vertex.position = position;
                vertex.color[3] = i == 0 ? 0.0f : 1.0f;
                batch.vertices.push_back(vertex);
            }
            model.batches.push_back(batch);
        }
        gamerenderer::loadPreviews({model}, 640, 480);
        s3denvironment::Camera camera;
        camera.position = {0,0,5};
        camera.target = {0,0,0};
        camera.nearPlane = 1;
        camera.farPlane = 100;
        camera.fieldOfView = 60;
        camera.aspect = 4.0f / 3;
        startupscene::Picture picture;
        picture.width = 640;
        picture.height = 480;
        for (unsigned mode = 0; mode < 3; ++mode) {
            model.materials[0].mode = mode;
            gamerenderer::drawPreview(0, model, model.batches, {}, &camera);
            gamerenderer::drawInterface({picture}, false);
            gamerenderer::capture(path);
            const auto pixel = center(path);
            require(pixel[0] == 0 && pixel[1] == 0 && pixel[2] == (mode == 0 ? 0 : 255),
                "Opaque depth must survive alpha rejection; cutout/translucent depth must not.");
        }
        gamerenderer::close();
        DestroyWindow(window);
        std::filesystem::remove(path);
        std::cout << "Material depth ordering checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        gamerenderer::close();
        if (window) DestroyWindow(window);
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
