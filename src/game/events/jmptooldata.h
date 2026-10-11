#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace jmp {
    class ToolData {
    public:
        bool attach(const std::vector<std::uint8_t>& data, std::string& error);
        void reset();
        unsigned count() const;
        unsigned fieldCount() const;
        std::string_view fieldString(unsigned row, unsigned field) const;
        bool attached() const;
        bool sharesResource(const ToolData& other) const;
        int searchItemInfo(std::string_view name) const;
        bool getValue(unsigned row, std::string_view name, std::uint32_t& value) const;
        bool getValue(unsigned row, std::string_view name, std::int32_t& value) const;
        bool getValue(unsigned row, std::string_view name, std::uint8_t& value) const;
        bool getValue(unsigned row, std::string_view name, float& value) const;
        bool getValue(unsigned row, std::string_view name, std::string_view& value) const;
        int findEntryByValue(int field, std::uint32_t value, unsigned start = 0) const;
        int findEntryByValue(int field, std::string_view value, unsigned start = 0) const;

    private:
        struct Field {
            std::uint32_t hash, mask;
            unsigned offset, shift;
        };
        std::shared_ptr<const std::vector<std::uint8_t>> bytes;
        std::vector<Field> fields;
        unsigned rows = 0, records = 0, stride = 0;
        std::size_t position(unsigned row, int field, unsigned width) const;
        std::uint32_t integer(unsigned row, int field) const;
        std::string_view string(unsigned row, int field) const;
    };

    struct ToolDataRef {
        const ToolData* table = nullptr;
        int row = -1;
    };
    bool sameEntry(const ToolDataRef& left, const ToolDataRef& right);
}
