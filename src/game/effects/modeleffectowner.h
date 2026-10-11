#pragma once

#include "game/actors/characterownership.h"
#include "jlib/j3d/j3dmodelinstance.h"

namespace modeleffects {
    class Owner : public characterownership::Resource {
    public:
        explicit Owner(const std::array<unsigned, 3>& counts);
        virtual void reset();
        void release() override;
        int attachmentState() override;
        void setMatrix(const j3dmodel::Matrix& matrix) override;
        void setAttachmentPosition(const std::array<float, 3>& position, const std::array<float, 3>& vector) override;
        void setVisible(bool visible) override;
        void setRunning(bool running) override;
        virtual void setPosition(const std::array<float, 3>& position);
        void setScale(const std::array<float, 3>& scale);
        virtual void setFrame(double frame);
        std::unique_ptr<j3dmodel::ModelInstance> model;
        std::uint8_t flags = 6;
        std::int32_t frame = 0, elapsed = 0;
        std::array<std::vector<std::int32_t>, 3> frames;
    };
}
