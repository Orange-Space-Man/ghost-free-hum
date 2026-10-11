#include "game/save/gamecardstorage.h"

#include <algorithm>
#include <fstream>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
#ifdef _WIN32
    struct WriteLock {
        HANDLE handle = INVALID_HANDLE_VALUE;
        ~WriteLock() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    };
#endif
    template<typename Data>
    unsigned read16(const Data& data, std::size_t offset) {
        return (data[offset] << 8) | data[offset + 1];
    }

    template<typename Data>
    std::uint32_t read32(const Data& data, std::size_t offset) {
        return (read16(data, offset) << 16) | read16(data, offset + 2);
    }

    template<typename Data>
    void write32(Data& data, std::size_t offset, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) data[offset + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }

    gamecard::Block block(const gamecard::Image& image, unsigned index) {
        gamecard::Block result{};
        std::copy_n(image.begin() + 0x40 + index * 0x2000, result.size(), result.begin());
        return result;
    }

    void store(gamecard::Image& image, unsigned index, const gamecard::Block& data) {
        std::copy(data.begin(), data.end(), image.begin() + 0x40 + index * 0x2000);
    }

    bool metadataValid(const gamecard::Image& image) {
        const std::string identity = "GLME01";
        const std::string filename = "LUIGI_MANSION_SAVEDATA_v3";
        return std::equal(identity.begin(), identity.end(), image.begin()) &&
            std::equal(filename.begin(), filename.end(), image.begin() + 8) && image[8 + filename.size()] == 0 &&
            read16(image, 0x38) == 3 && read32(image, 0x2c) == 0 && read32(image, 0x3c) == 0x1c00 &&
            (image[7] & 7) == 5 && read16(image, 0x30) == 0x4041 && read16(image, 0x32) == 0xffff;
    }

    bool readImage(const std::filesystem::path& path, gamecard::Image& image) {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input || input.tellg() != static_cast<std::streamoff>(image.size())) return false;
        input.seekg(0);
        input.read(reinterpret_cast<char*>(image.data()), image.size());
        return input.good();
    }
}

std::uint32_t gamecard::checksum(const Block& data) {
    std::uint32_t sum = 0;
    std::uint32_t inverse = 0;
    for (std::size_t offset = 0; offset < 0x1ffc; offset += 2) {
        const auto value = read16(data, offset);
        sum += value;
        inverse += ~value;
    }
    return (sum << 16) | (inverse & 0xffff);
}

bool gamecard::valid(const Block& data) {
    return read32(data, 8) == 0x3b0 && read32(data, 0x1ffc) == checksum(data);
}

gamecard::LoadResult gamecard::decode(const Image& image, gamesave::State& state) {
    if (!metadataValid(image)) return LoadResult::damaged;
    const auto first = block(image, 1);
    const auto second = block(image, 2);
    const auto firstValid = valid(first);
    const auto secondValid = valid(second);
    if (!firstValid && !secondValid) return LoadResult::damaged;
    state.working = firstValid && (!secondValid || read32(second, 0) < read32(first, 0)) ? first : second;
    state.committed = state.working;
    state.initialized = true;
    return LoadResult::loaded;
}

gamecard::Image gamecard::create() {
    Image result{};
    const std::string identity = "GLME01";
    const std::string filename = "LUIGI_MANSION_SAVEDATA_v3";
    std::copy(identity.begin(), identity.end(), result.begin());
    std::copy(filename.begin(), filename.end(), result.begin() + 8);
    result[6] = 0xff;
    result[7] = 5;
    result[0x30] = 0x40;
    result[0x31] = 0x41;
    result[0x32] = result[0x33] = 0xff;
    result[0x34] = 4;
    result[0x36] = result[0x37] = 0xff;
    result[0x39] = 3;
    result[0x3a] = result[0x3b] = 0xff;
    write32(result, 0x3c, 0x1c00);
    return result;
}

void gamecard::update(Image& image, gamesave::State& state, const Block& banner, std::uint32_t tick, std::uint32_t modified) {
    store(image, 0, banner);
    auto& data = state.working;
    write32(data, 8, 0x3b0);
    if (read32(data, 4) == 0) {
        write32(data, 0x1ffc, checksum(data));
        store(image, 1, data);
        write32(data, 4, tick == 0 ? 1 : tick);
    }
    const auto counter = read32(data, 0) + 1;
    write32(data, 0, counter);
    write32(data, 0x1ffc, checksum(data));
    store(image, (counter & 1) + 1, data);
    write32(image, 0x28, modified);
    state.committed = data;
}

gamecard::LoadResult gamecard::Storage::open(const std::filesystem::path& filename, gamesave::State& state, std::string& error) {
    path = filename;
    error.clear();
    image = {};
    result = LoadResult::unavailable;
    std::error_code failure;
    const bool exists = std::filesystem::exists(path, failure);
    if (failure) {
        error = failure.message();
    } else if (!exists) {
        image = create();
        result = LoadResult::empty;
    } else if (readImage(path, image)) {
        result = decode(image, state);
    } else {
        const auto size = std::filesystem::file_size(path, failure);
        result = !failure && size != image.size() ? LoadResult::damaged : LoadResult::unavailable;
        error = "Could not read the memory-card save file.";
    }
    return result;
}

bool gamecard::Storage::save(gamesave::State& state, const Block& banner, std::uint32_t tick, std::uint32_t modified, std::string& error) {
    error.clear();
    if (path.empty() || !state.initialized || (result != LoadResult::empty && result != LoadResult::loaded)) {
        error = "Memory-card save is not writable.";
        return false;
    }
    std::error_code failure;
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), failure);
    if (failure) { error = failure.message(); return false; }
#ifdef _WIN32
    auto lockPath = path;
    lockPath += ".lock";
    WriteLock lock;
    lock.handle = CreateFileW(lockPath.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (lock.handle == INVALID_HANDLE_VALUE) { error = "Memory-card save is in use."; return false; }
#endif
    const bool exists = std::filesystem::exists(path, failure);
    Image current{};
    if (failure || (result == LoadResult::empty && exists) ||
        (result == LoadResult::loaded && (!readImage(path, current) || current != image))) {
        error = "The memory-card save changed outside ghost-free-hum.";
        return false;
    }
    auto nextImage = image;
    auto nextState = state;
    update(nextImage, nextState, banner, tick, modified);
    auto temporary = path;
    temporary += ".writing";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) { error = "Could not open the pending memory-card save."; return false; }
        output.write(reinterpret_cast<const char*>(nextImage.data()), nextImage.size());
        output.close();
        if (!output) { error = "Could not write the memory-card save."; return false; }
    }
    Image verified{};
    if (!readImage(temporary, verified) || verified != nextImage) {
        error = "Memory-card write verification failed.";
        return false;
    }
#ifdef _WIN32
    const bool replaced = exists ? ReplaceFileW(path.c_str(), temporary.c_str(), nullptr, 0, nullptr, nullptr) != 0 :
        MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
    if (!replaced) { error = "Could not finish the memory-card save."; return false; }
#else
    std::filesystem::rename(temporary, path, failure);
    if (failure) { error = failure.message(); return false; }
#endif
    image = nextImage;
    state = nextState;
    result = LoadResult::loaded;
    return true;
}

gamecard::LoadResult gamecard::Storage::status() const { return result; }
