#pragma once

#include "jlib/jaudio/jaudioeffectadmission.h"

#include <functional>
#include <array>
#include <vector>

namespace jaudio {
    struct EffectListenerDistance {
        float x = 0, y = 0, z = 0, squared = 0;
        std::array<float, 3> previous{};
    };

    struct EffectRank {
        std::uint32_t score = 0;
        bool eligible = false;
    };

    EffectRank rankEffect(std::vector<EffectListenerDistance>& listeners, unsigned selectedListener,
        unsigned listenerCount, std::uint8_t priority, std::uint32_t metadata, float maximumDistance);

    std::array<float, 3> transformEffectPosition(const std::array<float, 12>& matrix, const std::array<float, 3>& position);
    EffectRank prepareEffectRank(std::vector<EffectListenerDistance>& listeners, const std::vector<std::array<float, 12>>& matrices,
        const std::array<float, 3>* position, unsigned selectedListener, unsigned listenerCount,
        std::uint8_t priority, std::uint32_t metadata, float maximumDistance);

    struct EffectCategoryServices {
        virtual ~EffectCategoryServices() = default;
        virtual EffectRank prepareRank(EffectRecord& record) = 0;
        virtual void release(EffectRecord& record) = 0;
    };

    void updateEffectCategory(EffectPool& pool, std::vector<EffectRecord*>& channels, EffectCategoryServices& services);

    class EffectSelection {
    public:
        explicit EffectSelection(unsigned channelCount);
        void consider(EffectRecord& record, std::uint32_t score);
        void assign(std::vector<EffectRecord*>& channels, const std::function<void(EffectRecord&)>& release);

    private:
        struct Candidate {
            std::uint8_t state = 255;
            std::uint32_t score = 0x7fffffff;
            EffectRecord* record = nullptr;
        };
        std::vector<Candidate> candidates;
        unsigned count = 0;
    };
}
