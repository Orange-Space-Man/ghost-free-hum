#include "game/actors/characterownership.h"

#include <stdexcept>

characterownership::Acquired characterownership::acquire(unsigned type, Resources& resources) {
    Acquired result;
    const std::array<float, 3> position{}, vector{};
    if (type == 0x56 || (type >= 0x57 && type <= 0x5a)) {
        result.second = resources.createSecondary(type == 0x56 ? 0x3c : 0x3d, position, vector);
        result.success = true;
        return result;
    }
    if (type == 0xbd) {
        result.first = resources.createSecondary(0x3c, position, vector);
        result.success = result.first != nullptr;
        return result;
    }
    if (type == 0xcd || type == 0xce) {
        unsigned resource = 0x1b;
        if (type == 0xcd) {
            const std::int32_t threshold = resources.gameVariant() == 0 ? 200 : 400;
            resource = resources.playerValue() < threshold ? 0x19 : 0x1a;
        }
        result.first = resources.createPrimary(resource, position);
        result.success = result.first != nullptr;
        return result;
    }
    unsigned first = 0, second = 0;
    switch (type) {
    case 0x75: first = 0x1d; second = 0x56; break;
    case 0x76: first = 0x1e; second = 0x57; break;
    case 0x77: first = 0x1f; second = 0x58; break;
    case 0xc2: first = 0x16; second = 0x4e; break;
    case 0xc3: first = 0x17; second = 0x4f; break;
    case 0xc4: first = 0x18; second = 0x50; break;
    case 0xc5: first = 0x15; second = 0x4d; break;
    case 0xc6: first = 0x13; second = 0x4b; break;
    case 0xc7: first = 0x14; second = 0x4c; break;
    case 0xc8: first = 0x0f; second = 0x47; break;
    case 0xc9: first = 0x10; second = 0x48; break;
    case 0xca: first = 0x11; second = 0x49; break;
    case 0xcb: first = 0x12; second = 0x4a; break;
    case 0xcc: first = 0x1c; second = 0x4c; break;
    default: result.success = true; return result;
    }
    result.first = resources.createPrimary(first, position);
    result.second = resources.createSecondary(second, position, vector);
    result.success = result.first && result.second;
    return result;
}

void characterownership::Ownership::reset() {
    sharedUsers = {};
    sharedResource = nullptr;
    sharedCount = 0;
    dependentActor = nullptr;
    attachmentCount = 0;
    strategy.reset();
}

bool characterownership::Ownership::acquire(unsigned type, Resources& resources, Acquired& acquired) {
    acquired = characterownership::acquire(type, resources);
    if (acquired.success) {
        return true;
    }
    if (acquired.first) {
        acquired.first->release();
    }
    if (acquired.second) {
        acquired.second->release();
    }
    return false;
}

void characterownership::Ownership::attach(Resource* resource) {
    if (resource && resource->attachmentState() == 0 && attachmentCount < attachments.size()) {
        attachments[attachmentCount++] = {resource, 0, {}};
    }
}

void characterownership::Ownership::attach(const Acquired& acquired) {
    attach(acquired.first);
    attach(acquired.second);
}

void characterownership::Ownership::releaseShared(void*& user) {
    user = nullptr;
    sharedCount = static_cast<std::int32_t>(static_cast<std::uint32_t>(sharedCount) - 1);
    if (sharedCount < 1) {
        if (!sharedResource) throw std::runtime_error("Original shared character resource is null during release.");
        sharedResource->release();
        sharedResource = nullptr;
    }
}

void characterownership::Ownership::deactivate(ActorLinks& actor) {
    strategy.reset();
    actor.setInactiveFlags();
    for (auto& user : sharedUsers) {
        if (!user) {
            continue;
        }
        releaseShared(user);
    }
    const unsigned count = attachmentCount;
    if (count > attachments.size()) {
        throw std::runtime_error("Invalid original character attachment count.");
    }
    for (unsigned i = 0; i < count; ++i) {
        if (!attachments[i].resource) {
            throw std::runtime_error("Null original character attachment during release.");
        }
        attachments[i].resource->release();
    }
    for (unsigned i = 0; i < 7; ++i) {
        if (auto* child = actor.child(i)) {
            actor.detach(child);
            actor.deactivate(child);
        }
    }
    for (unsigned i = 0; i < 7; ++i) {
        if (actor.child(i) == dependentActor) {
            if (dependentActor) {
                actor.deactivate(dependentActor);
            }
            break;
        }
    }
    dependentActor = nullptr;
    actor.releaseAudio();
}
