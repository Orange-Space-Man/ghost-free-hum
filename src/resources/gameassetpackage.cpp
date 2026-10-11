#include "resources/gameassetpackage.h"
#include "resources/roomdiscreader.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace {
    constexpr char signature[] = "PPLME001";
    constexpr std::uint64_t headerSize = 24;
    constexpr std::uint64_t maximumIndexSize = 16 * 1024 * 1024;

    std::uint32_t checksum(std::uint32_t value, const std::uint8_t* data, std::size_t size) {
        static const auto table = [] {
            std::array<std::uint32_t, 256> result{};
            for (std::uint32_t i = 0; i < 256; ++i) {
                auto word = i;
                for (unsigned bit = 0; bit < 8; ++bit) word = (word >> 1) ^ ((word & 1) ? 0xedb88320 : 0);
                result[i] = word;
            }
            return result;
        }();
        for (std::size_t i = 0; i < size; ++i) value = table[(value ^ data[i]) & 255] ^ (value >> 8);
        return value;
    }

    void writeNumber(std::ostream& stream, std::uint64_t value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i) stream.put(static_cast<char>(value >> (i * 8)));
    }

    std::uint64_t readNumber(std::istream& stream, unsigned bytes) {
        std::uint64_t result = 0;
        for (unsigned i = 0; i < bytes; ++i) {
            const auto value = stream.get();
            if (value == std::char_traits<char>::eof()) throw std::runtime_error("Truncated asset package.");
            result |= static_cast<std::uint64_t>(static_cast<unsigned char>(value)) << (i * 8);
        }
        return result;
    }

    bool validPath(const std::string& path) {
        if (path.empty() || path.size() > 4096 || path.front() == '/' || path.back() == '/') return false;
        if (path.find_first_of("\\:") != std::string::npos || path.find('\0') != std::string::npos) return false;
        std::size_t start = 0;
        while (start < path.size()) {
            const auto end = path.find('/', start);
            const auto part = path.substr(start, end - start);
            if (part.empty() || part == "." || part == "..") return false;
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return true;
    }

    void writeIndex(std::ostream& stream, const std::map<std::string, gameassets::Entry>& entries,
        std::uint64_t indexSize, std::uint64_t size) {
        stream.seekp(0);
        stream.write(signature, 8);
        writeNumber(stream, entries.size(), 4);
        writeNumber(stream, indexSize, 4);
        writeNumber(stream, size, 8);
        for (const auto& item : entries) {
            writeNumber(stream, item.first.size(), 4);
            writeNumber(stream, item.second.offset, 8);
            writeNumber(stream, item.second.size, 8);
            writeNumber(stream, item.second.checksum, 4);
            stream.write(item.first.data(), static_cast<std::streamsize>(item.first.size()));
        }
    }
}

