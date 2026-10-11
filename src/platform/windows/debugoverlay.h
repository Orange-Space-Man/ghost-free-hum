#pragma once

#include <string>
#include <windows.h>
#include <d3d11.h>

namespace debugoverlay {
    void open(HWND window, ID3D11Device* device, ID3D11DeviceContext* context);
    void close();
    bool handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    bool isVisible();
    void toggle();
    void setScene(const std::string& information);
    void draw();
}
