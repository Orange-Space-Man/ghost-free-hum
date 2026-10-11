#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/world/doorresources.h"
#include "resources/roomarchivereader.h"
#include "game/startup/startupresources.h"

#include <stdexcept>
#include <utility>

namespace {
    std::string resourcePath(const std::vector<std::uint8_t>& executable, std::uint32_t address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        std::uint32_t pointer = 0;
        for (auto byte : bytes) pointer = (pointer << 8) | byte;
        std::string result;
        for (unsigned i = 0; i < 256; ++i) {
            if (!dolresources::read(executable, pointer + i, 1, bytes, error)) throw std::runtime_error(error);
            if (!bytes[0]) return result;
            char character = static_cast<char>(bytes[0]);
            if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
            if (i || character != '/') result.push_back(character);
        }
        throw std::runtime_error("Unterminated door resource path.");
    }
}

bool doors::Resources::load(const std::vector<std::uint8_t>& archive,
    const std::vector<std::uint8_t>& executable, std::string& error) {
    error.clear();
    try {
        Resources result;
        if (!result.tables.read(executable, error) || !result.trig.read(executable, error)) return false;
        for (unsigned i = 0; i < result.models.size(); ++i) {
            auto& model = result.models[i];
            if (!roomarchive::read(archive, resourcePath(executable, glme01::doorModels + i * 4), model.bytes, error) ||
                !model.read(model.bytes, result.tables, error)) return false;
        }
        for (unsigned i = 0; i < result.animations.size(); ++i) {
            std::vector<std::uint8_t> bytes;
            if (!roomarchive::read(archive, resourcePath(executable, glme01::doorAnimations + i * 4), bytes, error) ||
                !result.animations[i].read(bytes, error)) return false;
        }
        result.loaded = true;
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

const doors::ModelResource& doors::Resources::model(unsigned index) const {
    if (!loaded) throw std::runtime_error("Door resources are not ready.");
    return models.at(index);
}

const dooranimation::Resource& doors::Resources::animation(unsigned index) const {
    if (!loaded) throw std::runtime_error("Door resources are not ready.");
    return animations.at(index);
}

const dooranimation::PoseTables& doors::Resources::poseTables() const { return tables; }
const originalmath::FloatTrig& doors::Resources::floatTrig() const { return trig; }
bool doors::Resources::ready() const { return loaded; }

void doors::PoseOwner::bind(std::shared_ptr<const Resources> resources) {
    if (!resources || !resources->ready()) throw std::runtime_error("Door resources are not ready for binding.");
    if (models.size() + 15 > 16 || animations.size() + 6 > 10)
        throw std::runtime_error("Door binding exceeds original controller capacity.");
    for (unsigned i = 0; i < 15; ++i) models.push_back({resources, i});
    for (unsigned i = 0; i < 6; ++i) animations.push_back({resources, i});
}

void doors::PoseOwner::selectModel(unsigned index) {
    if (index >= models.size()) throw std::out_of_range("Door model binding index.");
    selected = index;
}

void doors::PoseOwner::initializeMatrices() {
    if (state & 1) throw std::runtime_error("Repeated door matrix allocation is not supported.");
    const auto& binding = models.at(selected);
    count = static_cast<unsigned>(binding.owner->model(binding.index).hierarchy.visits().size());
    if (count >= 10) {
        heapMatrices = std::make_unique<dooranimation::Matrix[]>(count);
        state |= 2;
    } else {
        heapMatrices.reset();
    }
    state |= 1;
}

void doors::PoseOwner::sample(float frame, const dooranimation::Matrix& parent, unsigned animationIndex) {
    const auto& model = models.at(selected);
    const auto& animation = animations.at(animationIndex);
    model.owner->model(model.index).hierarchy.sampleInto(animation.owner->animation(animation.index),
        model.owner->poseTables(), frame, parent, state & 2 ? heapMatrices.get() : inlineMatrices.data(), count);
}

void doors::PoseOwner::update(double frame, const dooranimation::Matrix& parent, unsigned animationIndex) {
    if (!(state & 1)) {
        if (models.empty()) return;
        initializeMatrices();
    }
    const auto& animation = animations.at(animationIndex);
    frame = dooranimation::clampFrame(animation.owner->animation(animation.index), frame);
    sample(static_cast<float>(frame), parent, animationIndex);
    delta = static_cast<float>(frame - static_cast<double>(previous));
    previous = static_cast<float>(frame);
}

void doors::PoseOwner::updateLooping(float& frame, const dooranimation::Matrix& parent, unsigned animationIndex) {
    if (!(state & 1)) {
        if (models.empty()) return;
        initializeMatrices();
    }
    const auto& animation = animations.at(animationIndex);
    dooranimation::updateFrame(animation.owner->animation(animation.index), frame);
    sample(frame, parent, animationIndex);
    delta = frame - previous;
    if (delta < 0) delta = 1;
    previous = frame;
}

const dooranimation::Matrix& doors::PoseOwner::matrix(unsigned index) const {
    if (!(state & 1) || index >= count) throw std::out_of_range("Door pose matrix index.");
    return state & 2 ? heapMatrices[index] : inlineMatrices[index];
}

unsigned doors::PoseOwner::matrixCount() const { return count; }
unsigned doors::PoseOwner::flags() const { return state; }
unsigned doors::PoseOwner::selectedModel() const { return selected; }
const doors::ModelResource& doors::PoseOwner::model(unsigned index) const {
    const auto& binding = models.at(index);
    return binding.owner->model(binding.index);
}
float doors::PoseOwner::previousFrame() const { return previous; }
float doors::PoseOwner::frameDelta() const { return delta; }
