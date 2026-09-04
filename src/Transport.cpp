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
#include "Script.hpp"
#include "Speech.hpp"
#include "Picker.hpp"

#include <osdialog.h>

#include <GLFW/glfw3.h>

namespace demo {


static const float T_TITLE = 22.f;
static const float T_W = 984.f;
static const float T_H = 76.f;
static const float BTN_H = 30.f;
static const float BTN_Y = T_TITLE + 12.f;
static const float BTN_GAP = 8.f;
static const float BTN_PAD = 12.f;

enum Button { B_SCRIPT, B_RELOAD, B_RUN, B_RESTART, B_BACK, B_STEP, B_RATE, B_VOICE,
	B_BADGES, B_CAPTIONS, B_COUNT };
static const float BTN_W[B_COUNT] =
	{224.f, 74.f, 74.f, 86.f, 62.f, 62.f, 92.f, 66.f, 78.f, 92.f};

static const float RATES[4] = {0.75f, 1.0f, 1.5f, 2.0f};


/** Where the window was last put. A transport that opens in the middle of the rack every time
would be moved out of the way every time. */
static math::Vec gWhere = math::Vec(-1.f, -1.f);


/** True from the moment somebody asks for the window to close until it actually goes.

A remove event says nothing about WHY it fired: it fires when you press the cross, and again when
Rack exits and destroys the whole scene. Everything worth doing on a close — stopping the run,
handing the patch back, putting the cursor back — reaches into the application, and doing any of
that during Rack's own teardown reads objects that have already been destroyed.

SET AND LEFT SET, because requestDelete does not remove anything: Rack takes the widget away
later, in its own step, and a flag raised and lowered around the request is already down by then.
That is how a window closed with Escape came to be treated as an application exiting, which left
the run going, the patch unrestored and the cursor hidden with nothing left to unhide it. */
static bool gClosing = false;


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


// ------------------------------------------------------- the script, until phase three

/** SCAFFOLDING, and the last piece of phase one still standing. Phase three reads a script off a
markdown file; until then this one is built from whatever is on the rack when Run is pressed, so
it exercises resolution and every gesture against real modules rather than against a fixture.

It is a self-test as much as a demonstration: if a name cannot be found, a control is off the
screen, a value does not arrive or a cable is not made, the run stops and says so. */
static std::vector<Step> selfTestScript(Stage& stage) {
	stage.clear();
	std::vector<Step> s;

	// The first module with a parameter, and a pair with an output and an input between them.
	// Our own module is skipped: a demo of the demo transport pointing at the demo transport
	// proves nothing.
	app::ModuleWidget* withParam = NULL;
	app::ModuleWidget* withOut = NULL;
	app::ModuleWidget* withIn = NULL;
	for (app::ModuleWidget* mw : APP->scene->rack->getModules()) {
		if (!mw->module || !mw->module->model)
			continue;
		if (mw->module->model->plugin == pluginInstance)
			continue;
		if (!withParam && !mw->getParams().empty())
			withParam = mw;
		if (!withOut && !mw->getOutputs().empty())
			withOut = mw;
		if (!withIn && mw != withOut && !mw->getInputs().empty())
			withIn = mw;
	}

	if (!withParam && !withOut) {
		Step none;
		none.note = "There is nothing on the rack to demonstrate. Add a module or two with "
			"knobs and jacks, then press Run again.";
		none.wait = 3.f;
		s.push_back(none);
		return s;
	}

	Step opening;
	opening.note = "Everything from here is done through Rack's own event system, so the "
		"pointer is really pressing these controls. Each step checks afterwards that it "
		"worked.";
	opening.wait = 0.6f;
	s.push_back(opening);

	if (withParam) {
		stage.bindId("a", withParam->module->id);

		Step point;
		point.kind = Step::POINT;
		point.target = "a:#0";
		point.note = "Controls are addressed by name, and turned into a place on the screen "
			"at the last moment — so a script survives any window size, zoom or scroll.";
		s.push_back(point);

		Step set;
		set.kind = Step::SET;
		set.target = "a:#0";
		set.value = 0.85f;
		set.note = "A value travels rather than jumping. The pointer shows a drag; the value "
			"itself is written, because how far a knob turns for a given movement is the "
			"knob's own business.";
		set.wait = 0.5f;
		s.push_back(set);

		Step back;
		back.kind = Step::SET;
		back.target = "a:#0";
		back.value = 0.15f;
		s.push_back(back);
	}

	if (withOut && withIn) {
		stage.bindId("out", withOut->module->id);
		stage.bindId("in", withIn->module->id);

		Step patch;
		patch.kind = Step::PATCH;
		patch.target = "out:out:#0";
		patch.target2 = "in:in:#0";
		patch.note = "A cable is a held drag, not a click at each end, because that is how one "
			"is really made. The step then asks the engine whether the cable exists.";
		patch.wait = 0.8f;
		s.push_back(patch);

		Step unpatch;
		unpatch.kind = Step::UNPATCH;
		unpatch.target = "in:in:#0";
		unpatch.note = "And off again: pulled from the jack and dropped on bare rack, which is "
			"what deletes one.";
		unpatch.wait = 1.2f;
		s.push_back(unpatch);
	}

	return s;
}


// ---------------------------------------------------------------- the window

struct Transport : widget::OpaqueWidget, OurWidget {
	Runner runner;
	/** Stop is two-stage, and this is which stage the next press is. */
	bool stopped = false;
	math::Vec dragStart;

