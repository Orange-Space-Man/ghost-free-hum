#pragma once

#include "jlib/j3d/j3dmodelinstance.h"

namespace j3dmodel {
    class DrawBuffer {
    public:
        explicit DrawBuffer(unsigned capacity);
        void clear();
        std::vector<MaterialPacket*> packets;
        std::uint32_t drawType = 0, sortType = 0;
        float nearPlane = 1, farPlane = 10000, interval;
        Matrix* matrix = nullptr;
        MaterialPacket* callback = nullptr;
    };
}
