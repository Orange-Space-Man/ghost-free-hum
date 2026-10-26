#pragma once

#include "jlib/j3d/j3dmaterialloads.h"
#include "jlib/j3d/j3dmodelinstance.h"

namespace j3dmodel {
    class MaterialDisplayList {
    public:
        explicit MaterialDisplayList(const Material& material);
        ~MaterialDisplayList();
        MaterialDisplayList(const MaterialDisplayList&) = delete;
        MaterialDisplayList& operator=(const MaterialDisplayList&) = delete;
        void replay(MaterialLoadTarget& target) const;
        std::size_t commandCount() const;

    private:
        struct State;
        std::unique_ptr<State> state;
    };

    class PacketDrawTarget {
    public:
        virtual ~PacketDrawTarget() = default;
        virtual void select(MaterialPacket& packet) = 0;
        virtual void flush() = 0;
        virtual void shape(ShapePacket& packet) = 0;
    };

    void updateMaterialPacket(MaterialPacket& packet);
    void loadMaterialPacket(const MaterialPacket& packet, std::uint32_t systemFlags, MaterialLoadTarget& target);
    void drawMaterialPacket(MaterialPacket& packet, std::uint32_t systemFlags, std::uint32_t drawMode,
        MaterialLoadTarget& materialTarget, PacketDrawTarget& drawTarget);
}
