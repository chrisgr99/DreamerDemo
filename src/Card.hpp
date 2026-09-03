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


struct Card : widget::Widget {
	Card();

	/** Put a note up. `avoid` is the region the coming steps will work in, in scene
	coordinates; an empty rectangle means anywhere is fine. */
	void show(const std::string& text, math::Rect avoid);

	/** Pin the next note to a named berth, for the case where the computed one reads badly.
	Zero to five, in the preference order below; anything else returns to computing it. */
	void pin(int berth) { pinned = berth; }

	void hide();

	void step() override;
	void draw(const DrawArgs& args) override;

private:
	std::string text;
	math::Rect avoid;
	int pinned = -1;
	/** Chosen on the first draw after the text changes, because choosing needs the text
	measured and measuring needs a drawing context. */
	int berth = -1;
	double shownAt = 0.0, hiddenAt = 0.0;
	bool up = false;

	math::Rect berthRect(int which, math::Vec size) const;
};


} // namespace demo
