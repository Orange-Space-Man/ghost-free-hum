#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gamearchive {
    struct Entry {
        std::string name;
        std::uint16_t hash = 0;
        unsigned flags = 0, offset = 0, size = 0;
    };

    class Archive {
    public:
        bool read(const std::vector<std::uint8_t>& source, std::string& error);
        int findTypeResource(std::uint32_t type, std::string_view name) const;
        int findNameResource(std::string_view name) const;
        int findPathResource(std::string_view absolutePath) const;
        std::vector<unsigned> directory(std::string_view absolutePath) const;
        const Entry& entry(unsigned index) const;
        std::vector<std::uint8_t> resource(unsigned index) const;

    private:
        struct Node { std::uint32_t type; unsigned first, count; };
        std::vector<std::uint8_t> bytes;
        std::vector<Entry> files;
        std::vector<Node> nodes;
        unsigned payload = 0, payloadSize = 0;
    };
}
