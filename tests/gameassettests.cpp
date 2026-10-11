#include "resources/gameassetpackage.h"
#include "resources/roomdiscreader.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const std::string& message) {
        if (!condition) throw std::runtime_error(message);
    }

    void writeFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        require(static_cast<bool>(file), "Fixture write failed.");
    }

    std::vector<std::uint8_t> makeDisc() {
        std::vector<std::uint8_t> bytes(0x900);
        const auto word = [&](std::size_t offset, std::uint32_t value) {
            for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
        };
        const std::string id = "GLME01";
        std::copy(id.begin(), id.end(), bytes.begin());
        word(0x1c, 0xc2339f3d);
        word(0x420, 0x500);
        word(0x424, 0x700);
        word(0x428, 64);
        word(0x500, 0x100);
        word(0x590, 4);
        bytes[0x600] = 0x48;
        word(0x700, 0x01000000);
        word(0x708, 3);
        word(0x70c, 0x01000001);
        word(0x714, 3);
        word(0x718, 5);
        word(0x71c, 0x800);
        word(0x720, 4);
        const char names[] = "\0Dir\0sample.bin\0";
        std::copy(names, names + sizeof(names), bytes.begin() + 0x724);
        bytes[0x800] = 1;
        bytes[0x801] = 2;
        bytes[0x802] = 3;
        bytes[0x803] = 4;
        return bytes;
    }

    void compareDisc(const std::filesystem::path& disc, const std::filesystem::path& packagePath) {
        std::string error;
        gameassets::Package package;
        require(package.open(packagePath, error), error);
        std::vector<roomdisc::File> files;
        require(roomdisc::listFiles(disc, files, error), error);
        require(package.entries().size() == files.size() + 1, "Asset count mismatch.");
        std::ifstream original(disc, std::ios::binary);
        std::ifstream imported(packagePath, std::ios::binary);
        std::vector<char> left(1024 * 1024), right(left.size());
        std::uint64_t verified = 0;
        for (const auto& source : files) {
            const auto& entry = package.entries().at("files/" + source.path);
            require(entry.size == source.size, "Asset size mismatch: " + source.path);
            original.seekg(static_cast<std::streamoff>(source.offset));
            imported.seekg(static_cast<std::streamoff>(entry.offset));
            auto remaining = source.size;
            while (remaining != 0) {
                const auto length = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, left.size()));
                original.read(left.data(), static_cast<std::streamsize>(length));
                imported.read(right.data(), static_cast<std::streamsize>(length));
                require(original && imported && std::equal(left.begin(), left.begin() + length, right.begin()), "Asset bytes differ: " + source.path);
                remaining -= length;
                verified += length;
            }
        }
        std::vector<std::uint8_t> sourceDol, importedDol;
        require(roomdisc::readExecutable(disc, sourceDol, error), error);
        require(package.read("sys/main.dol", importedDol, error), error);
        require(sourceDol == importedDol, "Executable data differs.");
        std::cout << files.size() << " disc files and executable verified; " << verified << " file bytes identical.\n";
    }
}

int main(int argc, char* argv[]) {
    std::filesystem::path temporary;
    try {
        std::string error;
        if (argc == 4 && std::string(argv[1]) == "--import") {
            if (!std::filesystem::exists(argv[3])) require(gameassets::importDisc(argv[2], argv[3], error), error);
            compareDisc(argv[2], argv[3]);
            return 0;
        }
        temporary = std::filesystem::temp_directory_path() / ("polterport-assets-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(temporary);
        const auto disc = temporary / "test.iso";
        const auto output = temporary / "test.ppak";
        auto fixture = makeDisc();
        writeFile(disc, fixture);
        require(!gameassets::importDisc(disc, output, error, [](auto, auto) { return false; }), "Cancelled import succeeded.");
        require(!std::filesystem::exists(output) && !std::filesystem::exists(temporary / "test.ppak.importing"), "Cancelled import left a package.");
        require(gameassets::importDisc(disc, output, error), error);
        compareDisc(disc, output);
        require(!gameassets::importDisc(disc, output, error), "Existing package overwritten.");
        std::filesystem::remove(disc);
        std::uint64_t payloadOffset = 0;
        {
            gameassets::Package package;
            require(package.open(output, error), error);
            std::vector<std::uint8_t> data;
            require(package.read("files/Dir/sample.bin", data, error) && data == std::vector<std::uint8_t>({1, 2, 3, 4}), "ISO-free read failed.");
            require(!package.read("missing", data, error) && data.empty(), "Missing asset accepted.");
            payloadOffset = package.entries().at("files/Dir/sample.bin").offset;
        }
        {
            std::fstream damaged(output, std::ios::binary | std::ios::in | std::ios::out);
            damaged.seekp(static_cast<std::streamoff>(payloadOffset));
            damaged.put(0);
        }
        {
            gameassets::Package package;
            std::vector<std::uint8_t> data;
            require(package.open(output, error), error);
            require(!package.read("files/Dir/sample.bin", data, error) && data.empty(), "Corrupt payload accepted.");
        }
        std::filesystem::resize_file(output, 30);
        {
            gameassets::Package package;
            require(!package.open(output, error) && package.entries().empty(), "Truncated package accepted.");
        }
        std::filesystem::remove(output);
        fixture[7] = 1;
        writeFile(disc, fixture);
        require(!gameassets::importDisc(disc, output, error), "Unsupported revision accepted.");
        fixture[7] = 0;
        fixture[0x729] = '.';
        fixture[0x72a] = '.';
        fixture[0x72b] = 0;
        writeFile(disc, fixture);
        require(!gameassets::importDisc(disc, output, error), "Unsafe asset name accepted.");
        std::filesystem::remove(disc);
        std::filesystem::remove(temporary);
        std::cout << "Import, cancellation, ISO-free reads and corruption checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
