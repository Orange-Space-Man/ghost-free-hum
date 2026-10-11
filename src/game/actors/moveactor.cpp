#include "game/actors/moveactor.h"
#include "game/actors/actorpathsampler.h"

#include <stdexcept>
#include <cmath>
#include <cstring>

namespace {
    actormodel::ResourceRef auxiliary(const actormodel::Entry& entry, unsigned group, std::uint32_t index) {
        if (!entry.archive) return nullptr;
        const auto& resources = entry.archive->auxiliary.at(group);
        return index < resources.size() ? resources[index] : nullptr;
    }
}

actors::MoveActor::MoveActor(const Construction& construction, unsigned id, unsigned type, unsigned capacity) :
    fieldE0(construction.globals.color), fieldE4(construction.globals.color), construction(construction) {
    actorId = id;
    actorType = type;
    const auto& info = construction.catalog.type(type);
    flags = info.flags;
    const auto& entry = construction.resources.entry(info.model);
    model = std::make_unique<actormodel::Instance>(entry.archive ? entry.archive->model : nullptr, capacity ? capacity : 130);
    fieldE4 = construction.parameters.parameters(type).entries().at(24).bits;
    selectAnimation(0);
    blendFramesRemaining = blendFramesTotal = 0;
    model->bindKey(construction.resources.key(actorType, animationState));
    construction.pose.initialize(*model);
    fieldA4 = construction.globals.fieldA4;
    fieldA8 = construction.globals.fieldA8;
    matrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    field78C = 1;
}

actors::MoveActor::~MoveActor() {
    model.reset();
}

unsigned actors::MoveActor::id() const {
    return actorId;
}

unsigned actors::MoveActor::type() const {
    return actorType;
}

unsigned actors::MoveActor::room() const {
    return fieldB4;
}

unsigned actors::MoveActor::resolvePositionRoom() const {
    return construction.rooms.resolve(position, fieldB4);
}

void actors::MoveActor::setPosition(const std::array<float, 3>& value) {
    position = value;
    field5C = position;
    previousPosition = field5C;
    fieldB4 = construction.rooms.resolve(position, 0);
}

void actors::MoveActor::setRotation(const std::array<std::int16_t, 3>& value) {
    previousRotation = rotation = value;
}

void actors::MoveActor::setVelocity(const std::array<float, 3>& value) {
    velocity = value;
    speed = std::sqrt((velocity[0] * velocity[0] + velocity[1] * velocity[1]) + velocity[2] * velocity[2]);
    direction = velocity;
    if (speed > 0.000001f) {
        const float inverse = 1.0f / speed;
        for (auto& component : direction) component *= inverse;
    }
}

void actors::MoveActor::visibility(std::uint8_t primary, std::uint8_t secondary) {
    fieldE0 = (fieldE0 & 0xffffff00) | primary;
    fieldE4 = (fieldE4 & 0xffffff00) | secondary;
}

void actors::MoveActor::resetOwner(const jmp::ToolDataRef& source) {
    fieldE8 = 4;
    rotation.fill(0);
    previousRotation.fill(0);
    setVelocity({0, 0, 0});
    if (source.table && source.row >= 0) {
        std::uint8_t invisible = 0;
        source.table->getValue(source.row, "invisible", invisible);
        visibility(invisible ? 0 : 255, invisible ? 0 : 255);
    }
}

void actors::MoveActor::initializeType(unsigned type) {
    actorType = type;
    const auto& info = construction.catalog.type(type);
    const auto& entry = construction.resources.entry(info.model);
    flags = info.flags;
    fieldA4 = construction.globals.fieldA4;
    fieldA8 = construction.globals.fieldA8;
    animationState = 0xffffffff;
    selectAnimation(0);
    blendFramesRemaining = 0;
    fieldE0 = fieldE4 = construction.globals.color;
    model->model = entry.archive ? entry.archive->model : nullptr;
    model->resetCursors();
    fieldE4 = construction.parameters.parameters(type).entries().at(24).bits;
    scale.fill(1);
    fieldsE9.fill(64);
    fieldEC = 255;
    for (auto& channel : model->channels) channel.resource.reset();
    if (info.texture != -1) model->channels[15].resource = auxiliary(entry, 0, static_cast<std::uint32_t>(info.texture));
    field788 = nullptr;
}

