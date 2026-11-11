#include "game/events/eventactorcommands.h"

#include <stdexcept>

namespace {
    constexpr unsigned stopped = 0x10;
    constexpr unsigned collisionDisabled = 0x80;
    constexpr unsigned maskHidden = 0x400;
    constexpr unsigned shadowHidden = 0x800;
    constexpr unsigned bodyHidden = 0x4000;
    constexpr unsigned displayHidden = 0xc000;
}

void gameevent::applyActorCommand(actors::MoveActor& actor, const ActorCommand& command, ActorServices& services) {
    switch (command.operation) {
    case ActorOperation::Animation:
        actor.selectAnimation(command.argument);
        break;
    case ActorOperation::Look: {
        auto* target = services.find(command.argument);
        if (!target) throw std::runtime_error("Event LOOK target is missing.");
        actor.lookAt(*target);
        if (actor.type() == 0) services.lookPlayer(*target);
        break;
    }
    case ActorOperation::Path:
        actor.selectPath(command.argument);
        break;
    case ActorOperation::Stop:
        actor.flags |= stopped;
        actor.setVelocity({});
        break;
    case ActorOperation::Start:
        actor.flags &= ~stopped;
        actor.setVelocity({});
        break;
    case ActorOperation::Sound:
        services.sound(actor, command.value & 65535);
        break;
    case ActorOperation::Position:
        actor.setPosition(command.position);
        break;
    case ActorOperation::CollisionOn:
        actor.flags &= ~collisionDisabled;
        break;
    case ActorOperation::CollisionOff:
        actor.flags |= collisionDisabled;
        break;
    case ActorOperation::Alpha:
        actor.fieldE4 = (actor.fieldE4 & 0xffffff00) | (command.value & 255);
        break;
    case ActorOperation::FixedAlpha:
        actor.fieldE0 = (actor.fieldE0 & 0xffffff00) | (command.value & 255);
        actor.fieldE4 = (actor.fieldE4 & 0xffffff00) | (command.value & 255);
        break;
    case ActorOperation::ShadowOn:
        actor.flags &= ~shadowHidden;
        break;
    case ActorOperation::ShadowOff:
        actor.flags |= shadowHidden;
        break;
    case ActorOperation::BodyOn:
        actor.flags &= ~bodyHidden;
        break;
    case ActorOperation::BodyOff:
        actor.flags |= bodyHidden;
        break;
    case ActorOperation::MaskOn:
        actor.flags &= ~maskHidden;
        break;
    case ActorOperation::MaskOff:
        actor.flags |= maskHidden;
        break;
    case ActorOperation::DisplayOn:
        actor.flags &= ~displayHidden;
        break;
    case ActorOperation::DisplayOff:
        actor.flags |= displayHidden;
        break;
    }
}
