/** The transport: the window a demo is driven from, and the stage it is drawn on.

One window, wide and short, dragged by its own background and remembered where you put it. It is
the only interface this plugin has; the module exists to open it.

NOTHING PAUSES IN FLIGHT, and STOP IS TWO-STAGE. Stopping a running demo leaves you standing on
the step it reached, with the rack as the demo built it, so you can step back or look at what it
did. Pressing Stop again puts things back. That is what stopping is for while authoring, and it
costs a viewer nothing.
*/
#include "Runner.hpp"
#include "Theatre.hpp"
#include "Card.hpp"

#include <GLFW/glfw3.h>

namespace demo {


static const float T_TITLE = 22.f;
static const float T_W = 486.f;
static const float T_H = 76.f;
static const float BTN_H = 30.f;
static const float BTN_Y = T_TITLE + 12.f;
static const float BTN_GAP = 8.f;
static const float BTN_PAD = 12.f;

enum Button { B_RUN, B_RESTART, B_BACK, B_STEP, B_RATE, B_COUNT };
static const float BTN_W[B_COUNT] = {74.f, 86.f, 62.f, 62.f, 92.f};

static const float RATES[4] = {0.75f, 1.0f, 1.5f, 2.0f};


/** Where the window was last put. A transport that opens in the middle of the rack every time
would be moved out of the way every time. */
static math::Vec gWhere = math::Vec(-1.f, -1.f);


// ---------------------------------------------------------------- the stage

static Card* gCard = NULL;
static Theatre* gTheatre = NULL;

Card* card() {
	if (!gCard) {
		gCard = new Card;
		APP->scene->addChild(gCard);
	}
	return gCard;
}

Theatre* theatre() {
	if (!gTheatre) {
		gTheatre = new Theatre;
		APP->scene->addChild(gTheatre);
	}
	return gTheatre;
}

/** The card under the pointer, and both over the transport. Re-adding a child is how Rack
reorders one, and drawing order is the order of the list. */
void raiseTheatre() {
	Card* c = card();
	Theatre* t = theatre();
	APP->scene->removeChild(c);
	APP->scene->addChild(c);
	APP->scene->removeChild(t);
	APP->scene->addChild(t);
}


// ---------------------------------------------------------------- phase one's script

/** SCAFFOLDING, and the only part of phase one that is thrown away. It exercises every piece of
the surface — a note taking a berth away from the action, a pointer announcing its move and
travelling, a badge naming a gesture, a ripple, a glow — with positions given as fractions of the
window, because control resolution is phase two.

The prose is what a real script's prose looks like: what is happening, in the app's own words. */
static std::vector<Step> cannedScript() {
	std::vector<Step> s;

	Step title;
	title.note = "This is the demo transport. Everything below is drawn over the rack: "
		"the pointer, the label beside it, and this card.";
	title.wait = 1.2f;
	s.push_back(title);

	Step a;
	a.note = "The pointer announces where it is going before it goes, and waits when it "
		"arrives. You are told what is about to happen, and then it happens.";
	a.frac = math::Vec(0.22f, 0.30f);
	a.gesture = "left click";
	a.act = true;
	a.glowFrac = math::Rect(math::Vec(0.20f, 0.28f), math::Vec(0.05f, 0.05f));
	s.push_back(a);

	Step b;
	b.frac = math::Vec(0.70f, 0.24f);
	b.gesture = "left click";
	b.act = true;
	b.glowFrac = math::Rect(math::Vec(0.68f, 0.22f), math::Vec(0.05f, 0.05f));
	s.push_back(b);

	Step c;
	c.note = "A note stays up until another note replaces it, so one card can cover several "
		"steps. This one covered two.";
	c.frac = math::Vec(0.50f, 0.55f);
	c.gesture = "drag";
	c.act = true;
	c.glowFrac = math::Rect(math::Vec(0.44f, 0.50f), math::Vec(0.12f, 0.10f));
	c.wait = 1.0f;
	s.push_back(c);

	Step d;
	d.note = "The card takes a berth clear of wherever the coming steps are going to work, and "
		"stays there while you read it.";
	d.frac = math::Vec(0.16f, 0.20f);
	d.gesture = "scroll wheel";
	d.act = true;
	d.wait = 1.4f;
	s.push_back(d);

	return s;
}


// ---------------------------------------------------------------- the window

struct Transport : widget::OpaqueWidget {
	Runner runner;
	/** Stop is two-stage, and this is which stage the next press is. */
	bool stopped = false;
	math::Vec dragStart;

