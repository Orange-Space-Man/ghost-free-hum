#include "game/effects/particleparameters.h"

#include <stdexcept>

namespace {
    std::uint16_t half(const std::vector<std::uint8_t>& data, unsigned offset) {
        return static_cast<std::uint16_t>((unsigned(data.at(offset)) << 8) | data.at(offset + 1));
    }

    std::uint32_t word(const std::vector<std::uint8_t>& data, unsigned offset) {
        return (std::uint32_t(half(data, offset)) << 16) | half(data, offset + 2);
    }

    template<std::size_t Count>
    unsigned words(const std::vector<std::uint8_t>& data, unsigned offset, std::array<std::uint32_t, Count>& destination) {
        for (unsigned i = 0; i < Count; ++i) destination[i] = word(data, offset + i * 4);
        return offset + static_cast<unsigned>(Count * 4);
    }

    unsigned curve(particles::ParameterCurve& destination, const std::shared_ptr<const std::vector<std::uint8_t>>& source,
        unsigned offset) {
        destination.source = source;
        destination.count = source->at(offset);
        destination.first = offset + 4;
        destination.second = destination.first + destination.count * 4;
        const auto end = destination.second + destination.count * 4;
        if (end > source->size()) throw std::runtime_error("Original particle parameter curve exceeds its resource.");
        return end;
    }
}

std::uint32_t particles::ParameterCurve::value(bool secondArray, unsigned index) const {
    if (!source || index >= count) throw std::out_of_range("Original particle parameter curve index is invalid.");
    return word(*source, (secondArray ? second : first) + index * 4);
}

void particles::Parameters::load(std::shared_ptr<const std::vector<std::uint8_t>> source, unsigned offset) {
    if (!source || offset > source->size() || source->size() > 0x7fffffff)
        throw std::runtime_error("Original particle parameter resource is invalid.");
    const auto& data = *source;
    flags = word(data, offset);
    words(data, offset + 4, fieldsD0);
    fieldFC = word(data, offset + 0x28);
    unsigned cursor;
    if (flags & 0xc0) cursor = curve(curves[0], source, offset + 0x2c);
    else {
        fieldF4 = word(data, offset + 0x2c);
        cursor = offset + 0x30;
    }
    fieldF8 = word(data, cursor);
    field100 = word(data, cursor + 4);
    if (flags & 0x300) cursor = curve(curves[1], source, cursor + 8);
    else {
        field104 = word(data, cursor + 8);
        cursor += 12;
    }
    if (flags & 0xc00) cursor = curve(curves[2], source, cursor);
    else {
        field108 = word(data, cursor);
        cursor += 4;
    }
    words(data, cursor, fields10C);
    field148 = word(data, cursor + 0x34);
    for (unsigned i = 0; i < fields140.size(); ++i) fields140[i] = half(data, cursor + 0x38 + i * 2);
    field14C = half(data, cursor + 0x3e);
    field204 = half(data, cursor + 0x40);
    field1E4 = data.at(cursor + 0x42);
    field14E = data.at(cursor + 0x43);
    for (unsigned i = 0; i < fields206.size(); ++i) fields206[i] = data.at(cursor + 0x44 + i);
    cursor += 0x48;
    if (flags & 0x10000) cursor = words(data, cursor, fields168);
    if (flags & 0x20000) cursor = words(data, cursor, fields174);
    if (flags & 0x40000) cursor = words(data, cursor, fields180);
    if (flags & 0x80000) cursor = words(data, cursor, fields19C);
    if (flags & 0x100000) cursor = words(data, cursor, fields1AC);
    if (flags & 0x200000) {
        cursor = words(data, cursor, fields1BC);
        fields1C8[0] = data.at(cursor);
        fields1C8[2] = data.at(cursor + 1);
        fields1C8[1] = data.at(cursor + 2);
        cursor += 4;
    }
    if (flags & 0x400000) {
        field1CC = word(data, cursor);
        cursor += 4;
    }
    if (flags & 0x800000) cursor = words(data, cursor, fields1D0);
    if (flags & 0x2000) {
        cursor = words(data, cursor, fields150);
        fields160[0] = data.at(cursor);
        fields160[1] = data.at(cursor + 1);
    }
}
