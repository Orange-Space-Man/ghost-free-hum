#pragma once

#include "game/actors/actormodelarchive.h"
#include "game/models/s3dmorph.h"
#include "game/models/s3denvironment.h"

namespace actormodel {
    using Matrix = std::array<float, 12>;

    struct Attachment {
        std::uint16_t field0 = 0, field2 = 0, field4 = 0;
        void* object = nullptr;
    };

    struct Channel {
        ResourceRef resource;
        float frame = 0;
    };

    struct Effect {
        float value = 0;
        ResourceRef field4, field8;
        ResourceRef decodedShapes, decodedKeys;
        s3dmorph::Animation animation;
    };

    class Instance {
    public:
        Instance(ResourceRef resource, unsigned capacity);
        unsigned capacity() const;
        Matrix& matrix(unsigned bank, unsigned index);
        const Matrix& matrix(unsigned bank, unsigned index) const;
        Attachment& attachment(unsigned index);
        void bindKey(ResourceRef resource);
        void resetCursors();
        ResourceRef model;
        ResourceRef key;
        std::uint32_t field0 = 0;
        float frame = 0, blend = 0;
        std::uint32_t color = 0xffffffff;
        std::array<Channel, 16> channels;
        std::array<Effect, 8> effects;
        ResourceRef field110;
        float materialFrame = 0;
        ResourceRef decodedMaterial;
        s3denvironment::MaterialAnimation materialAnimation;

    private:
        unsigned count;
        std::unique_ptr<Matrix[]> matrices;
        std::unique_ptr<Attachment[]> attachments;
    };
}
