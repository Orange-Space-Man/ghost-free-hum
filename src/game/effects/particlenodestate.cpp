#include "game/effects/particlenodestate.h"

particles::Node::Node() : next(this), previous(this) { reset(); }

void particles::Node::reset() {
    fields0.fill(0);
    fields30.fill(0);
    field34 = 0;
    fields38.fill(0);
    fields44.fill(0);
    field48 = 1;
    fields4C.fill(0);
    fields54.fill(0);
}

particles::ShortNode::ShortNode() : next(this), previous(this) {}
