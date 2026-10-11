#include "game/world/roommodelreader.h"

#include <iostream>
#include <string>
#include <vector>

namespace {
    void write32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value) {
        for (std::size_t index = 0; index < 4; ++index) {
            data[offset + index] = static_cast<std::uint8_t>(value >> (24 - index * 8));
        }
    }

    std::vector<std::uint8_t> makeModel() {
        std::vector<std::uint8_t> data(0x60 + 2 * 0x8c, 0);
        data[0] = 2;
        write32(data, 0x3c, 0x60);
        for (const std::size_t offset : {0x60U, 0xecU}) {
            data[offset + 2] = 0xff;
            data[offset + 3] = 0xff;
            data[offset + 4] = 0xff;
            data[offset + 5] = 0xff;
            for (std::size_t axis = 0; axis < 3; ++axis) {
                write32(data, offset + 0x0c + axis * 4, 0x3f800000);
            }
        }
        data[0x62] = 0;
        data[0x63] = 1;
        return data;
    }
}

int main() {
    int failures = 0;
    const auto check = [&](bool condition, const char* name) {
        if (!condition) {
            std::cerr << "Failed: " << name << '\n';
            ++failures;
        }
    };
    roommodel::Model model;
    std::string error;
    const std::vector<std::uint8_t> original = makeModel();
    std::vector<std::uint8_t> data = original;
    check(roommodel::inspect(data, model, error), "valid model");
    check(model.nodes.size() == 2, "two reachable nodes");
    check(model.nodes.size() == 2 && model.nodes[0].scale[0] == 1.0f && model.nodes[1].link2 == -1, "big-endian floats and signed links");
    check(data == original, "input remains unchanged");

    for (std::size_t length = 0; length < original.size(); ++length) {
        data.assign(original.begin(), original.begin() + length);
        check(!roommodel::inspect(data, model, error), "truncated required data");
        check(model.nodes.empty(), "failure clears result");
    }
    data = original;
    write32(data, 0x10, 0xffffffff);
    check(!roommodel::inspect(data, model, error), "oversized section offset");
    data = original;
    write32(data, 0x10, 0x20);
    check(!roommodel::inspect(data, model, error), "section inside header");
    data = original;
    write32(data, 0x3c, 0);
    check(!roommodel::inspect(data, model, error), "missing hierarchy");
    data = original;
    data[0x63] = 2;
    check(!roommodel::inspect(data, model, error), "out-of-range link");
    data = original;
    data[0x62] = 0xff;
    data[0x63] = 0xfe;
    check(!roommodel::inspect(data, model, error), "negative non-sentinel link");
    data = original;
    data[0xee] = 0;
    data[0xef] = 0;
    check(!roommodel::inspect(data, model, error), "hierarchy cycle");
    data = original;
    write32(data, 0x40, 0xec);
    check(!roommodel::inspect(data, model, error), "link crosses next section");
    data = original;
    write32(data, 0x6c, 0x7fc00000);
    check(!roommodel::inspect(data, model, error), "non-finite transform");
    data = original;
    data.insert(data.end(), 32, 0);
    check(roommodel::inspect(data, model, error) && model.nodes.size() == 2, "padding is not a node count");
    if (failures == 0) {
        std::cout << "Room-model checks passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