	Transport() {
		box.size = math::Vec(T_W, T_H);
		// OPENS ON WHATEVER WAS TOUCHED LAST — the script played most recently, or the one whose
		// file was edited most recently, whichever of those happened later. There is no state in
		// which the transport comes up holding nothing while scripts exist.
		const std::string want = pickerRemembered();
		if (!want.empty())
			loadScript(want);
		if (scriptPath.empty())
			rebuild();
	}

	/** The script that is loaded, if one is. Empty means the self test, which is built from the
	rack as it stands so that pressing Run with no script still demonstrates something. */
	std::string scriptPath;

	void rebuild() {
		if (scriptPath.empty()) {
			runner.bindings.clear();
			runner.patchPath.clear();
			runner.title = "Self test";
			runner.pacing = Pacing();
			runner.load(selfTestScript(runner.stage));
			return;
		}
		loadScript(scriptPath);
	}

	/** Read a script off disk and set the runner up from its header. Re-reading the same path is
	Reload, which is how an author corrects a sentence and hears it again.

	A SCRIPT THAT WILL NOT PARSE IS NOT LOADED AT ALL. Its error goes where a failed step's goes,
	across the title strip, because a script with a mistake in it should say so before a take
	starts rather than halfway through one. */
	void loadScript(const std::string& path) {
		const Script sc = scriptLoad(path);
		if (!sc.error.empty()) {
			runner.failure = sc.error;
			return;
		}
		runner.stop();
		runner.releaseSession();
		scriptPath = path;
		runner.failure.clear();
		runner.pacing = sc.pacing;
		runner.bindings = sc.bindings;
		runner.patchPath = sc.patchPath;
		runner.scriptPath = sc.path;
		runner.title = sc.title.empty() ? system::getFilename(path) : sc.title;
		runner.load(sc.steps);
		runner.voice = sc.voice;
		runner.voiceRate = sc.rate;
		runner.master = sc.master;
		runner.duck = sc.duck;
		theatre()->badges = sc.badges;
		card()->enabled = sc.captions;
		stopped = false;
		pickerRemember(path);
		renderVoice();
	}

	/** RENDERED WHEN A SCRIPT IS LOADED, not when it is run. The first time a sentence is
	written it has to be spoken by `say` and measured, which takes a moment; every time after it
	is already on disk. Doing it here means Run is always immediate, and means a reworded note
	cannot reach a take still speaking the old words. */
	void renderVoice() {
		if (!runner.speak || runner.steps.empty())
			return;
		const int made = runner.render();
		if (made > 0)
			INFO("DreamerDemo: rendered %d lines in %s", made, runner.voice.c_str());
	}