void actors::MoveActor::selectAnimation(unsigned state) {
    if (animationState == state) return;
    animationState = state;
    const auto& type = construction.catalog.type(actorType);
    const auto& animation = type.animations.at(state);
    keyIndex = animation[0];
    const auto& metadata = construction.catalog.model(type.model).keyMetadata.at(keyIndex);
    const auto& entry = construction.resources.entry(type.model);
    if (fieldFA == -1) fieldF8 = static_cast<std::int16_t>(metadata[0]);
    model->field110 = metadata[4] == 0xffffffff ? nullptr : auxiliary(entry, 3, metadata[4]);
    model->materialFrame = 0;
    for (auto& channel : model->channels) channel.resource.reset();
    if (metadata[1] != 0xffffffff) model->channels[0].resource = auxiliary(entry, 0, metadata[1]);
    if (type.texture != -1) model->channels[15].resource = auxiliary(entry, 0, static_cast<std::uint32_t>(type.texture));
    if (animation[5] & 0x800) animationClock = animationFrame = 0;
    if (animation[5] & 2) {
        const auto key = construction.resources.key(actorType, state);
        if (!key) throw std::runtime_error("Reverse actor animation requires its original key resource.");
        animationFrame = static_cast<float>(int((unsigned(key->bytes.at(4)) << 8) | key->bytes.at(5)) - 1);
    }
    animationSpeed = 1;
    blendFramesRemaining = blendFramesTotal = static_cast<std::uint16_t>(animation[1] >> 16);
    flags &= 0xfff9ffff;
    sound.bind(metadata[5] & 0x80000000 ? nullptr : auxiliary(entry, 4, metadata[5]), animation[5] & 2 ? -1 : 1, 0);
    field160 = 1;
}

bool actors::MoveActor::selectAnimation(std::string_view name) {
    const auto state = construction.catalog.findAnimation(actorType, name);
    if (state < 0) return false;
    selectAnimation(static_cast<unsigned>(state));
    return true;
}

void actors::MoveActor::updateAnimationSound(const actoranimation::SoundServices& services) {
    const auto& animation = construction.catalog.type(actorType).animations.at(animationState);
    float rate;
    std::memcpy(&rate, &animation[6], sizeof(rate));
    sound.update(animationFrame, float(double(animationSpeed) * double(rate)), services);
}

void actors::MoveActor::advanceAnimation() {
    const auto previousFlags = flags;
    if (fieldF4) {
        fieldF0 += 1;
        const unsigned duration = (unsigned(fieldF4->bytes.at(0)) << 8) | fieldF4->bytes.at(1);
        if (fieldF0 >= float(duration)) fieldF4.reset();
    }
    if (!model->model) return;
    const auto key = construction.resources.key(actorType, animationState);
    if (!key) throw std::runtime_error("Actor animation update requires its original key resource.");
    const unsigned duration = (unsigned(key->bytes.at(4)) << 8) | key->bytes.at(5);
    const unsigned keyFlags = (unsigned(key->bytes.at(8)) << 24) | (unsigned(key->bytes.at(9)) << 16) |
        (unsigned(key->bytes.at(10)) << 8) | key->bytes.at(11);
    const auto& animation = construction.catalog.type(actorType).animations.at(animationState);
    float rate;
    std::memcpy(&rate, &animation[6], sizeof(rate));
    rate *= animationSpeed;
    if (blendFramesRemaining) --blendFramesRemaining;
    flags &= ~0x60000u;
    if ((animation[5] & 1) && !(flags & 0x1000)) {
        float factor = 0.025f * speed;
        if (factor > 1) factor = 1;
        rate *= factor;
        if (rate < 2) rate = 2;
        animationClock += rate;
        if (animationClock >= float(duration)) {
            flags |= 0x20000;
            animationClock -= float(duration);
        }
    } else if (!(keyFlags & 1)) {
        animationClock += rate;
        if (animationClock >= float(duration)) {
            flags |= 0x20000;
            animationClock -= float(duration);
        }
    } else if (!(previousFlags & 0x40000)) {
        animationClock += rate;
        if (animationClock >= float(duration)) {
            animationClock = float(duration);
            flags |= 0x40000;
        }
    } else {
        selectAnimation(animation[2]);
    }
    animationFrame = animationClock;
    if (construction.catalog.type(actorType).animations.at(animationState)[5] & 2)
        animationFrame = float(int(duration) - 1) - animationClock;
    animationFrameStep = rate;
}

