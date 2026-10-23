#pragma once

#include "jlib/j3d/j3dmodelinstance.h"
#include <functional>

namespace j3dmodel {
    class DrawBuffer {
    public:
        explicit DrawBuffer(unsigned capacity);
        void clear();
        bool entryMaterial(MaterialPacket& packet, std::uint32_t textureAddress);
        bool entryDepth(MaterialPacket& packet, const Matrix& camera);
        bool entryUnsorted(MaterialPacket& packet);
        bool entryCallbackMaterial(MaterialPacket& packet);
        bool entryCallbackShape(MaterialPacket& packet);
        void visit(const std::function<void(MaterialPacket&)>& draw) const;
        std::vector<MaterialPacket*> packets;
        std::uint32_t drawType = 0, sortType = 0;
        float nearPlane = 1, farPlane = 10000, interval;
        Matrix* matrix = nullptr;
        MaterialPacket* callback = nullptr;
    };
}
