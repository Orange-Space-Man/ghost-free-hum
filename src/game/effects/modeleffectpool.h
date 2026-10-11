#pragma once

#include "game/effects/modeleffectowner.h"
#include "game/effects/particleeffectfactory.h"
#include "game/world/roomlocation.h"
#include "game/world/roomtraversalstate.h"

#include <string>

namespace modeleffects {
    struct Definition {
        std::int32_t lifetime = 0;
        bool transformAnimation = false;
        std::array<unsigned, 3> counts{};
        unsigned resourceType = 0, type = 0, capacity = 0;
        std::vector<std::string> names;
    };

    std::vector<Definition> readDefinitions(const std::vector<std::uint8_t>& executable, unsigned map);

    class Pool {
    public:
        explicit Pool(Definition definition);
        virtual ~Pool() = default;
        virtual void load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults);
        virtual Owner* available();
        virtual Owner* available(const j3dmodel::Matrix& matrix);
        virtual void advance(Owner& owner, bool visible);
        virtual void update(const roomlocation::Rooms& rooms, const roomtraversal::State& visibility);
        const Definition definition;
        std::vector<std::unique_ptr<Owner>> owners;
        std::shared_ptr<j3dmodel::Animation> transform;
        std::array<std::vector<std::shared_ptr<j3dmodel::Animation>>, 3> animations;

    protected:
        Pool(Definition definition, std::nullptr_t);
    };

    class Factory {
    public:
        void clear();
        void add(Pool& pool);
        Owner* create(unsigned type, const j3dmodel::Matrix& matrix);
        Owner* create(unsigned type, const std::array<float, 3>& position);

    private:
        std::array<Pool*, 64> pools{};
        unsigned count = 0;
    };
}
