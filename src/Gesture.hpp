#pragma once
/** The gestures, injected into Rack's own event system.

`APP->event->handleButton`, `handleHover` and `handleScroll` are the functions Rack's own mouse
callbacks call. A gesture sent through them does exactly what a person doing it would do, which
is why menus, cable drags, module dragging and the module browser all work here without a line of
code apiece.

THE PRICE IS THAT A GESTURE CAN MISS. A click two pixels off a jack does nothing, and a demo that
carried on regardless would ruin a take silently. So every step that claims to change something
checks afterwards that it did — the cable exists, the parameter moved — and a failure stops the
run and says which step.

ONE THING IS NOT INJECTED. A parameter is set by writing the value, not by dragging the knob
through it. How far a knob turns for a given movement is the knob's own business — its range, its
sensitivity, whether it is a switch — so a drag long enough to reach a value on one control
overshoots on the next, and neither is knowable from a script. The pointer still shows a drag;
what the drag produces is exact. This is the one place where the theatre and the behaviour are
deliberately different things.
*/
#include "plugin.hpp"
#include "Resolve.hpp"

namespace demo {


/** Positions are in SCENE coordinates, the same ones Rack's window hands its own callbacks. */
void gHover(math::Vec pos, math::Vec delta);
void gPress(math::Vec pos, int button);
void gRelease(math::Vec pos, int button);
void gClick(math::Vec pos, int button);

/** A click that spans a frame rather than happening inside one.

A press and a release in the same frame are invisible to anything watching a parameter for an
edge, and a momentary button is exactly that — it rises on the press and falls on the release, so
a module stepping once per frame sees it at rest both times. */
void gClickHeld(math::Vec pos, int button);

/** Whether the widget Rack believes is under the pointer is this one, or something inside it.

A CLICK CAN LAND ON THE WRONG THING. Injecting a press at a control's position sends it to
whatever is topmost there, and a window opened over the rack — the chart's own, a menu, this
plugin's transport — is topmost. The press then does something else entirely, or nothing, and the
demo carries on believing it pressed a button. Asked before every click, this turns that into a
failure with a name. */
bool gHoveredIs(widget::Widget* want);

/** What Rack believes is under the pointer, named, for the log. A demo that does nothing visible
is otherwise impossible to argue with; this says what the click was actually offered. */
std::string gHoveredName();
void gScroll(math::Vec pos, math::Vec delta);

/** Put a parameter at a fraction of its own range, without touching the widget. */
void gSetParam(const Target& t, float unit);

/** Where a parameter stands now, as a fraction of its range. */
float gParamUnit(const Target& t);

/** Whether a cable joins these two ports. The question every patch step asks itself. */
bool gCableExists(engine::Module* outModule, int outId,
	engine::Module* inModule, int inId);

/** How many cables are on this port. A patch step counts before and after rather than trusting
that the cable it meant to make is the only one there. */
int gCableCount(const Target& port);


} // namespace demo
