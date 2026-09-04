#pragma once
#include <rack.hpp>

using namespace rack;

extern Plugin* pluginInstance;
extern Model* modelDemo;

namespace demo {


/** The look, in one place. Everything this plugin draws sits OVER somebody else's rack, so it
has to read as one layer rather than as several things that happen to be on top. Dark chips with
a hairline light border, white lettering, and one accent used only where the eye is being sent. */
extern const NVGcolor INK;       /**< lettering and hairlines */
extern const NVGcolor CHIP;      /**< the ground of a badge, a card or the transport */
extern const NVGcolor ACCENT;    /**< the click ripple and the glow: where to look */

/** Rack's own UI face, so a badge over Rack's interface is set in Rack's lettering. */
std::shared_ptr<window::Font> uiFont();

/** The narration card and the pointer theatre, created on first use and left in the scene.
Both are singletons: there is one demo running at a time, by construction. */
struct Card;
struct Theatre;
Card* card();
Theatre* theatre();

/** Puts the card and the pointer at the top of the scene, above the transport. Called when a run
starts, because either may have been added before the transport was opened. */
void raiseTheatre();

/** Opens the transport, or brings it forward if it is already up. */
void transportShow();

/** THE TRANSPORT STANDS OUT OF THE WAY of the region a demo is about to work in. It is an opaque
window over the rack, so a click aimed at a control underneath it would land on the transport
instead. Called once per note, with the region that note's steps will touch. */
void transportStepAside(math::Rect region);

/** Where the transport is, in scene coordinates, or an empty rectangle when it is not open. The
card keeps clear of it: a note that covers the controls cannot be dismissed by pressing one. */
math::Rect transportRect();


} // namespace demo
