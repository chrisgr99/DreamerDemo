#include "Card.hpp"

namespace demo {


static const float WRAP = 560.f;      // the widest a line of narration is allowed to run
static const float PAD = 18.f;
static const float TEXT = 19.f;
static const float LINE = 25.f;
static const float MARGIN = 26.f;     // from the edge of the window
static const float FADE = 0.22f;

/** Six berths, in preference order. Bottom centre first: it is out of the way of a rack, which
fills from the top, and it is where a viewer already expects a caption. */
enum Berth { BOTTOM_CENTRE, TOP_CENTRE, BOTTOM_LEFT, BOTTOM_RIGHT, TOP_LEFT, TOP_RIGHT };
static const int BERTHS = 6;


Card::Card() {
	box.pos = math::Vec(0.f, 0.f);
	box.size = math::Vec(100.f, 100.f);
}


void Card::show(const std::string& t, math::Rect a) {
	if (up && t == text)
		return;   // the same note again is the same note; do not restart its fade
	text = t;
	avoid = a;
	isAlert = false;
	avoidControls = transportRect();
	berth = -1;
	up = true;
	shownAt = system::getTime();
}


void Card::alert(const std::string& t) {
	text = t;
	avoid = math::Rect();
	avoidControls = transportRect();
	isAlert = true;
	berth = -1;
	up = true;
	shownAt = system::getTime();
}


void Card::hide() {
	if (!up)
		return;
	up = false;
	isAlert = false;
	hiddenAt = system::getTime();
}


void Card::step() {
	box.pos = math::Vec(0.f, 0.f);
	box.size = APP->scene->box.size;
	widget::Widget::step();
}


math::Rect Card::berthRect(int which, math::Vec size) const {
	const float cx = (box.size.x - size.x) / 2.f;
	const float left = MARGIN;
	const float right = box.size.x - size.x - MARGIN;
	const float top = MARGIN;
	const float bottom = box.size.y - size.y - MARGIN;
	switch (which) {
		case BOTTOM_CENTRE: return math::Rect(math::Vec(cx, bottom), size);
		case TOP_CENTRE:    return math::Rect(math::Vec(cx, top), size);
		case BOTTOM_LEFT:   return math::Rect(math::Vec(left, bottom), size);
		case BOTTOM_RIGHT:  return math::Rect(math::Vec(right, bottom), size);
		case TOP_LEFT:      return math::Rect(math::Vec(left, top), size);
		default:            return math::Rect(math::Vec(right, top), size);
	}
}


void Card::draw(const DrawArgs& args) {
	if (!enabled && !isAlert)
		return;
	const double now = system::getTime();
	float alpha = 1.f;
	if (up)
		alpha = math::clamp((float) (now - shownAt) / FADE, 0.f, 1.f);
	else {
		alpha = 1.f - math::clamp((float) (now - hiddenAt) / FADE, 0.f, 1.f);
		if (alpha <= 0.f)
			return;
	}
	if (text.empty())
		return;

	std::shared_ptr<window::Font> font = uiFont();
	if (!font || font->handle < 0)
		return;

	nvgFontFaceId(args.vg, font->handle);
	nvgFontSize(args.vg, TEXT);
	nvgTextLineHeight(args.vg, LINE / TEXT);
	float bounds[4] = {0.f, 0.f, 0.f, 0.f};
	nvgTextBoxBounds(args.vg, 0.f, 0.f, WRAP, text.c_str(), NULL, bounds);
	const math::Vec size(std::fmin(WRAP, bounds[2] - bounds[0]) + PAD * 2.f,
		bounds[3] - bounds[1] + PAD * 2.f);

	// FIRST BERTH THAT DOES NOT OVERLAP the region the coming steps will touch. If every one of
	// them does, the first is used anyway: a card slightly over the action is better than no
	// caption, and the author can pin one when it reads badly.
	if (berth < 0) {
		berth = (pinned >= 0 && pinned < BERTHS) ? pinned : 0;
		if (pinned < 0) {
			// FIRST CHOICE: clear of the action AND clear of the transport. Second choice: clear
			// of the transport at least, since covering the controls is the one failure that
			// cannot be recovered from — a note that hides the buttons has no way down.
			int clearOfBoth = -1, clearOfControls = -1;
			for (int i = 0; i < BERTHS; i++) {
				const math::Rect r = berthRect(i, size);
				const bool overAction = avoid.size.x > 0.f && avoid.size.y > 0.f
					&& r.intersects(avoid);
				const bool overControls = avoidControls.size.x > 0.f
					&& avoidControls.size.y > 0.f && r.intersects(avoidControls);
				if (!overControls && clearOfControls < 0)
					clearOfControls = i;
				if (!overAction && !overControls) {
					clearOfBoth = i;
					break;
				}
			}
			berth = (clearOfBoth >= 0) ? clearOfBoth
				: (clearOfControls >= 0) ? clearOfControls : 0;
		}
	}

	const math::Rect r = berthRect(berth, size);

	nvgGlobalAlpha(args.vg, alpha);

	nvgBeginPath(args.vg);
	nvgRoundedRect(args.vg, r.pos.x, r.pos.y, r.size.x, r.size.y, 7.f);
	nvgFillColor(args.vg, CHIP);
	nvgFill(args.vg);
	nvgStrokeColor(args.vg, nvgRGBA(0xcf, 0xcf, 0xcf, 0xb0));
	nvgStrokeWidth(args.vg, 1.f);
	nvgStroke(args.vg);

	nvgFillColor(args.vg, INK);
	nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
	nvgTextBox(args.vg, r.pos.x + PAD, r.pos.y + PAD, WRAP, text.c_str(), NULL);

	nvgGlobalAlpha(args.vg, 1.f);

	widget::Widget::draw(args);
}


} // namespace demo
