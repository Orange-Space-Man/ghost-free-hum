#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace j3dmodel {
    class NameTable {
    public:
        bool read(const std::vector<std::uint8_t>& bytes, std::size_t offset, std::size_t end, std::string& error);
        int index(std::string_view name) const;
        const std::string* name(std::uint16_t index) const;
        unsigned size() const;
        static std::uint16_t key(std::string_view name);

    private:
        struct Entry {
            std::uint16_t key;
            std::string name;
        };
        std::vector<Entry> entries;
    };
}
