#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace particles {
    struct ParameterCurve {
        std::shared_ptr<const std::vector<std::uint8_t>> source;
        unsigned count = 0, first = 0, second = 0;
        std::uint32_t value(bool secondArray, unsigned index) const;
    };

    struct Parameters {
        void load(std::shared_ptr<const std::vector<std::uint8_t>> source, unsigned offset);
        std::uint32_t flags = 0;
        std::array<std::uint32_t, 9> fieldsD0{};
        std::uint32_t fieldF4 = 0, fieldF8 = 0, fieldFC = 0, field100 = 0, field104 = 0, field108 = 0;
        std::array<std::uint32_t, 13> fields10C{};
        std::array<std::uint16_t, 3> fields140{};
        std::uint32_t field148 = 0;
        std::uint16_t field14C = 0;
        std::uint8_t field14E = 0;
        std::array<std::uint32_t, 4> fields150{};
        std::array<std::uint8_t, 2> fields160{};
        std::array<std::uint32_t, 3> fields168{}, fields174{};
        std::array<std::uint32_t, 7> fields180{};
        std::array<std::uint32_t, 4> fields19C{}, fields1AC{};
        std::array<std::uint32_t, 3> fields1BC{};
        std::array<std::uint8_t, 3> fields1C8{};
        std::uint32_t field1CC = 0;
        std::array<std::uint32_t, 5> fields1D0{};
        std::uint8_t field1E4 = 0;
        std::array<ParameterCurve, 3> curves;
        std::uint16_t field204 = 0;
        std::array<std::uint8_t, 3> fields206{};
    };
}