void actors::MoveActor::finishFrame() {
    if (flags & 0x60) return;
    advanceAnimation();
    for (unsigned shift = 0; shift < 32; shift += 8) {
        unsigned current = (fieldE0 >> shift) & 255;
        const unsigned target = (fieldE4 >> shift) & 255;
        const unsigned distance = current < target ? target - current : current - target;
        if (distance <= fieldE8) current = target;
        else {
            if (current < target) current = (current + fieldE8) & 255;
            if (target < current) current = (current - fieldE8) & 255;
        }
        fieldE0 = (fieldE0 & ~(255u << shift)) | (current << shift);
    }
    model->color = fieldE0;
    previousPosition = position;
}

void actors::MoveActor::selectTexture(std::int16_t texture, unsigned channel) {
    auto& target = model->channels.at(channel);
    const auto& entry = construction.resources.entry(construction.catalog.type(actorType).model);
    const auto resource = texture == -1 ? nullptr : auxiliary(entry, 0, static_cast<std::uint32_t>(texture));
    if (texture == -1 || target.resource != resource) {
        target.resource = resource;
        target.frame = 0;
    }
}

std::uint32_t actors::MoveActor::hitDamage() const {
    return construction.parameters.parameters(actorType).entries()[1].bits;
}


void actors::MoveActor::followPath() {
    if (!fieldF4) throw std::runtime_error("Actor path update requires its retained path resource.");
    std::array<float, 3> current, origin;
    actoranimation::samplePath(*fieldF4, fieldF0, current, &rotation);
    actoranimation::samplePath(*fieldF4, 0, origin);
    for (unsigned axis = 0; axis < 3; ++axis) position[axis] = field5C[axis] + (current[axis] - origin[axis]);
    previousRotation = rotation;
}

void actors::MoveActor::approachRotation() {
    for (unsigned axis = 0; axis < 3; ++axis) {
        const auto difference = static_cast<std::uint16_t>(previousRotation[axis] - rotation[axis]);
        if (difference > 0x7ff && difference < 0xf800)
            rotation[axis] = static_cast<std::int16_t>(rotation[axis] + (difference < 0x8000 ? 0x800 : -0x800));
        else rotation[axis] = previousRotation[axis];
    }
}

void actors::MoveActor::copyLinkedPose() {
    if (!poseLinkedActor) return;
    poseLinkedActor->matrix = matrix;
    if (!model || !model->model || !poseLinkedActor->model) throw std::runtime_error("Linked actor pose requires both retained models.");
    const auto& bytes = model->model->bytes;
    const unsigned count = ((unsigned(bytes.at(8)) << 8) | bytes.at(9)) +
        ((unsigned(bytes.at(12)) << 8) | bytes.at(13));
    for (unsigned index = 0; index < count; ++index) poseLinkedActor->model->matrix(0, index) = model->matrix(0, index);
}