	/** THE LIST OF SCRIPTS, dropped out of the field that names the current one. Every script
	is in it, so there is nothing left for a menu of commands to do: opening a file by hand was
	only ever a way round a list that did not show them all. */
	void chooseScript() {
		const std::vector<std::string> found = scriptList();
		if (found.empty()) {
			runner.failure = "no scripts in " + scriptDir();
			return;
		}
		const math::Rect field = buttonRect(B_SCRIPT);
		Transport* self = this;
		pickerOpen(math::Rect(box.pos.plus(field.pos), field.size), found, scriptPath,
			[self](std::string path) { self->loadScript(path); });
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
			// THE FIELD SHOWS WHAT IS LOADED. A picker whose face says only "Script" makes you
			// open it to find out which one you are about to run.
			case B_SCRIPT: return runner.title.empty() ? "Self test" : runner.title;
			case B_RUN: return runner.isRunning() ? "Stop" : (stopped ? "Reset" : "Run");
			case B_RESTART: return "Restart";
			case B_BACK: return "Back";
			case B_STEP: return "Step";
			// Lit when on, so the label is the name of the thing rather than its state.
			case B_RELOAD: return "Reload";
			case B_VOICE: return "Voice";
			case B_BADGES: return "Badges";
			case B_CAPTIONS: return "Captions";
			default: {
				char buf[24];
				std::snprintf(buf, sizeof(buf), "Rate %.2gx", runner.rate);
				return buf;
			}
		}
	}

