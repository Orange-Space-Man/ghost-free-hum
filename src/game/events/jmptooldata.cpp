#include "game/events/jmptooldata.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
    std::uint32_t word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        if (offset > data.size() || data.size() - offset < 4) throw std::runtime_error("Truncated JMP data.");
        return (std::uint32_t(data[offset]) << 24) | (std::uint32_t(data[offset + 1]) << 16) |
            (std::uint32_t(data[offset + 2]) << 8) | data[offset + 3];
    }

    std::uint32_t hash(std::string_view name) {
        std::uint32_t result = 0;
        for (unsigned char character : name) {
            if (!character) break;
            const auto signedByte = character < 128 ? static_cast<int>(character) : static_cast<int>(character) - 256;
            result = (result * 256 + static_cast<std::uint32_t>(signedByte)) % 33554393;
        }
        return result;
    }
}

bool jmp::ToolData::attach(const std::vector<std::uint8_t>& data, std::string& error) {
    error.clear();
    if (data.empty()) return false;
    try {
        const auto rowCount = word(data, 0), fieldCount = word(data, 4);
        const auto recordOffset = word(data, 8), recordStride = word(data, 12);
        if (fieldCount > (data.size() - 16) / 12 || recordOffset < 16 + std::size_t(fieldCount) * 12 ||
            recordOffset > data.size() || rowCount > static_cast<unsigned>(std::numeric_limits<int>::max()) ||
            (rowCount && (!recordStride || rowCount > (data.size() - recordOffset) / recordStride)))
            throw std::runtime_error("Invalid JMP table ranges.");
        ToolData result;
        for (unsigned i = 0; i < fieldCount; ++i) {
            const auto offset = 16 + std::size_t(i) * 12;
            const auto packed = word(data, offset + 8);
            Field field{word(data, offset), word(data, offset + 4), packed >> 16, (packed >> 8) & 255};
            if (rowCount && field.offset >= recordStride) throw std::runtime_error("Invalid JMP field offset.");
            result.fields.push_back(field);
        }
        result.bytes = std::make_shared<const std::vector<std::uint8_t>>(data);
        result.rows = rowCount;
        result.records = recordOffset;
        result.stride = recordStride;
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}

void jmp::ToolData::reset() { *this = {}; }
unsigned jmp::ToolData::count() const { return rows; }
unsigned jmp::ToolData::fieldCount() const { return static_cast<unsigned>(fields.size()); }
std::string_view jmp::ToolData::fieldString(unsigned row, unsigned field) const { return string(row, static_cast<int>(field)); }
bool jmp::ToolData::attached() const { return bytes != nullptr; }
bool jmp::ToolData::sharesResource(const ToolData& other) const { return bytes == other.bytes; }

bool jmp::sameEntry(const ToolDataRef& left, const ToolDataRef& right) {
    if (left.row != right.row) return false;
    if (!left.table || !right.table) throw std::runtime_error("Invalid JMP entry reference.");
    return left.table->sharesResource(*right.table);
}

int jmp::ToolData::searchItemInfo(std::string_view name) const {
    const auto key = hash(name);
    for (std::size_t i = 0; i < fields.size(); ++i) if (fields[i].hash == key) return static_cast<int>(i);
    return -1;
}

std::size_t jmp::ToolData::position(unsigned row, int field, unsigned width) const {
    if (!bytes || row >= rows || field < 0 || static_cast<std::size_t>(field) >= fields.size() ||
        fields[field].offset > stride || width > stride - fields[field].offset)
        throw std::runtime_error("JMP access outside record.");
    return records + std::size_t(row) * stride + fields[field].offset;
}

std::uint32_t jmp::ToolData::integer(unsigned row, int field) const {
    const auto offset = position(row, field, 4);
    const auto raw = word(*bytes, offset) & fields[field].mask;
    return fields[field].shift < 32 ? raw >> fields[field].shift : 0;
}

std::string_view jmp::ToolData::string(unsigned row, int field) const {
    const auto offset = position(row, field, 1);
    const auto first = bytes->begin() + offset, end = bytes->begin() + records + std::size_t(row + 1) * stride;
    const auto last = std::find(first, end, 0);
    if (last == end) throw std::runtime_error("Unterminated JMP string.");
    return {reinterpret_cast<const char*>(bytes->data() + offset), static_cast<std::size_t>(last - first)};
}

bool jmp::ToolData::getValue(unsigned row, std::string_view name, std::uint32_t& value) const {
    const int field = searchItemInfo(name);
    if (field < 0) return false;
    value = integer(row, field);
    return true;
}

bool jmp::ToolData::getValue(unsigned row, std::string_view name, std::int32_t& value) const {
    const int field = searchItemInfo(name);
    if (field < 0) return false;
    const auto raw = word(*bytes, position(row, field, 4));
    std::memcpy(&value, &raw, 4);
    return true;
}

bool jmp::ToolData::getValue(unsigned row, std::string_view name, std::uint8_t& value) const {
    const int field = searchItemInfo(name);
    if (field < 0) return false;
    value = (word(*bytes, position(row, field, 4)) & fields[field].mask) != 0;
    return true;
}

bool jmp::ToolData::getValue(unsigned row, std::string_view name, float& value) const {
    const int field = searchItemInfo(name);
    if (field < 0) return false;
    const auto raw = word(*bytes, position(row, field, 4));
    std::memcpy(&value, &raw, 4);
    return true;
}

bool jmp::ToolData::getValue(unsigned row, std::string_view name, std::string_view& value) const {
    const int field = searchItemInfo(name);
    if (field < 0) return false;
    value = string(row, field);
    return true;
}

int jmp::ToolData::findEntryByValue(int field, std::uint32_t value, unsigned start) const {
    for (unsigned row = start; row < rows; ++row) if (integer(row, field) == value) return static_cast<int>(row);
    return -1;
}

int jmp::ToolData::findEntryByValue(int field, std::string_view value, unsigned start) const {
    value = value.substr(0, value.find('\0'));
    for (unsigned row = start; row < rows; ++row) if (string(row, field) == value) return static_cast<int>(row);
    return -1;
}
