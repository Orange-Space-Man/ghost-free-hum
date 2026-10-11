#include "game/actors/characteractorlinks.h"

characterownership::Links::Links(actors::MoveActor& actor, enemies::Manager& manager, jaudio::PositionOwners& sounds) :
    actor(actor), manager(manager), sounds(sounds) {}

void characterownership::Links::setInactiveFlags() { actor.flags |= 0xc420; }

void* characterownership::Links::child(unsigned index) { return actor.children.at(index).actor; }

void characterownership::Links::detach(void* child) {
    actor.detachChild(*static_cast<actors::MoveActor*>(child));
}

void characterownership::Links::deactivate(void* child) {
    manager.deactivate(static_cast<actors::MoveActor*>(child)->actorId);
}

void characterownership::Links::releaseAudio() { sounds.release(&actor.position); }
