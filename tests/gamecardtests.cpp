#include "game/save/gamecardstorage.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
    void check(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    void write(const std::filesystem::path& path, const gamecard::Image& image) {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(image.data()), image.size());
        output.close();
        check(output.good(), "Could not create the card test fixture.");
    }
}

int main() {
    std::filesystem::path directory;
    try {
        std::string error;
        gamesave::State state;
        check(gamesave::initialize({0, 31, 255}, state, error), error.c_str());
        gamecard::Block zero{};
        check(gamecard::checksum(zero) == 0x0000f002, "Original checksum complement count changed.");
        auto image = gamecard::create();
        const auto empty = state;
        check(gamecard::decode(image, state) == gamecard::LoadResult::damaged && state.working == empty.working,
            "Uninitialized image was treated as a valid save.");
        state.working[13] = 1;
        gamecard::update(image, state, zero, 0, 123);
        check(state.working[3] == 1 && state.working[7] == 1 && gamecard::valid(state.working), "First save counter or zero-tick fallback changed.");
        gamesave::State loaded;
        check(gamecard::decode(image, loaded) == gamecard::LoadResult::loaded && gamesave::slotUsed(loaded, 0), "First save did not reload.");
        check(loaded.working == state.working && loaded.working == loaded.committed, "Reload changed save bytes.");
        const auto first = image;
        state.working[13 + 0x3b0] = 1;
        gamecard::update(image, state, zero, 456, 124);
        check(std::equal(first.begin() + 0x4040, first.end(), image.begin() + 0x4040), "Second write replaced the backup copy.");
        check(gamecard::decode(image, loaded) == gamecard::LoadResult::loaded && gamesave::slotUsed(loaded, 1), "Newer copy was not selected.");
        image[0x2040 + 50] ^= 1;
        check(gamecard::decode(image, loaded) == gamecard::LoadResult::loaded && !gamesave::slotUsed(loaded, 1) && gamesave::slotUsed(loaded, 0),
            "A damaged newer copy did not recover the older valid save.");
        image[0x4040 + 50] ^= 1;
        check(gamecard::decode(image, loaded) == gamecard::LoadResult::damaged, "Two damaged copies were accepted.");
        image = first;
        image[0x31] ^= 1;
        check(gamecard::decode(image, loaded) == gamecard::LoadResult::damaged, "Original icon metadata validation was skipped.");
        image = first;
        image[3] = 'P';
        check(gamecard::decode(image, loaded) == gamecard::LoadResult::damaged, "A different game revision's save was accepted.");
        image = first;
        std::copy_n(image.begin() + 0x4040, 0x2000, image.begin() + 0x2040);
        image[0x2040 + 13] = 0;
        gamecard::Block tie{};
        std::copy_n(image.begin() + 0x2040, 0x2000, tie.begin());
        const auto sum = gamecard::checksum(tie);
        for (unsigned i = 0; i < 4; ++i) image[0x2040 + 0x1ffc + i] = static_cast<std::uint8_t>(sum >> (24 - i * 8));
        check(gamecard::decode(image, loaded) == gamecard::LoadResult::loaded && gamesave::slotUsed(loaded, 0), "Equal counters did not prefer the second copy.");

        directory = std::filesystem::temp_directory_path() / ("polterport-card-test-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        const auto path = directory / "slot-a.gci";
        gamecard::Storage storage;
        state = empty;
        check(storage.open(path, state, error) == gamecard::LoadResult::empty && !std::filesystem::exists(directory), "Opening an empty card wrote a file.");
        check(storage.save(state, zero, 1, 2, error), error.c_str());
        check(std::filesystem::file_size(path) == 0x6040, "Unexpected GCI file size.");
        gamecard::Storage reopened;
        check(reopened.open(path, loaded, error) == gamecard::LoadResult::loaded && loaded.working == state.working, "Persistent card reload failed.");
        state.working[13] = 1;
        check(storage.save(state, zero, 3, 4, error), error.c_str());
        check(!reopened.save(loaded, zero, 5, 6, error), "A stale card handle overwrote a newer save.");
        check(reopened.open(path, loaded, error) == gamecard::LoadResult::loaded && gamesave::slotUsed(loaded, 0), "Second persistent save failed.");
        image = first;
        image[0x2040 + 50] ^= 1;
        image[0x4040 + 50] ^= 1;
        write(path, image);
        check(storage.open(path, loaded, error) == gamecard::LoadResult::damaged, "Damaged on-disk save was accepted.");
        check(!storage.save(loaded, zero, 7, 8, error), "Damaged save was silently replaced.");
        std::filesystem::remove(path);
        std::filesystem::remove(directory);
        std::cout << "Original save checksums, alternating copies, recovery, metadata, and persistent storage passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        if (!directory.empty()) std::cerr << "Test files retained at " << directory << '\n';
        return 1;
    }
}
