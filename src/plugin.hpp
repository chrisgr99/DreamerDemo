#pragma once
#include <rack.hpp>

#include <string>

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

/** Inherited by every widget this plugin puts in the scene, so the resolver can tell them from
somebody else's floating window. Without it, "the window at the front" would find the transport,
or the card, or the list of scripts. */
struct OurWidget {
	virtual ~OurWidget() {}
};

/** The floating window at the front of the scene that belongs to somebody else — a module's own
window, such as the chart. Nothing, when there is none.

A DEMO HAS TO BE ABLE TO CLOSE ONE. A window opened by a module is not a module, so none of its
controls can be addressed by name, and the realistic way to shut one is the cross in its corner
rather than pressing the button that opened it a second time. */
widget::Widget* frontWindow();

/** Everything in the scene, named, for when the answer to "no window is open" is not obvious. */
std::string sceneContents();

/** The thing most recently put on the rack that is neither a module nor a cable — which is to
say, the widget a demo has just clipped onto a port.

A CLIP-ON WIDGET HAS NO NAME. It is not a module, so none of its controls can be addressed, and
a demo that can create one but not then press it can only show half of what it is for. What it
can be told is which one is newest, and during a demo that is the one just made. */
widget::Widget* frontRackWidget();

/** The narration card and the pointer theatre, created on first use and left in the scene.
Both are singletons: there is one demo running at a time, by construction. */
struct Card;
struct Theatre;
Card* card();
Theatre* theatre();

/** Puts the card and the pointer at the top of the scene, above the transport. Called when a run
starts, because either may have been added before the transport was opened.

ASKED FOR, NOT DONE ON THE SPOT. Reordering the scene means removing and re-adding children, and
a run starts from inside a button press — that is, while Rack is part way through dispatching
that very event to the scene's children. Rearranging the list underneath it is how a press on Run
ends up somewhere else entirely, picking up a cable from a jack behind the window. The work is
done on the next frame instead, which is soon enough for something nobody can see. */
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
