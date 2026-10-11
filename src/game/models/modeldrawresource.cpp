#include "game/models/modeldrawresource.h"

#include <stdexcept>
#include <utility>

namespace {
    void range(const std::vector<std::uint8_t>& bytes, std::size_t offset, std::size_t size) {
        if (offset > bytes.size() || size > bytes.size() - offset)
            throw std::runtime_error("Model draw data exceeds its resource.");
    }

    unsigned half(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        range(bytes, offset, 2);
        return (unsigned(bytes[offset]) << 8) | bytes[offset + 1];
    }

    std::int16_t signedHalf(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        const unsigned value = half(bytes, offset);
        return static_cast<std::int16_t>(value < 0x8000 ? int(value) : int(value) - 0x10000);
    }

    std::size_t word(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        return (std::size_t(half(bytes, offset)) << 16) | half(bytes, offset + 2);
    }
}

bool modeldraw::Resource::read(const std::vector<std::uint8_t>& bytes, std::string& error) {
    error.clear();
    try {
        range(bytes, 0, 0x40);
        Resource result;
        const auto hierarchyOffset = word(bytes, 0x3c), batchOffset = word(bytes, 0x38);
        const auto materialOffset = word(bytes, 0x34), samplerOffset = word(bytes, 0x10);
        std::vector<std::int16_t> pending{0};
        while (!pending.empty()) {
            const auto index = pending.back();
            pending.pop_back();
            if (index == -1) continue;
            if (index < 0 || result.hierarchy.count(index))
                throw std::runtime_error("Repeated or invalid model draw node.");
            const auto offset = hierarchyOffset + std::size_t(index) * 0x8c;
            range(bytes, offset, 0x8c);
            Node node;
            node.child = signedHalf(bytes, offset + 2);
            node.sibling = signedHalf(bytes, offset + 4);
            node.flags = static_cast<std::uint16_t>(half(bytes, offset + 8));
            const int count = signedHalf(bytes, offset + 0x4c);
            node.batchCount = static_cast<std::int16_t>(count);
            const auto bindings = hierarchyOffset + word(bytes, offset + 0x50);
            for (int i = 0; i < count; ++i) {
                Binding binding{signedHalf(bytes, bindings + i * 4), signedHalf(bytes, bindings + i * 4 + 2)};
                if (binding.material < 0 || binding.batch < 0)
                    throw std::runtime_error("Negative model material or batch index.");
                node.bindings.push_back(binding);
                if (!result.passes.count(binding.material)) {
                    const auto material = materialOffset + std::size_t(binding.material) * 0x28;
                    range(bytes, material, 0x28);
                    auto pass = bytes[material + 2];
                    if (!pass) {
                        const auto sampler = signedHalf(bytes, material + 8);
                        if (sampler < -1) throw std::runtime_error("Invalid model sampler index.");
                        if (sampler != -1) {
                            const auto record = samplerOffset + std::size_t(sampler) * 0x14;
                            range(bytes, record, 0x14);
                            pass = bytes[record + 6];
                        }
                    }
                    result.passes.emplace(binding.material, pass);
                }
                if (!result.batches.count(binding.batch)) {
                    const auto record = batchOffset + std::size_t(binding.batch) * 0x18;
                    range(bytes, record, 0x18);
                    Batch batch;
                    batch.polygonCount = static_cast<std::uint16_t>(half(bytes, record));
                    batch.attributes = static_cast<std::uint32_t>(word(bytes, record + 4));
                    batch.lighting = bytes[record + 8];
                    batch.cull = bytes[record + 9];
                    batch.textureCount = bytes[record + 10];
                    batch.normalMode = bytes[record + 11];
                    const auto start = batchOffset + word(bytes, record + 12);
                    const auto size = std::size_t(half(bytes, record + 2)) * 32;
                    range(bytes, start, size);
                    batch.displayList.assign(bytes.begin() + start, bytes.begin() + start + size);
                    result.batches.emplace(binding.batch, std::move(batch));
                }
            }
            pending.push_back(node.child);
            pending.push_back(node.sibling);
            result.hierarchy.emplace(index, std::move(node));
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

const modeldraw::Node& modeldraw::Resource::node(std::int16_t index) const { return hierarchy.at(index); }
const modeldraw::Batch& modeldraw::Resource::batch(std::int16_t index) const { return batches.at(index); }
std::uint8_t modeldraw::Resource::materialPass(std::int16_t index) const { return passes.at(index); }
const std::map<std::int16_t, modeldraw::Node>& modeldraw::Resource::nodes() const { return hierarchy; }
