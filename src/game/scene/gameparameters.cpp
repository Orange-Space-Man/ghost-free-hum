#include "game/scene/gameparameters.h"

#include <algorithm>
#include <stdexcept>

namespace {
    class Input {
    public:
        explicit Input(const std::vector<std::uint8_t>& source) : data(source) {}

        std::uint32_t read(unsigned width) {
            require(width);
            std::uint32_t result = 0;
            for (unsigned i = 0; i < width; ++i) result = (result << 8) | data[position++];
            return result;
        }

        std::string name() {
            const auto length = read(2);
            require(length);
            std::string result(data.begin() + position, data.begin() + position + std::min(length, 79U));
            position += length;
            result.resize(result.find('\0') == std::string::npos ? result.size() : result.find('\0'));
            return result;
        }

        void skip(unsigned size) { require(size); position += size; }

    private:
        const std::vector<std::uint8_t>& data;
        std::size_t position = 0;
        void require(unsigned size) const {
            if (size > data.size() - position) throw std::runtime_error("Truncated parameter stream.");
        }
    };
}

std::uint16_t gameparameters::keyCode(std::string_view name) {
    std::uint32_t hash = 0;
    for (unsigned char character : name) {
        if (!character) break;
        const auto signedByte = character < 128 ? static_cast<int>(character) : static_cast<int>(character) - 256;
        hash = (hash * 256 + static_cast<std::uint32_t>(signedByte)) % 33554393;
    }
    return static_cast<std::uint16_t>(hash);
}

void gameparameters::Parameters::add(std::string name, unsigned width, std::uint32_t initial) {
    if (width != 1 && width != 2 && width != 4) throw std::runtime_error("Unsupported parameter value width.");
    const auto terminator = name.find('\0');
    if (terminator != std::string::npos) name.resize(terminator);
    const auto key = keyCode(name);
    values.push_back({std::move(name), key, width, width == 1 ? initial & 255 : width == 2 ? initial & 65535 : initial});
}

void gameparameters::Parameters::addVector(std::string name, const std::array<std::uint32_t, 3>& initial) {
    add(std::move(name), 4, 0);
    values.back().width = 12;
    values.back().vectorBits = initial;
}

bool gameparameters::Parameters::load(const std::vector<std::uint8_t>& data, std::string& error) {
    error.clear();
    if (values.empty()) return true;
    try {
        Input stream(data);
        const auto count = stream.read(4);
        if (count < 0x80000000) {
            for (unsigned i = 0; i < count; ++i) {
                const auto key = stream.read(2);
                const auto name = stream.name();
                const auto parameter = std::find_if(values.begin(), values.end(), [&](const Parameter& value) {
                    return value.key == key && value.name == name;
                });
                const auto size = stream.read(4);
                if (parameter == values.end()) stream.skip(size);
                else if (parameter->width == 12) for (auto& word : parameter->vectorBits) word = stream.read(4);
                else parameter->bits = stream.read(parameter->width);
            }
        }
        hasLoaded = true;
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}

bool gameparameters::Parameters::loaded() const { return hasLoaded; }
const std::vector<gameparameters::Parameter>& gameparameters::Parameters::entries() const { return values; }
void gameparameters::Parameters::setBits(unsigned index, std::uint32_t value) {
    auto& parameter = values.at(index);
    parameter.bits = parameter.width == 1 ? value & 255 : parameter.width == 2 ? value & 65535 : value;
}