	Transport() {
		box.size = math::Vec(T_W, T_H);
		runner.load(cannedScript());
	}

	math::Rect closeLeft() {
		return math::Rect(math::Vec(0.f, 0.f), math::Vec(T_TITLE, T_TITLE));
	}

	math::Rect closeRight() {
		return math::Rect(math::Vec(box.size.x - T_TITLE, 0.f), math::Vec(T_TITLE, T_TITLE));
	}

	math::Rect buttonRect(int i) {
		float x = BTN_PAD;
		for (int k = 0; k < i; k++)
			x += BTN_W[k] + BTN_GAP;
		return math::Rect(math::Vec(x, BTN_Y), math::Vec(BTN_W[i], BTN_H));
	}

	std::string buttonLabel(int i) {
		switch (i) {
			case B_RUN: return runner.isRunning() ? "Stop" : (stopped ? "Reset" : "Run");
			case B_RESTART: return "Restart";
			case B_BACK: return "Back";
			case B_STEP: return "Step";
			default: {
				char buf[24];
				std::snprintf(buf, sizeof(buf), "Rate %.2gx", runner.rate);
				return buf;
			}
		}
	}

	void press(int i) {
		switch (i) {
			case B_RUN:
				if (runner.isRunning()) {
					// First press: stand still, with the rack as the demo left it.
					runner.stop();
					stopped = true;
				}
				else if (stopped) {
					// Second press: put everything back. In phase one that is the pointer and
					// the card; from phase three it is the user's own patch.
					card()->hide();
					theatre()->clear();
					stopped = false;
				}
				else {
					runner.run();
				}
				break;
			case B_RESTART:
				stopped = false;
				runner.restart();
				break;
			case B_BACK:
				stopped = true;
				runner.back();
				break;
			case B_STEP:
				stopped = true;
				runner.stepOnce();
				break;
			default: {
				int k = 0;
				for (int j = 0; j < 4; j++) {
					if (RATES[j] == runner.rate)
						k = (j + 1) % 4;
				}
				runner.rate = RATES[k];
				break;
			}
		}
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (closeLeft().contains(e.pos) || closeRight().contains(e.pos)) {
				requestDelete();
				e.consume(this);
				e.stopPropagating();
				return;
			}
			for (int i = 0; i < B_COUNT; i++) {
				if (buttonRect(i).contains(e.pos)) {
					press(i);
					e.consume(this);
					e.stopPropagating();
					return;
				}
			}
		}
		OpaqueWidget::onButton(e);
	}

	void onDragStart(const DragStartEvent& e) override {
		dragStart = box.pos;
		OpaqueWidget::onDragStart(e);
	}

	void onDragMove(const DragMoveEvent& e) override {
		// The whole window is its own handle. There is nothing in it worth dragging by a strip.
		box.pos = box.pos.plus(e.mouseDelta);
		gWhere = box.pos;
		OpaqueWidget::onDragMove(e);
	}

	/** ESCAPE CLOSES IT, wherever the pointer is and whatever is selected. Rack sends a key to
	the selected widget or to whatever the pointer is over, and neither is this window when
	somebody glances at it and reaches for Escape — so it asks the keyboard itself once a frame,
	and stands down while a menu or a text field has it. */
	bool escapeWasDown = false;

	void step() override {
		runner.tick();
		// THE POINTER IS UP FOR AS LONG AS THE DEMO OWNS THE SCREEN, which includes standing
		// stopped on a step. It goes away, and the real cursor comes back, only when the demo
		// has been put back.
		theatre()->running = runner.isRunning() || stopped;

		if (APP->window && APP->window->win) {
			const bool down = glfwGetKey(APP->window->win, GLFW_KEY_ESCAPE) == GLFW_PRESS;
			const bool wasDown = escapeWasDown;
			escapeWasDown = down;
			if (down && !wasDown && !dynamic_cast<ui::TextField*>(
					APP->event->getSelectedWidget())) {
				bool menuUp = false;
				for (widget::Widget* child : APP->scene->children) {
					ui::MenuOverlay* overlay = dynamic_cast<ui::MenuOverlay*>(child);
					if (overlay && overlay->visible && !overlay->requestedDelete)
						menuUp = true;
				}
				if (!menuUp) {
					requestDelete();
					return;
				}
			}
		}
		OpaqueWidget::step();
	}

