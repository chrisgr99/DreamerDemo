#pragma once
/** The narration card: one text place, floating over the rack.

OVER, NOT BESIDE. Docking the text would take width from the thing being demonstrated, and a
separate operating-system window would not appear in a recording at all. So it is a layer inside
Rack's own scene, above the rack, taking no space and no events.

IT IS PLACED, NOT DRAGGED. The runner knows every step's target in advance, so it can say which
region the coming steps will touch and the card takes a berth clear of it. The berth is chosen
once, when the note goes up, and does not move while that note is up: a card that shuffles about
while you are reading it is worse than one that briefly overlaps something.
*/
#include "plugin.hpp"

namespace demo {


struct Card : widget::Widget, OurWidget {
	/** WHETHER THE CARD IS SHOWN AT ALL. Experience with the same system in DreamRack was that a
	demo reads better with the captions and the badges both off and the speech carrying the
	action: written narration and spoken narration are the same words twice, and the eye stops
	watching the thing being demonstrated in order to read.

	The pacing does not change either way — a note's hold is how long its sentence takes, spoken
	or not — so a script runs to the same length with the card up or down. */
	bool enabled = true;

	Card();

	/** Put a note up. `avoid` is the region the coming steps will work in, in scene
	coordinates; an empty rectangle means anywhere is fine. */
	void show(const std::string& text, math::Rect avoid);

	/** Say something that is not narration, whether or not captions are on.

	SCRIPTS ARE NORMALLY RUN WITH CAPTIONS OFF, because the speech carries the words. So nothing
	the viewer must be told can go through the ordinary note: a failure written on a card that is
	switched off is a failure nobody hears about, and the run just stops looking as though it did
	nothing. This ignores the switch without changing it, so the next run is still captioned the
	way its script asked. */
	void alert(const std::string& text);

	/** Pin the next note to a named berth, for the case where the computed one reads badly.
	Zero to five, in the preference order below; anything else returns to computing it. */
	void pin(int berth) { pinned = berth; }

	void hide();

	void step() override;
	void draw(const DrawArgs& args) override;

private:
	std::string text;
	math::Rect avoid;
	/** The transport, which the card must never cover, because the way out of a message is to
	press a button and a covered button cannot be pressed. Held apart from `avoid` rather than
	merged with it: two rectangles at opposite ends of the window merge into one that covers
	everything between them, and then no berth is free. */
	math::Rect avoidControls;
	/** True while what is up is a message rather than narration. */
	bool isAlert = false;
	int pinned = -1;
	/** Chosen on the first draw after the text changes, because choosing needs the text
	measured and measuring needs a drawing context. */
	int berth = -1;
	double shownAt = 0.0, hiddenAt = 0.0;
	bool up = false;

	math::Rect berthRect(int which, math::Vec size) const;
};


} // namespace demo
