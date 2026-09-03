#pragma once
/** The visible surface of a demo: a pointer, a badge, a ripple and a glow.

PURE PRESENTATION. Nothing here changes anything in Rack. The pointer is theatre drawn over the
top of state changes that happen elsewhere, which is what lets the two be reasoned about
separately — the choreography can be wrong without the demo building the wrong patch.

ANNOUNCE, THEN DO. The runner puts the pointer over the control, waits, raises the badge naming
the gesture, waits again, and only then acts. This widget provides the pieces; the ordering is
the runner's.

Everything is in SCENE coordinates. A control's position is worked out from its widget and
converted once, so nothing here knows about the rack's zoom or scroll.
*/
#include "plugin.hpp"

namespace demo {


/** The whole gesture vocabulary. A demo that needs an eighth word needs a discussion, not a new
string — the badge is read at a glance, and a set of words that grows stops being learnable. */
extern const char* const GESTURES[7];


struct Theatre : widget::Widget {
	/** True while a demo is running: the real cursor is hidden and real mouse movement is
	swallowed, so Rack's own hover highlight cannot follow a pointer the viewer cannot see. */
	bool running = false;
	/** Set for the moment a gesture is injected into Rack's event system, so the injected hover
	passes through this widget instead of being eaten by it. Phase two uses it. */
	bool injecting = false;

	Theatre();

	/** Where the pointer is now, in scene coordinates. */
	math::Vec at() const { return cursor; }

	/** Put the pointer somewhere with no travel. Used at the start of a run. */
	void placeAt(math::Vec pos);

	/** Travel there over `seconds`, easing in and out. */
	void travelTo(math::Vec pos, float seconds);

	/** True while a travel is still running. */
	bool travelling() const;

	/** Raise the badge, or lower it with an empty string. */
	void setBadge(const std::string& text);

	/** Rings radiating from the pointer. One ring is a blink you can miss; three, staggered,
	read as a press even out of the corner of the eye. */
	void ripple();

	/** Ring a control for a moment, so the thing being acted on is named by more than the
	pointer sitting on it. In scene coordinates. */
	void glow(math::Rect rect, float seconds);

	/** Take everything down. */
	void clear();

	void step() override;
	void draw(const DrawArgs& args) override;
	void onHover(const HoverEvent& e) override;

private:
	math::Vec cursor;
	math::Vec travelFrom, travelTo_;
	double travelStart = 0.0, travelEnd = 0.0;

	std::string badge;
	/** Which side of the pointer the badge sits on: it takes the side AWAY from where the
	pointer last travelled, so it never covers what you were just shown. */
	bool badgeLeft = false;

	struct Ripple {
		math::Vec pos;
		double at = 0.0;
	};
	std::vector<Ripple> ripples;

	math::Rect glowRect;
	double glowUntil = 0.0;

	void drawCursor(NVGcontext* vg, math::Vec p);
	void drawBadge(NVGcontext* vg, math::Vec p);
};


} // namespace demo
