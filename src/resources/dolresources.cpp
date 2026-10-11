#include "resources/dolresources.h"
#include <stdexcept>

namespace {
    void require(bool condition, const char* error) {
        if (!condition) throw std::runtime_error(error);
    }

    void range(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t size) {
        require(offset <= data.size() && size <= data.size() - offset, "Truncated startup resource.");
    }

    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        range(data, offset, 2);
        return (data[offset] << 8) | data[offset + 1];
    }

    std::uint32_t dword(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (word(data, offset) << 16) | word(data, offset + 2);
    }


}

bool dolresources::read(const std::vector<std::uint8_t>& dol, std::uint32_t address,
    std::size_t size, std::vector<std::uint8_t>& output, std::string& error) {
    output.clear();
    error.clear();
    try {
        range(dol, 0, 0x100);
        for (unsigned i = 0; i < 18; ++i) {
            const auto offset = dword(dol, i * 4);
            const auto start = dword(dol, 0x48 + i * 4);
            const auto length = dword(dol, 0x90 + i * 4);
            if (address >= start && static_cast<std::uint64_t>(address) - start < length) {
                const std::size_t relative = address - start;
                require(size <= length - relative, "Embedded resource crosses a DOL section.");
                range(dol, offset, length);
                output.assign(dol.begin() + offset + relative, dol.begin() + offset + relative + size);
                return true;
            }
        }
        throw std::runtime_error("Embedded resource address is not in the executable.");
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