	void press(int i) {
		// ANY PRESS BUT RUN TAKES THE MESSAGE DOWN. A failure has to stay up long enough to be
		// read, and then it has to be dismissable by doing something — otherwise the only way
		// out of it is to close the window.
		if (i != B_RUN) {
			runner.failure.clear();
			if (!runner.isRunning())
				card()->hide();
		}
		switch (i) {
			case B_SCRIPT:
				chooseScript();
				break;
			case B_RELOAD:
				if (!scriptPath.empty())
					loadScript(scriptPath);
				break;
			case B_RUN:
				if (runner.isRunning()) {
					// First press: stand still, with the rack as the demo left it.
					runner.stop();
					stopped = true;
				}
				else if (stopped) {
					// Second press: put everything back, the user's own patch included.
					card()->hide();
					theatre()->clear();
					runner.releaseSession();
					runner.failure.clear();
					stopped = false;
				}
				else {
					if (scriptPath.empty())
						rebuild();
					runner.run();
				}
				break;
			case B_RESTART:
				stopped = false;
				if (scriptPath.empty())
					rebuild();
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
			case B_VOICE:
				runner.speak = !runner.speak;
				if (!runner.speak)
					speechSilence();
				else
					renderVoice();
				break;
			case B_BADGES:
				theatre()->badges = !theatre()->badges;
				break;
			case B_CAPTIONS:
				card()->enabled = !card()->enabled;
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
				gClosing = true;
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

	/** Whether the runner was running on the previous frame, so the moment it stops can be
	noticed. */
	bool wasRunning = false;

	void step() override {
		runner.tick();

		// A SCRIPT THAT REACHES ITS LAST STEP HAS STOPPED, not finished with. The rack it built
		// stays exactly as it is — a take that cut back to the viewer's own patch on the last
		// frame would be unusable, and an author wants to look at what the demo made — so the
		// end of a script leaves the window standing on it, with the button reading Reset. One
		// press then hands the patch back. A failed step lands in the same place.
		if (wasRunning && !runner.isRunning())
			stopped = true;
		wasRunning = runner.isRunning();

		// THE POINTER IS UP FOR AS LONG AS THE DEMO OWNS THE SCREEN, which includes standing
		// stopped on a step. It goes away, and the real cursor comes back, only when the demo
		// has been put back.
		theatre()->running = runner.isRunning() || stopped;
		// LIVE ONLY WHILE IT IS ACTUALLY PERFORMING. Standing stopped on a step still shows the
		// synthetic pointer, but the real cursor has to come back or there is nothing to press.
		theatre()->live = runner.isRunning();

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
					gClosing = true;
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
			gClosing = true;
			requestDelete();
			return;
		}
		OpaqueWidget::onHoverKey(e);
	}

	/** A name that will not fit is cut and finished with an ellipsis, rather than running out
	of its field. */
	std::string fit(NVGcontext* vg, std::string text, float width) {
		float bounds[4] = {0.f, 0.f, 0.f, 0.f};
		nvgTextBounds(vg, 0.f, 0.f, text.c_str(), NULL, bounds);
		if (bounds[2] - bounds[0] <= width)
			return text;
		while (text.size() > 1) {
			text.resize(text.size() - 1);
			const std::string tryIt = text + "\u2026";
			nvgTextBounds(vg, 0.f, 0.f, tryIt.c_str(), NULL, bounds);
			if (bounds[2] - bounds[0] <= width)
				return tryIt;
		}
		return text;
	}

	/** A chevron, so a field that opens a list looks like one. */
	void drawChevron(NVGcontext* vg, float x, float y, float r) {
		nvgBeginPath(vg);
		nvgMoveTo(vg, x - r, y - r * 0.45f);
		nvgLineTo(vg, x, y + r * 0.55f);
		nvgLineTo(vg, x + r, y - r * 0.45f);
		nvgStrokeColor(vg, INK);
		nvgStrokeWidth(vg, std::fmax(1.2f, r * 0.34f));
		nvgLineCap(vg, NVG_ROUND);
		nvgLineJoin(vg, NVG_ROUND);
		nvgStroke(vg);
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

	/** The script picker: a field carrying the name of what is loaded, with a chevron. */
	void drawField(NVGcontext* vg, math::Rect r, const std::string& label) {
		nvgBeginPath(vg);
		nvgRoundedRect(vg, r.pos.x, r.pos.y, r.size.x, r.size.y, 4.f);
		nvgFillColor(vg, nvgRGB(0x1a, 0x1a, 0x1e));
		nvgFill(vg);
		nvgStrokeColor(vg, nvgRGBA(0xcf, 0xcf, 0xcf, 0x90));
		nvgStrokeWidth(vg, 1.f);
		nvgStroke(vg);

		std::shared_ptr<window::Font> font = uiFont();
		if (!font || font->handle < 0)
			return;
		nvgFontFaceId(vg, font->handle);
		nvgFontSize(vg, 14.f);
		nvgFillColor(vg, INK);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, r.pos.x + 10.f, r.pos.y + r.size.y / 2.f + 0.5f,
			fit(vg, label, r.size.x - 32.f).c_str(), NULL);
		drawChevron(vg, r.pos.x + r.size.x - 12.f, r.pos.y + r.size.y / 2.f - 1.f, 4.5f);
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
			// A FAILED STEP IS NAMED IN THE TITLE, not only in the card. The card is the
			// viewer's; this strip is the author's, and it is where you look when a take
			// stopped early.
			char buf[240];
			if (!runner.failure.empty()) {
				std::snprintf(buf, sizeof(buf), "%s", runner.failure.c_str());
				nvgFillColor(args.vg, ACCENT);
			}
			else {
				std::snprintf(buf, sizeof(buf), "%s — step %d of %d",
					runner.title.c_str(), runner.at() + 1, (int) runner.steps.size());
			}
			nvgText(args.vg, box.size.x / 2.f, T_TITLE / 2.f, buf, NULL);
		}

		for (int i = 0; i < B_COUNT; i++) {
			if (i == B_SCRIPT) {
				drawField(args.vg, buttonRect(i), buttonLabel(i));
				continue;
			}
			const bool lit = (i == B_RUN && runner.isRunning())
				|| (i == B_VOICE && runner.speak)
				|| (i == B_BADGES && theatre()->badges)
				|| (i == B_CAPTIONS && card()->enabled);
			drawChip(args.vg, buttonRect(i), buttonLabel(i), lit);
		}

		OpaqueWidget::draw(args);
	}

	void onRemove(const RemoveEvent& e) override;
};


static Transport* gTransport = NULL;

