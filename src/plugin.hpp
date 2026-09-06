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

/** THE CONTROLS THAT LIVE ON THE MODULE'S PANEL.

A demo is set up and driven in two quite different situations. Choosing a script, reloading it,
turning the voice or the badges off — that is done with the rack sitting still, it is nobody's
business but the author's, and it wants a permanent place: the module's own panel, which is
where a Rack user looks for a module's controls. Driving a take wants the opposite: three
buttons that never move and never hide, in a corner, over whatever the demo is doing.

These are the panel's half. They are answered without the transport window existing, and pressing
one opens it, since that is where the state lives. */
int demoPanelRows();
std::string demoPanelLabel(int row);
bool demoPanelLit(int row);
bool demoPanelIsField(int row);
void demoPanelPress(int row, math::Rect anchorScene);
/** Performs a pending panel press. Called from the runner's step, and from the module's while
there is no runner window yet — never from an event, since what these do is destroy and rebuild
the rack that delivered it. */
void demoPanelPump();
/** The transport's own chip and field, so the panel is drawn in the same hand. */
void demoDrawChip(NVGcontext* vg, math::Rect r, const std::string& label, bool lit);
void demoDrawField(NVGcontext* vg, math::Rect r, const std::string& label);

/** THE TRANSPORT STANDS OUT OF THE WAY of the region a demo is about to work in. It is an opaque
window over the rack, so a click aimed at a control underneath it would land on the transport
instead. Called once per note, with the region that note's steps will touch. */
void transportStepAside(math::Rect region);

/** Whether the transport exists at all.

NOT THE SAME AS BEING ON SCREEN. The transport takes itself out of the picture while a run is
going, which is what makes a clean recording — so "no transport visible" is the ordinary state
during a take, and anything that treats it as proof the demo is over will end the demo the
moment it begins. */
bool transportOpen();

/** Where the transport is, in scene coordinates, or an empty rectangle when it is not open. The
card keeps clear of it: a note that covers the controls cannot be dismissed by pressing one. */
math::Rect transportRect();


} // namespace demo
