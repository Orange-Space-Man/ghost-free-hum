#include "resources/dolresources.h"
#include "resources/roomarchivereader.h"
#include "resources/gameedition.h"
#include "resources/regionstaticmappings.h"
#include <sstream>
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
        std::uint32_t actualAddress = address;
        const auto edition = gameedition::identify(dol);
        const bool originalConstant = (address >= 0x804cff60 && address < 0x804d7fe0) ||
            (address >= 0x804d9880 && address < 0x804dd940);
        if (originalConstant && edition != "GLME01" && !edition.empty()) {
            if (size > UINT32_MAX || !regionstatic::translate(edition, address,
                static_cast<std::uint32_t>(size), actualAddress)) {
                std::ostringstream message;
                message << "No verified " << edition << " counterpart for North American constant 0x"
                    << std::hex << address << ".";
                throw std::runtime_error(message.str());
            }
        }
        for (unsigned i = 0; i < 18; ++i) {
            const auto offset = dword(dol, i * 4);
            const auto start = dword(dol, 0x48 + i * 4);
            const auto length = dword(dol, 0x90 + i * 4);
            if (actualAddress >= start && static_cast<std::uint64_t>(actualAddress) - start < length) {
                const std::size_t relative = actualAddress - start;
                if (size > length - relative) {
                    std::ostringstream message;
                    message << "Embedded resource at 0x" << std::hex << address << std::dec << " (" << size
                        << " bytes) crosses a DOL section.";
                    throw std::runtime_error(message.str());
                }
                range(dol, offset, length);
                output.assign(dol.begin() + offset + relative, dol.begin() + offset + relative + size);
                return true;
            }
        }
        std::ostringstream message;
        message << "Embedded resource address 0x" << std::hex << address << " is not in the executable.";
        throw std::runtime_error(message.str());
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool dolresources::readStaticArchiveResource(const std::vector<std::uint8_t>& executable,
    const std::string& path, std::vector<std::uint8_t>& output, std::string& error) {
    const std::uint32_t staticArchiveAddress = gameedition::startup(gameedition::identify(executable)).titleArchive;
    std::vector<std::uint8_t> header, archive;
    if (!read(executable, staticArchiveAddress, 8, header, error)) return false;
    if (dword(header, 0) != 0x52415243 || dword(header, 4) > 0x100000) {
        error = "Invalid executable static archive.";
        return false;
    }
    return read(executable, staticArchiveAddress, dword(header, 4), archive, error) &&
        roomarchive::read(archive, path, output, error);
}
