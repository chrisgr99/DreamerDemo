#include "Theatre.hpp"

#include <GLFW/glfw3.h>

namespace demo {


const char* const GESTURES[7] = {
	"move pointer", "left click", "right click",
	"button down", "drag", "button up", "scroll wheel",
};

/** TWICE the size of a real pointer. This one is watched from across the room, or through a
magnified view, and at the size the operating system draws a cursor it is the smallest thing on
screen while being the thing to follow. */
static const float CURSOR = 2.0f;

static const float RIPPLE_LIFE = 0.75f;   // seconds
static const int RIPPLE_RINGS = 3;
static const float RIPPLE_STAGGER = 0.11f;

/** THE ONE NUMBER THE BADGE IS BUILT FROM. It is read at a glance, from across a room or through
a magnified view, while the pointer is the thing being watched — so it is set at caption size
rather than at the size of a tooltip. Everything else about the badge is a fraction of this. */
static const float BADGE_TEXT = 70.f;
static const float BADGE_GAP = BADGE_TEXT * 0.4f;   // pointer to the near corner of the badge


/** Smooth at both ends. A pointer that starts and stops abruptly reads as a jump cut, and the
eye loses it; easing is what makes it possible to follow one across a wide window. */
static float ease(float t) {
	t = math::clamp(t, 0.f, 1.f);
	return t * t * (3.f - 2.f * t);
}


Theatre::Theatre() {
	box.pos = math::Vec(0.f, 0.f);
	box.size = math::Vec(100.f, 100.f);
	cursor = math::Vec(200.f, 200.f);
}


void Theatre::placeAt(math::Vec pos) {
	cursor = pos;
	travelFrom = pos;
	travelTo_ = pos;
	travelStart = travelEnd = 0.0;
}


void Theatre::travelTo(math::Vec pos, float seconds) {
	// The badge goes to the side the pointer is LEAVING, so it trails behind the movement rather
	// than leading it into whatever is about to be shown.
	badgeLeft = pos.x >= cursor.x;
	travelFrom = cursor;
	travelTo_ = pos;
	travelStart = system::getTime();
	travelEnd = travelStart + std::fmax(0.001f, seconds);
}


bool Theatre::travelling() const {
	return system::getTime() < travelEnd;
}


void Theatre::setBadge(const std::string& text) {
	badge = text;
}


void Theatre::ripple() {
	Ripple r;
	r.pos = cursor;
	r.at = system::getTime();
	ripples.push_back(r);
}


void Theatre::glow(math::Rect rect, float seconds) {
	glowRect = rect;
	glowUntil = system::getTime() + seconds;
}


void Theatre::clear() {
	live = false;
	badge.clear();
	ripples.clear();
	glowUntil = 0.0;
}


void Theatre::step() {
	// Always the whole scene: the pointer may go anywhere, and a widget the size of the window
	// costs nothing when it draws almost nothing.
	box.pos = math::Vec(0.f, 0.f);
	box.size = APP->scene->box.size;

	const double now = system::getTime();
	if (now < travelEnd) {
		const float t = ease((float) ((now - travelStart) / (travelEnd - travelStart)));
		cursor = travelFrom.plus(travelTo_.minus(travelFrom).mult(t));
	}
	else {
		cursor = travelTo_;
	}

	while (!ripples.empty() && now - ripples.front().at > RIPPLE_LIFE + RIPPLE_STAGGER * RIPPLE_RINGS)
		ripples.erase(ripples.begin());

	// NOTHING CAN BE PERFORMING WITH NO TRANSPORT OPEN. This is a safety net rather than a rule:
	// a hidden cursor is invisible in the most literal way, and if any path ever fails to lower
	// this flag the pointer is gone for the rest of the session with nothing on screen to say
	// why. Losing the window is proof enough that the demo is over.
	if (transportRect().size.x <= 0.f) {
		live = false;
		running = false;
	}

	// THE REAL CURSOR IS HIDDEN FOR THE LENGTH OF A TAKE, so a recording contains one pointer
	// rather than two. Rack draws its cursor through the operating system, so this is the only
	// way to be rid of it, and it has to be put back the moment the run ends.
	static bool hidden = false;
	if (APP->window && APP->window->win && hidden != live) {
		glfwSetInputMode(APP->window->win, GLFW_CURSOR,
			live ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);
		hidden = live;
	}

	widget::Widget::step();
}


void Theatre::onHover(const HoverEvent& e) {
	// SWALLOWED WHILE RUNNING. The viewer's own mouse is lying wherever they left it; if it so
	// much as twitches, Rack hovers whatever is under it and lights a control the synthetic
	// pointer is nowhere near. Then the picture has two opinions about where the pointer is.
	if (live && !injecting) {
		e.consume(this);
		return;
	}
	widget::Widget::onHover(e);
}


void Theatre::drawCursor(NVGcontext* vg, math::Vec p) {
	// An arrow, in the proportions every screen recording uses, drawn rather than loaded so it
	// scales cleanly and needs no artwork shipped with the plugin.
	static const float PTS[7][2] = {
		{3.f, 2.f}, {3.f, 21.f}, {8.f, 16.f}, {11.5f, 23.f},
		{14.5f, 21.8f}, {11.f, 15.f}, {17.5f, 15.f},
	};

	for (int pass = 0; pass < 2; pass++) {
		// The first pass is the shadow: the same shape, offset and blurred by being drawn fat
		// and dark. Without it the pointer disappears over a light panel.
		nvgSave(vg);
		nvgTranslate(vg, p.x + (pass == 0 ? 1.5f : 0.f), p.y + (pass == 0 ? 2.5f : 0.f));
		nvgScale(vg, CURSOR, CURSOR);
		nvgBeginPath(vg);
		nvgMoveTo(vg, PTS[0][0], PTS[0][1]);
		for (int i = 1; i < 7; i++)
			nvgLineTo(vg, PTS[i][0], PTS[i][1]);
		nvgClosePath(vg);
		if (pass == 0) {
			nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 0x55));
			nvgStrokeWidth(vg, 3.f);
			nvgLineJoin(vg, NVG_ROUND);
			nvgStroke(vg);
			nvgFillColor(vg, nvgRGBA(0, 0, 0, 0x55));
			nvgFill(vg);
		}
		else {
			nvgFillColor(vg, nvgRGB(0xff, 0xff, 0xff));
			nvgFill(vg);
			nvgStrokeColor(vg, nvgRGB(0x11, 0x11, 0x11));
			nvgStrokeWidth(vg, 1.3f);
			nvgLineJoin(vg, NVG_ROUND);
			nvgStroke(vg);
		}
		nvgRestore(vg);
	}
}


