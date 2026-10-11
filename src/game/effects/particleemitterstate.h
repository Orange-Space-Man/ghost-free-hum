#pragma once

#include "game/effects/particleparameters.h"
#include "game/effects/particlepoolstate.h"
#include "game/actors/characterownership.h"

namespace particles {
    using Resource = std::shared_ptr<const std::vector<std::uint8_t>>;

    class EmitterCallback {
    public:
        virtual ~EmitterCallback() = default;
        virtual void advance(Emitter& emitter) = 0;
    };

    struct DrawState {
        void load(Resource source, unsigned offset, Resource texture, bool extended);
        Resource source, texture, alternateTexture;
        unsigned sourceOffset = 0;
        std::array<unsigned, 3> arrays{};
        std::array<std::uint8_t, 3> fields18{};
        std::uint8_t count = 0;
        std::array<std::uint32_t, 2> fields2C{};
        std::array<std::uint8_t, 2> fields34{};
        Ring<Node>* nodes = nullptr;
        Ring<ShortNode>* shortNodes = nullptr;
    };

    struct Emitter : characterownership::Resource {
        Emitter();
        void reset();
        void clearParticles();
        void reload(particles::Resource source, particles::Resource texture, particles::Resource alternateTexture = {});
        void release() override;
        int attachmentState() override;
        void setAttachmentPosition(const std::array<float, 3>& position, const std::array<float, 3>& vector) override;
        void setMatrix(const std::array<float, 12>& matrix) override;
        void setVisible(bool visible) override;
        void setRunning(bool running) override;
        void stop();
        bool inactive() const;
        bool empty() const;
        Ring<Node> nodes;
        Ring<ShortNode> shortNodes;
        Ring<Node>* nodePool = nullptr;
        Ring<ShortNode>* shortNodePool = nullptr;
        float fieldB8 = 0;
        std::uint16_t fieldBC = 0, flags = 0;
        std::uint8_t fieldC0 = 0;
        EmitterCallback* fieldC4;
        NodeCallback* fieldC8;
        Parameters parameters;
        Emitter* next;
        Emitter* previous;
        std::array<float, 3> position{}, field220{}, field22C{};
        DrawState ordinaryDraw, extendedDraw;
        DrawState* draw = nullptr;
    };

    class Simulation {
    public:
        virtual ~Simulation() = default;
        virtual void callback(Emitter& emitter) = 0;
        virtual void emit(Emitter& emitter) = 0;
        virtual void advance(Emitter& emitter) = 0;
    };

    class EmitterPool {
    public:
        EmitterPool(unsigned emitters, unsigned nodes, unsigned shortNodes);
        Emitter* create(Resource parameters, Resource texture, Resource alternateTexture = {});
        void update(Simulation& simulation);
        void enterAlternate();
        void leaveAlternate();
        Pool<Emitter> emitters;
        Pool<Node> nodes;
        Pool<ShortNode> shortNodes;
        std::array<Ring<Emitter>, 3> active;
        Ring<Emitter>* destination;
        std::uint32_t attempts = 0;
        std::array<std::uint32_t, 29> attemptHistory{};
        std::uint8_t flags = 0;
    };
}