/** True while the window is only being reordered. REORDERING IS A REMOVE AND AN ADD — that is how
Rack raises a child — and removeChild dispatches the remove event before it unhooks anything. So
onRemove fires on a window that is not going anywhere, and without this it would stop the run,
hand the user's patch back and null the pointer that the very next line uses. */
static bool gRaising = false;

void Transport::onRemove(const RemoveEvent& e) {
	if (!gRaising) {
		// NOTHING IN THE APPLICATION IS TOUCHED UNLESS WE KNOW IT IS STILL THERE.
		//
		// This handler runs for two entirely different reasons and cannot tell them apart from
		// the event: somebody closed the window, or Rack is exiting and destroying the scene. In
		// the second case the rack, the engine and every module have already gone, and anything
		// that reaches for one of them is reading freed memory. Stopping a run asks the rack for
		// half-made cables, hands the level back through a module's parameter and injects a
		// mouse release — all of which are fine on a close and fatal on an exit.
		//
		// A deliberate close is the only case in which the application is known to be alive, so
		// it is the only case that does any of it.
		if (gClosing) {
			runner.stop();
			runner.releaseSession();
			if (gCard)
				gCard->hide();
		}
		else {
			// The one thing worth doing on the way out, because it is a separate process and
			// would otherwise carry on talking after Rack has gone.
			speechSilence();
		}
		if (gTransport == this)
			gTransport = NULL;
		gClosing = false;
	}
	OpaqueWidget::onRemove(e);
}


widget::Widget* frontWindow() {
	// Frontmost first: drawing order is the order of the list, so the last child is on top.
	for (size_t k = APP->scene->children.size(); k > 0; k--) {
		std::list<widget::Widget*>::reverse_iterator it = APP->scene->children.rbegin();
		std::advance(it, APP->scene->children.size() - k);
		widget::Widget* w = *it;
		if (!w || !w->visible || w->requestedDelete)
			continue;
		if (dynamic_cast<OurWidget*>(w))
			continue;                              // the transport, the card, the pointer, a list
		if (dynamic_cast<ui::MenuOverlay*>(w))
			continue;                              // a menu is not a window
		if (w == (widget::Widget*) APP->scene->rackScroll || w == APP->scene->menuBar
			|| w == APP->scene->browser)
			continue;                              // the application's own furniture
		// A window rather than a stray layer: big enough to have a frame and a title.
		if (w->box.size.x < 120.f || w->box.size.y < 80.f)
			continue;
		return w;
	}
	return NULL;
}


math::Rect transportRect() {
	if (!gTransport)
		return math::Rect();
	return math::Rect(gTransport->box.pos, gTransport->box.size);
}


/** Six places the transport can stand, in preference order. The top of the window first, because
the card prefers the bottom and the two must not be sent to the same corner. */
void transportStepAside(math::Rect region) {
	if (!gTransport || region.size.x <= 0.f || region.size.y <= 0.f)
		return;
	const math::Rect mine(gTransport->box.pos, gTransport->box.size);
	if (!mine.intersects(region))
		return;

	const math::Vec scene = APP->scene->box.size;
	const float m = 24.f;
	const float cx = (scene.x - T_W) / 2.f;
	const math::Vec spots[6] = {
		math::Vec(cx, m),
		math::Vec(m, m),
		math::Vec(scene.x - T_W - m, m),
		math::Vec(m, scene.y - T_H - m),
		math::Vec(scene.x - T_W - m, scene.y - T_H - m),
		math::Vec(cx, scene.y - T_H - m),
	};
	for (int i = 0; i < 6; i++) {
		const math::Rect there(spots[i], gTransport->box.size);
		if (!there.intersects(region)) {
			gTransport->box.pos = spots[i];
			gWhere = spots[i];
			return;
		}
	}
	// Nowhere is clear. Leave it where the author put it rather than shuffling it about.
}


void transportShow() {
	if (gTransport) {
		Transport* window = gTransport;
		gRaising = true;
		APP->scene->removeChild(window);
		APP->scene->addChild(window);
		gRaising = false;
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