void Theatre::drawBadge(NVGcontext* vg, math::Vec p) {
	std::shared_ptr<window::Font> font = uiFont();
	if (!font || font->handle < 0)
		return;

	nvgFontFaceId(vg, font->handle);
	nvgFontSize(vg, BADGE_TEXT);
	float bounds[4] = {0.f, 0.f, 0.f, 0.f};
	nvgTextBounds(vg, 0.f, 0.f, badge.c_str(), NULL, bounds);
	const float w = bounds[2] - bounds[0] + BADGE_TEXT * 0.46f;
	const float h = BADGE_TEXT * 1.34f;

	// Beside the pointer, on the side it is not travelling towards, and flipped back on itself
	// rather than allowed off the edge of the window.
	float x = badgeLeft ? p.x - BADGE_GAP - w : p.x + BADGE_GAP;
	if (x < 4.f)
		x = p.x + BADGE_GAP;
	if (x + w > box.size.x - 4.f)
		x = p.x - BADGE_GAP - w;
	const float y = math::clamp(p.y - h / 2.f + 6.f, 4.f, box.size.y - h - 4.f);

	nvgBeginPath(vg);
	nvgRoundedRect(vg, x, y, w, h, BADGE_TEXT * 0.12f);
	nvgFillColor(vg, CHIP);
	nvgFill(vg);
	nvgStrokeColor(vg, nvgRGB(0xcf, 0xcf, 0xcf));
	nvgStrokeWidth(vg, 1.5f);
	nvgStroke(vg);

	nvgFillColor(vg, INK);
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	nvgText(vg, x + BADGE_TEXT * 0.23f, y + h / 2.f + 0.5f, badge.c_str(), NULL);
}


void Theatre::draw(const DrawArgs& args) {
	const double now = system::getTime();

	// The glow around the control being acted on, under everything else.
	if (now < glowUntil) {
		const float left = (float) (glowUntil - now);
		const float a = math::clamp(left / 0.35f, 0.f, 1.f);
		for (int i = 0; i < 3; i++) {
			const float grow = 2.f + i * 3.f;
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, glowRect.pos.x - grow, glowRect.pos.y - grow,
				glowRect.size.x + grow * 2.f, glowRect.size.y + grow * 2.f, 4.f + grow);
			nvgStrokeColor(args.vg, nvgRGBA((unsigned char) (ACCENT.r * 255),
				(unsigned char) (ACCENT.g * 255), (unsigned char) (ACCENT.b * 255),
				(unsigned char) (a * (110 - i * 30))));
			nvgStrokeWidth(args.vg, 2.5f);
			nvgStroke(args.vg);
		}
	}

	// The rings, which start at the pointer and travel outwards.
	for (size_t i = 0; i < ripples.size(); i++) {
		for (int k = 0; k < RIPPLE_RINGS; k++) {
			const float t = (float) (now - ripples[i].at) / RIPPLE_LIFE - k * RIPPLE_STAGGER;
			if (t < 0.f || t > 1.f)
				continue;
			const float r = 5.f + t * 26.f;
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, ripples[i].pos.x, ripples[i].pos.y, r);
			nvgStrokeColor(args.vg, nvgRGBA((unsigned char) (ACCENT.r * 255),
				(unsigned char) (ACCENT.g * 255), (unsigned char) (ACCENT.b * 255),
				(unsigned char) ((1.f - t) * 240)));
			nvgStrokeWidth(args.vg, 2.5f);
			nvgStroke(args.vg);
		}
	}

	if (badges && !badge.empty())
		drawBadge(args.vg, cursor);

	if (running)
		drawCursor(args.vg, cursor);

	widget::Widget::draw(args);
}


} // namespace demo