bool gameassets::Package::open(const std::filesystem::path& path, std::string& error) {
    index.clear();
    file.close();
    file.clear();
    error.clear();
    try {
        file.open(path, std::ios::binary | std::ios::ate);
        if (!file || file.tellg() < 0) throw std::runtime_error("Cannot open the asset package.");
        const auto size = static_cast<std::uint64_t>(file.tellg());
        file.seekg(0);
        char magic[8]{};
        file.read(magic, 8);
        if (!file || !std::equal(magic, magic + 8, signature)) throw std::runtime_error("Unsupported asset package. Import the ISO again.");
        const auto count = readNumber(file, 4);
        const auto indexSize = readNumber(file, 4);
        if (readNumber(file, 8) != size || count == 0 || count > 100000 ||
            indexSize > maximumIndexSize || size < headerSize || indexSize > size - headerSize) {
            throw std::runtime_error("Invalid asset package header. Import the ISO again.");
        }
        const auto dataStart = headerSize + indexSize;
        std::uint64_t consumed = 0;
        std::uint64_t expectedOffset = dataStart;
        for (std::uint64_t i = 0; i < count; ++i) {
            if (consumed > indexSize || indexSize - consumed < 24) throw std::runtime_error("Invalid asset package index.");
            const auto length = readNumber(file, 4);
            Entry entry;
            entry.offset = readNumber(file, 8);
            entry.size = readNumber(file, 8);
            entry.checksum = static_cast<std::uint32_t>(readNumber(file, 4));
            consumed += 24;
            if (length > 4096 || length > indexSize - consumed) throw std::runtime_error("Invalid asset path length.");
            std::string name(static_cast<std::size_t>(length), '\0');
            file.read(name.data(), static_cast<std::streamsize>(length));
            consumed += length;
            if (!file || !validPath(name) || entry.offset != expectedOffset || entry.offset > size ||
                entry.size > size - entry.offset || !index.emplace(name, entry).second) {
                throw std::runtime_error("Invalid asset package entry.");
            }
            expectedOffset += entry.size;
        }
        if (consumed != indexSize || expectedOffset != size || index.count("sys/main.dol") == 0) {
            throw std::runtime_error("Incomplete asset package.");
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        index.clear();
        file.close();
        return false;
    }
}

bool gameassets::Package::read(const std::string& path, std::vector<std::uint8_t>& data, std::string& error) {
    data.clear();
    error.clear();
    try {
        const auto found = index.find(path);
        if (found == index.end()) throw std::runtime_error("Asset not found: " + path);
        const auto& entry = found->second;
        if (entry.size > 64 * 1024 * 1024) throw std::runtime_error("Asset exceeds the buffered read limit: " + path);
        std::vector<std::uint8_t> result(static_cast<std::size_t>(entry.size));
        file.clear();
        file.seekg(static_cast<std::streamoff>(entry.offset));
        if (!file.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()))) {
            throw std::runtime_error("Cannot read asset: " + path);
        }
        if ((checksum(0xffffffff, result.data(), result.size()) ^ 0xffffffff) != entry.checksum) {
            throw std::runtime_error("Asset checksum failed: " + path + ". Import the ISO again.");
        }
        data = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

const std::map<std::string, gameassets::Entry>& gameassets::Package::entries() const {
    return index;
}

bool gameassets::importDisc(const std::filesystem::path& disc, const std::filesystem::path& destination,
    std::string& error, const Progress& progress) {
    error.clear();
    const auto staging = std::filesystem::path(destination.wstring() + L".importing");
    bool ownsStaging = false;
    try {
        if (std::filesystem::exists(destination)) throw std::runtime_error("An asset package already exists at the destination.");
        if (!destination.parent_path().empty()) std::filesystem::create_directories(destination.parent_path());
        if (!std::filesystem::create_directory(staging)) throw std::runtime_error("An unfinished import exists. Remove the .importing folder after closing other ghost-free-hum instances, then retry.");
        ownsStaging = true;
        std::vector<roomdisc::File> files;
        std::vector<std::uint8_t> executable;
        if (!roomdisc::listFiles(disc, files, error) || !roomdisc::readExecutable(disc, executable, error)) throw std::runtime_error(error);
        std::map<std::string, Entry> entries;
        std::map<std::string, roomdisc::File> sources;
        entries.emplace("sys/main.dol", Entry{0, executable.size(), 0});
        for (const auto& entry : files) {
            const auto name = "files/" + entry.path;
            if (!validPath(name) || !entries.emplace(name, Entry{0, entry.size, 0}).second) throw std::runtime_error("Invalid or duplicate disc filename.");
            sources.emplace(name, entry);
        }
        std::uint64_t indexSize = 0;
        std::uint64_t payloadSize = 0;
        for (const auto& item : entries) {
            indexSize += 24 + item.first.size();
            payloadSize += item.second.size;
        }
        if (indexSize > maximumIndexSize || entries.size() > 100000) throw std::runtime_error("Disc contains too many files.");
        const auto total = headerSize + indexSize + payloadSize;
        if (std::filesystem::space(staging).available < total) throw std::runtime_error("Not enough disk space to import the game.");
        const auto temporary = staging / "game.ppak";
        std::ifstream input(disc, std::ios::binary);
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!input || !output) throw std::runtime_error("Cannot open import files.");
        writeIndex(output, entries, indexSize, total);
        std::vector<std::uint8_t> buffer(1024 * 1024);
        std::uint64_t completed = 0;
        for (auto& item : entries) {
            auto& entry = item.second;
            entry.offset = headerSize + indexSize + completed;
            auto crc = std::uint32_t{0xffffffff};
            if (item.first == "sys/main.dol") {
                output.write(reinterpret_cast<const char*>(executable.data()), static_cast<std::streamsize>(executable.size()));
                crc = checksum(crc, executable.data(), executable.size());
                completed += executable.size();
            } else {
                input.clear();
                input.seekg(static_cast<std::streamoff>(sources.at(item.first).offset));
                auto remaining = entry.size;
                while (remaining != 0) {
                    if (progress && !progress(completed, payloadSize)) throw std::runtime_error("Import cancelled.");
                    const auto chunk = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, buffer.size()));
                    if (!input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(chunk))) throw std::runtime_error("Cannot read the ISO during import.");
                    output.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(chunk));
                    if (!output) throw std::runtime_error("Cannot write the asset package.");
                    crc = checksum(crc, buffer.data(), chunk);
                    remaining -= chunk;
                    completed += chunk;
                }
            }
            entry.checksum = crc ^ 0xffffffff;
        }
        if (progress && !progress(completed, payloadSize)) throw std::runtime_error("Import cancelled.");
        writeIndex(output, entries, indexSize, total);
        output.flush();
        if (!output) throw std::runtime_error("Could not finish the asset package.");
        output.close();
        if (!output) throw std::runtime_error("Could not close the asset package.");
        Package check;
        if (!check.open(temporary, error)) throw std::runtime_error(error);
        check = Package{};
        std::filesystem::rename(temporary, destination);
        std::filesystem::remove(staging);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        if (ownsStaging) {
            std::error_code ignored;
            std::filesystem::remove(staging / "game.ppak", ignored);
            std::filesystem::remove(staging, ignored);
        }
        return false;
    }
}
