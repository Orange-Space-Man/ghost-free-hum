#include "game/world/doormodelhierarchy.h"

#include <set>
#include <cstring>
#include <stdexcept>
#include <utility>

bool doormodel::Hierarchy::read(const std::vector<std::uint8_t>& model, std::string& error) {
    error.clear();
    try {
        if (model.size() < 0x40) throw std::runtime_error("Truncated door model header.");
        std::size_t offset = 0;
        for (unsigned i = 0x3c; i < 0x40; ++i) offset = (offset << 8) | model[i];
        std::vector<NodeVisit> result, pending{{0, -1}};
        std::vector<std::array<float, 9>> channels;
        std::set<std::int16_t> seen;
        while (!pending.empty()) {
            const auto visit = pending.back();
            pending.pop_back();
            if (visit.index == -1) continue;
            if (visit.index < 0 || !seen.insert(visit.index).second)
                throw std::runtime_error("Unsupported repeated or negative door model node.");
            const std::size_t position = offset + static_cast<std::size_t>(visit.index) * 0x8c;
            if (position > model.size() || model.size() - position < 0x8c)
                throw std::runtime_error("Door model node exceeds its resource.");
            const auto link = [&](unsigned field) {
                const unsigned value = (static_cast<unsigned>(model[position + field]) << 8) | model[position + field + 1];
                return static_cast<std::int16_t>(value < 0x8000 ? static_cast<int>(value) : static_cast<int>(value) - 0x10000);
            };
            result.push_back(visit);
            std::array<float, 9> transform;
            for (unsigned channel = 0; channel < transform.size(); ++channel) {
                std::uint32_t bits = 0;
                for (unsigned byte = 0; byte < 4; ++byte)
                    bits = (bits << 8) | model[position + 0xc + channel * 4 + byte];
                std::memcpy(&transform[channel], &bits, sizeof(bits));
            }
            channels.push_back(transform);
            pending.push_back({link(2), visit.index});
            pending.push_back({link(4), visit.parent});
        }
        order = std::move(result);
        transforms = std::move(channels);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

const std::vector<doormodel::NodeVisit>& doormodel::Hierarchy::visits() const {
    return order;
}

std::vector<dooranimation::Matrix> doormodel::Hierarchy::staticPose(const dooranimation::PoseTables& tables,
    const dooranimation::Matrix& parent) const {
    if (order.empty()) throw std::runtime_error("Door model hierarchy is not loaded.");
    std::vector<dooranimation::Matrix> matrices(order.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        const auto& visit = order[i];
        if (static_cast<std::size_t>(visit.index) >= matrices.size())
            throw std::runtime_error("Door model index exceeds original matrix allocation count.");
        const auto& inherited = visit.parent == -1 ? parent : matrices.at(visit.parent);
        matrices[visit.index] = dooranimation::concatenate(inherited, tables.staticLocal(transforms[i]));
    }
    return matrices;
}

std::vector<dooranimation::Matrix> doormodel::Hierarchy::sample(const dooranimation::Resource& animation,
    const dooranimation::PoseTables& tables, float frame, const dooranimation::Matrix& parent) const {
    if (order.empty()) throw std::runtime_error("Door model hierarchy is not loaded.");
    std::vector<dooranimation::Matrix> matrices(order.size());
    sampleInto(animation, tables, frame, parent, matrices.data(), static_cast<unsigned>(matrices.size()));
    return matrices;
}

void doormodel::Hierarchy::sampleInto(const dooranimation::Resource& animation, const dooranimation::PoseTables& tables,
    float frame, const dooranimation::Matrix& parent, dooranimation::Matrix* matrices, unsigned count) const {
    if (order.empty() || !matrices) throw std::runtime_error("Door model hierarchy or matrix storage is not ready.");
    for (const auto& visit : order) {
        if (static_cast<unsigned>(visit.index) >= count || (visit.parent != -1 && static_cast<unsigned>(visit.parent) >= count))
            throw std::runtime_error("Door model index exceeds original matrix allocation count.");
        const auto& inherited = visit.parent == -1 ? parent : matrices[visit.parent];
        matrices[visit.index] = dooranimation::samplePose(animation, tables, visit.index, frame, inherited);
    }
}
