#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace gameparameters {
    struct Parameter {
        std::string name;
        std::uint16_t key = 0;
        unsigned width = 4;
        std::uint32_t bits = 0;
        std::array<std::uint32_t, 3> vectorBits{};
    };

    std::uint16_t keyCode(std::string_view name);

    class Parameters {
    public:
        void add(std::string name, unsigned width, std::uint32_t initial);
        void addVector(std::string name, const std::array<std::uint32_t, 3>& initial);
        bool load(const std::vector<std::uint8_t>& data, std::string& error);
        bool loaded() const;
        const std::vector<Parameter>& entries() const;
        void setBits(unsigned index, std::uint32_t value);

    private:
        std::vector<Parameter> values;
        bool hasLoaded = false;
    };
}