	void onHoverKey(const HoverKeyEvent& e) override {
		if (e.action == GLFW_PRESS && e.key == GLFW_KEY_ESCAPE) {
			e.consume(this);
			e.stopPropagating();
			requestDelete();
			return;
		}
		OpaqueWidget::onHoverKey(e);
	}

	void drawChip(NVGcontext* vg, math::Rect r, const std::string& label, bool lit) {
		nvgBeginPath(vg);
		nvgRoundedRect(vg, r.pos.x, r.pos.y, r.size.x, r.size.y, 4.f);
		nvgFillColor(vg, lit ? nvgRGB(0x3a, 0x2c, 0x18) : nvgRGB(0x22, 0x22, 0x26));
		nvgFill(vg);
		nvgStrokeColor(vg, lit ? ACCENT : nvgRGBA(0xcf, 0xcf, 0xcf, 0x90));
		nvgStrokeWidth(vg, 1.f);
		nvgStroke(vg);

		std::shared_ptr<window::Font> font = uiFont();
		if (!font || font->handle < 0)
			return;
		nvgFontFaceId(vg, font->handle);
		nvgFontSize(vg, 14.f);
		nvgFillColor(vg, lit ? ACCENT : INK);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(vg, r.pos.x + r.size.x / 2.f, r.pos.y + r.size.y / 2.f + 0.5f,
			label.c_str(), NULL);
	}

	void drawCross(NVGcontext* vg, math::Rect r) {
		const float m = 7.f;
		nvgBeginPath(vg);
		nvgMoveTo(vg, r.pos.x + m, r.pos.y + m);
		nvgLineTo(vg, r.pos.x + r.size.x - m, r.pos.y + r.size.y - m);
		nvgMoveTo(vg, r.pos.x + r.size.x - m, r.pos.y + m);
		nvgLineTo(vg, r.pos.x + m, r.pos.y + r.size.y - m);
		nvgStrokeColor(vg, INK);
		nvgStrokeWidth(vg, 2.f);
		nvgStroke(vg);
	}

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 6.f);
		nvgFillColor(args.vg, CHIP);
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, INK);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		// The title strip, a shade lighter, with a rule under it.
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, T_TITLE, 6.f);
		nvgRect(args.vg, 0.f, T_TITLE - 6.f, box.size.x, 6.f);
		nvgFillColor(args.vg, nvgRGB(0x2a, 0x2a, 0x2e));
		nvgFill(args.vg);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0.f, T_TITLE);
		nvgLineTo(args.vg, box.size.x, T_TITLE);
		nvgStrokeColor(args.vg, INK);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		// A CROSS AT EACH END. Where the close control belongs is a habit, not a fact — the Mac
		// puts it at the left and Windows at the right — and two of them cost a few pixels of a
		// strip that is otherwise empty.
		drawCross(args.vg, closeLeft());
		drawCross(args.vg, closeRight());

		std::shared_ptr<window::Font> font = uiFont();
		if (font && font->handle >= 0) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, T_TITLE * 0.72f);
			nvgFillColor(args.vg, INK);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			char buf[64];
			std::snprintf(buf, sizeof(buf), "Demo — step %d of %d",
				runner.at() + 1, (int) runner.steps.size());
			nvgText(args.vg, box.size.x / 2.f, T_TITLE / 2.f, buf, NULL);
		}

		for (int i = 0; i < B_COUNT; i++)
			drawChip(args.vg, buttonRect(i), buttonLabel(i), i == B_RUN && runner.isRunning());

		OpaqueWidget::draw(args);
	}

	void onRemove(const RemoveEvent& e) override;
};


static Transport* gTransport = NULL;

void Transport::onRemove(const RemoveEvent& e) {
	runner.stop();
	if (gCard)
		gCard->hide();
	if (gTransport == this)
		gTransport = NULL;
	OpaqueWidget::onRemove(e);
}


void transportShow() {
	if (gTransport) {
		APP->scene->removeChild(gTransport);
		APP->scene->addChild(gTransport);
		raiseTheatre();
		return;
	}
	gTransport = new Transport;
	if (gWhere.x < 0.f) {
		gWhere = math::Vec(std::fmax(20.f, (APP->scene->box.size.x - T_W) / 2.f),
			std::fmax(20.f, APP->scene->box.size.y - T_H - 40.f));
	}
	gTransport->box.pos = gWhere;
	APP->scene->addChild(gTransport);
	raiseTheatre();
}


} // namespace demo
