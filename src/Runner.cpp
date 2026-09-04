#include "Runner.hpp"
#include "Theatre.hpp"
#include "Card.hpp"
#include "Gesture.hpp"
#include "Speech.hpp"

#include <patch.hpp>

#include <GLFW/glfw3.h>

namespace demo {


/** How long the pointer is shown pressed. Short, because it is punctuation rather than an event:
the ripple is what says a click happened. */
static const float PRESS = 0.18f;

/** How many notches a scroll gesture is made of. One jump is not a wheel being turned; a run of
small pulses is seen to turn. */
static const int WHEEL_PULSES = 8;

/** How close a parameter has to land to count as having been set. Wider than nothing, because a
snapped or quantised parameter cannot land anywhere it likes. */
static const float SET_TOLERANCE = 0.03f;


// ---------------------------------------------------------------- snapshots

/** WHERE A STEP'S STATE IS KEPT. A snapshot is a patch file, because a patch file is exactly the
state a step can change, and Rack already knows how to write and read one. It also means module
ids survive a restore, so the names a script bound still point at the same modules. */
static std::string snapDir() {
	const std::string dir = asset::user("DreamerDemo/snapshots");
	if (!system::isDirectory(dir))
		system::createDirectories(dir);
	return dir;
}

static std::string snapPath(int i) {
	return snapDir() + "/step-" + std::to_string(i) + ".vcv";
}

static std::string sessionPath() {
	return snapDir() + "/session.vcv";
}

/** The path the user's own patch had, so restoring the session does not leave Rack thinking the
demo's file is the one they were working on. */
static std::string gSessionOwnPath;


void Runner::snapshot(int i) {
	try {
		APP->patch->save(snapPath(i));
	}
	catch (Exception& e) {
		// A snapshot that cannot be written costs stepping back, not the run.
		WARN("DreamerDemo: could not snapshot step %d: %s", i, e.what());
	}
}


void Runner::restoreSnapshot(int i) {
	const std::string path = snapPath(i);
	if (!system::isFile(path))
		return;
	try {
		APP->patch->load(path);
		APP->patch->path = gSessionOwnPath;
		std::string why;
		applyBindings(&why);
	}
	catch (Exception& e) {
		WARN("DreamerDemo: could not restore step %d: %s", i, e.what());
	}
}


void Runner::armSession() {
	if (armed)
		return;
	gSessionOwnPath = APP->patch->path;
	try {
		APP->patch->save(sessionPath());
		armed = true;
	}
	catch (Exception& e) {
		WARN("DreamerDemo: could not save the session: %s", e.what());
	}
}


void Runner::releaseSession() {
	if (!armed)
		return;
	armed = false;
	if (!system::isFile(sessionPath()))
		return;
	try {
		APP->patch->load(sessionPath());
		APP->patch->path = gSessionOwnPath;
	}
	catch (Exception& e) {
		WARN("DreamerDemo: could not restore the session: %s", e.what());
	}
}


// ---------------------------------------------------------------- menus

/** A menu item by its text, wherever it is in whatever menu is open. Exact first, then any item
containing it, so a script can say "Polyphony" without writing out "Polyphony channels: 4". */
static bool menuItemRect(widget::Widget* w, const std::string& want, math::Rect* out) {
	if (ui::MenuItem* item = dynamic_cast<ui::MenuItem*>(w)) {
		if (item->text == want) {
			*out = sceneRect(item);
			return true;
		}
	}
	for (widget::Widget* child : w->children) {
		if (menuItemRect(child, want, out))
			return true;
	}
	return false;
}

static bool menuItemLoose(widget::Widget* w, const std::string& want, math::Rect* out) {
	if (ui::MenuItem* item = dynamic_cast<ui::MenuItem*>(w)) {
		if (item->text.find(want) != std::string::npos) {
			*out = sceneRect(item);
			return true;
		}
	}
	for (widget::Widget* child : w->children) {
		if (menuItemLoose(child, want, out))
			return true;
	}
	return false;
}

static bool findMenuItem(const std::string& want, math::Rect* out) {
	for (widget::Widget* child : APP->scene->children) {
		ui::MenuOverlay* overlay = dynamic_cast<ui::MenuOverlay*>(child);
		if (!overlay || !overlay->visible || overlay->requestedDelete)
			continue;
		if (menuItemRect(overlay, want, out))
			return true;
		if (menuItemLoose(overlay, want, out))
			return true;
	}
	return false;
}


// ---------------------------------------------------------------- the machine

void Runner::load(const std::vector<Step>& s) {
	stop();
	steps = s;
	index = 0;
	failure.clear();
}


bool Runner::applyBindings(std::string* why) {
	// NOTHING DECLARED, NOTHING TO CLEAR. A script that names its modules in its header is bound
	// afresh every run, so a name left over from the last one cannot point at a module that has
	// since gone. But a caller that bound names itself — the self test does, straight from the
	// rack — has already put them there, and clearing would throw them away a moment before the
	// first step asked for one.
	if (bindings.empty())
		return true;
	stage.clear();
	for (size_t i = 0; i < bindings.size(); i++) {
		if (!stage.bindModel(bindings[i].first, bindings[i].second)) {
			if (why)
				*why = "there is no " + bindings[i].second + " on the rack for \""
					+ bindings[i].first + "\"";
			return false;
		}
	}
	return true;
}


int Runner::render() {
	std::vector<std::string> lines;
	for (size_t i = 0; i < steps.size(); i++) {
		if (!steps[i].note.empty())
			lines.push_back(steps[i].note);
	}
	return speechRender(lines, voice, voiceRate);
}


/** DUCKED WHILE THE VOICE IS SPEAKING, and put back after. The take is one audio track — screen
and computer audio together — so there is no balance to fix afterwards and it has to be right
while it plays. The parameter named in the header is moved like any other, so nothing here needs
a mechanism the runner does not already have. */
/** The parameter named as the master, or nothing. */
static engine::ParamQuantity* masterOf(const Stage& stage, const std::string& master) {
	if (master.empty())
		return NULL;
	const Target t = stage.find(master);
	if (!t.ok || !t.module || t.paramId < 0)
		return NULL;
	return t.module->paramQuantities[t.paramId];
}


void Runner::duckCapture() {
	haveRest = false;
	ducked = false;
	engine::ParamQuantity* q = masterOf(stage, master);
	if (!q)
		return;
	masterRest = q->getValue();
	// NOTHING TO DUCK. A master already at the bottom would be pulled down and left there, and
	// the demo would look like the thing that silenced the patch.
	if (masterRest <= q->getMinValue() + 0.0001f)
		return;
	haveRest = true;
}


void Runner::duckDown() {
	if (ducked || !haveRest)
		return;
	engine::ParamQuantity* q = masterOf(stage, master);
	if (!q)
		return;

	// DECIBELS, WHERE THE PARAMETER SPEAKS THEM. A fader that displays decibels is asked for its
	// current reading less the duck, which is exactly right whatever curve it uses underneath.
	// Anything else is treated as a linear gain and scaled, which is what a level control is
	// even when it does not say so.
	if (q->unit == " dB" || q->unit == "dB") {
		q->setDisplayValue(q->getDisplayValue() - duck);
	}
	else {
		q->setValue(masterRest * std::pow(10.f, -duck / 20.f));
	}
	ducked = true;
}


void Runner::duckUp() {
	if (!haveRest)
		return;
	ducked = false;
	// PUT BACK EXACTLY, and unconditionally rather than only when this runner believes it is
	// down. A level left low is the one failure a viewer cannot diagnose: the patch simply makes
	// no sound, and nothing on screen says why.
	engine::ParamQuantity* q = masterOf(stage, master);
	if (q)
		q->setValue(masterRest);
}


void Runner::enter(Phase p, float seconds) {
	phase = p;
	until = system::getTime() + std::fmax(0.f, seconds) / std::fmax(0.1f, rate);
}


void Runner::fail(const std::string& why) {
	failure = why;
	card()->show("The demo stopped. " + why, math::Rect());
	stop();
}


/** THE REGION THE COMING STEPS WILL TOUCH, so the card can take a berth clear of it, and so the
transport can step aside. It runs to the next note rather than to the next step, because the card
is up for exactly that long. */
static math::Rect regionFor(const Stage& stage, const std::vector<Step>& steps, int from) {
	math::Rect r;
	bool any = false;
	for (size_t i = (size_t) from; i < steps.size(); i++) {
		if (i > (size_t) from && !steps[i].note.empty())
			break;
		for (int which = 0; which < 2; which++) {
			const std::string& ref = which ? steps[i].target2 : steps[i].target;
			if (ref.empty())
				continue;
			const Target t = stage.find(ref);
			if (!t.ok)
				continue;
			// Generous: a pointer at a control occupies a hand's worth of screen, not a point.
			const math::Rect one = t.rect.grow(math::Vec(70.f, 60.f));
			r = any ? r.expand(one) : one;
			any = true;
		}
	}
	return any ? r : math::Rect();
}


/** THE VIEW, IN MODULE COORDINATES.

Rack keeps the view as a scroll offset in pixels and a zoom, and offers a grid offset measured
from an origin constant. Both are awkward to move smoothly: a pixel offset means something
different at every zoom, and the grid one has that constant in it. What a camera actually has is
a place it is looking at and how close it is, and those two are independent — so that is what is
interpolated, and the offset is worked out from them on every frame.

The arithmetic is Rack's own, from zoomToBound: the offset that puts a module-space point in the
middle of the viewport is that point times the zoom, less half the viewport. */
static math::Vec viewCentre() {
	app::RackScrollWidget* scroll = APP->scene->rackScroll;
	if (!scroll)
		return math::Vec();
	const float zoom = std::fmax(0.0001f, scroll->getZoom());
	return scroll->offset.plus(scroll->box.size.div(2.f)).div(zoom);
}


/** The zoom at which a bound fills the viewport, with the same margin Rack leaves. */
static float zoomForBound(math::Rect bound) {
	app::RackScrollWidget* scroll = APP->scene->rackScroll;
	if (!scroll)
		return 1.f;
	const math::Vec size = scroll->box.size;
	bound = bound.grow(math::Vec(24.f, 24.f));
	if (bound.size.x <= 1.f || bound.size.y <= 1.f)
		return scroll->getZoom();
	return std::fmin(size.x / bound.size.x, size.y / bound.size.y);
}


void Runner::camApply(math::Vec centre, float zoom) {
	app::RackScrollWidget* scroll = APP->scene->rackScroll;
	if (!scroll)
		return;
	scroll->zoomWidget->setZoom(zoom);
	scroll->offset = centre.mult(zoom).minus(scroll->box.size.div(2.f));
}


void Runner::camTo(math::Rect bound, float seconds) {
	app::RackScrollWidget* scroll = APP->scene->rackScroll;
	if (!scroll)
		return;
	camZoomFrom = scroll->getZoom();
	camCentreFrom = viewCentre();
	camZoomTo = math::clamp(zoomForBound(bound), 0.1f, 4.f);
	camCentreTo = bound.getCenter();
	camStart = system::getTime();
	camEnd = camStart + std::fmax(0.05f, seconds) / std::fmax(0.1f, rate);
	camMoving = true;
}


void Runner::camCentre(math::Vec centre, float seconds) {
	app::RackScrollWidget* scroll = APP->scene->rackScroll;
	if (!scroll)
		return;
	camZoomFrom = camZoomTo = scroll->getZoom();
	camCentreFrom = viewCentre();
	camCentreTo = centre;
	camStart = system::getTime();
	camEnd = camStart + std::fmax(0.05f, seconds) / std::fmax(0.1f, rate);
	camMoving = true;
}


void Runner::camTick() {
	if (!camMoving)
		return;
	if (!APP->scene->rackScroll) {
		camMoving = false;
		return;
	}
	const double now = system::getTime();
	float t = (float) ((now - camStart) / std::fmax(0.001, camEnd - camStart));
	if (t >= 1.f) {
		t = 1.f;
		camMoving = false;
	}
	// Eased at both ends, so the move starts and stops like a camera rather than a jump cut.
	t = t * t * (3.f - 2.f * t);

	// GEOMETRIC IN ZOOM. Halfway between one and four is two, not two and a half.
	const float zoom = camZoomFrom
		* std::pow(camZoomTo / std::fmax(0.0001f, camZoomFrom), t);
	camApply(camCentreFrom.plus(camCentreTo.minus(camCentreFrom).mult(t)), zoom);

	// The pointer arrives as the view settles. Its destination is asked for again on every
	// frame, because the module is travelling across the screen while the camera closes on it —
	// a destination worked out once, before the move, would be where the module used to be.
	if (!camPointTarget.empty()) {
		const Target where = stage.find(camPointTarget);
		if (where.ok) {
			const math::Vec now2 = where.centre();
			theatre()->placeAt(camPointFrom.plus(now2.minus(camPointFrom).mult(t)));
		}
		if (!camMoving)
			camPointTarget.clear();
	}
}


void Runner::instant(const Step& s) {
	// Steps with no pointer in them. They happen at once, and the note beside them is what tells
	// the viewer that something has changed.
	switch (s.kind) {
		case Step::ZOOM: {
			if (s.target.empty()) {
				// THE WHOLE RACK, as the union of what is on it. Asked of the modules rather
				// than of Rack's own framing call, so that it eases there rather than cutting.
				math::Rect all;
				bool any = false;
				for (app::ModuleWidget* mw : APP->scene->rack->getModules()) {
					all = any ? all.expand(mw->box) : mw->box;
					any = true;
				}
				if (!any)
					break;
				camPointTarget.clear();
				camTo(all, pacing.perform);
				break;
			}
			const Target t = stage.find(s.target);
			if (!t.ok || !t.mw) {
				fail("Step " + std::to_string(index + 1) + ": " + t.why + ".");
				return;
			}
			// The module's own box is already in rack coordinates, which is what the camera
			// takes. A factor above one frames it with less around it.
			const float f = std::fmax(0.2f, s.value);
			const math::Vec pad = t.mw->box.size.mult((1.f / f) * 0.5f);
			camPointFrom = theatre()->at();
			camTo(t.mw->box.grow(pad), pacing.perform);
			camPointTarget = s.target;
			break;
		}

		case Step::PAN: {
			// PANNING MOVES WHERE THE VIEW IS LOOKING and leaves how close it is alone.
			math::Vec want;
			if (!s.target.empty()) {
				const Target t = stage.find(s.target);
				if (!t.ok || !t.mw) {
					fail("Step " + std::to_string(index + 1) + ": " + t.why + ".");
					return;
				}
				want = t.mw->box.getCenter();
			}
			else {
				want = viewCentre().plus(math::Vec(s.value * RACK_GRID_WIDTH,
					s.value2 * RACK_GRID_HEIGHT));
			}
			camPointTarget.clear();
			camCentre(want, pacing.perform);
			break;
		}

		case Step::OPEN: {
			if (!system::isFile(s.arg)) {
				fail("Step " + std::to_string(index + 1) + ": there is no patch at " + s.arg);
				return;
			}
			try {
				APP->patch->load(s.arg);
				APP->patch->path = gSessionOwnPath;
			}
			catch (Exception& e) {
				fail("Step " + std::to_string(index + 1) + ": " + e.what());
				return;
			}
			std::string why;
			if (!applyBindings(&why)) {
				fail("Step " + std::to_string(index + 1) + ": after loading that patch, " + why);
				return;
			}
			break;
		}

		case Step::ADD: {
			const size_t slash = s.arg.find('/');
			if (slash == std::string::npos) {
				fail("Step " + std::to_string(index + 1) + ": add needs \"Plugin/Model\".");
				return;
			}
			plugin::Model* model = plugin::getModel(s.arg.substr(0, slash),
				s.arg.substr(slash + 1));
			if (!model) {
				fail("Step " + std::to_string(index + 1) + ": there is no module " + s.arg + ".");
				return;
			}
			engine::Module* module = model->createModule();
			APP->engine->addModule(module);
			app::ModuleWidget* mw = model->createModuleWidget(module);
			APP->scene->rack->addModule(mw);

			// BESIDE WHAT IS ALREADY THERE, on the top row, which is where a rack grows. Rack
			// squeezes it into the nearest free space from that point.
			float right = 0.f, top = 0.f;
			bool anyModule = false;
			for (app::ModuleWidget* other : APP->scene->rack->getModules()) {
				if (other == mw)
					continue;
				if (!anyModule || other->box.pos.y < top)
					top = other->box.pos.y;
				right = std::fmax(right, other->box.pos.x + other->box.size.x);
				anyModule = true;
			}
			APP->scene->rack->setModulePosNearest(mw, math::Vec(right, top));
			stage.bindId(s.target, module->id);
			break;
		}

		default:
			break;
	}
}


void Runner::expand(const Step& s) {
	gests.clear();
	gi = 0;
	checkA = Target();
	checkB = Target();
	checkCount = 0;

	switch (s.kind) {
		case Step::SAY:
		case Step::WAIT:
			return;
		case Step::ZOOM:
		case Step::PAN: {
			// A VIEW CHANGE IS NOT SOMETHING THE POINTER DOES, and it has to happen WHILE the
			// sentence about it is being read rather than after it. So it is started here, as
			// the note goes up, and eases along on its own clock while the step holds.
			instant(s);
			return;
		}
		case Step::OPEN:
		case Step::ADD: {
			Gest g;
			g.word = "";
			g.act = Gest::INSTANT;
			gests.push_back(g);
			return;
		}
		default:
			break;
	}

	Target a = stage.find(s.target);
	if (!a.ok) {
		fail("Step " + std::to_string(index + 1) + ": " + a.why + ".");
		return;
	}
	if (!onScreen(a.rect)) {
		fail("Step " + std::to_string(index + 1) + ": \"" + s.target
			+ "\" is not on the screen. Frame it first with a zoom step.");
		return;
	}
	checkA = a;

	Gest move;
	move.word = "move pointer";
	move.act = Gest::MOVE;
	move.pos = a.centre();
	move.target = a;
	gests.push_back(move);

	switch (s.kind) {
		case Step::POINT:
			break;

		case Step::CLICK:
		case Step::RIGHT_CLICK:
		case Step::MENU: {
			Gest g;
			const bool right = (s.kind != Step::CLICK);
			g.word = right ? "right click" : "left click";
			g.act = right ? Gest::CLICK_R : Gest::CLICK_L;
			g.pos = a.centre();
			g.glow = a.rect;
			g.target = a;
			gests.push_back(g);

			if (s.kind == Step::MENU) {
				// THE ITEM'S POSITION DOES NOT EXIST YET. The menu opens when the right click
				// lands, so where the row is can only be asked once that has happened; these two
				// resolve themselves when they start.
				Gest to;
				to.word = "move pointer";
				to.act = Gest::MENU_MOVE;
				to.arg = s.arg;
				gests.push_back(to);

				Gest pick;
				pick.word = "left click";
				pick.act = Gest::CLICK_HERE;
				gests.push_back(pick);
			}
			break;
		}

		case Step::SET: {
			if (a.paramId < 0) {
				fail("Step " + std::to_string(index + 1) + ": \"" + s.target
					+ "\" is not a parameter.");
				return;
			}
			Gest g;
			g.word = "drag";
			g.act = Gest::SET_VALUE;
			g.pos = a.centre();
			g.glow = a.rect;
			g.value = s.value;
			g.target = a;
			gests.push_back(g);
			break;
		}

		case Step::SCROLL: {
			Gest g;
			g.word = "scroll wheel";
			g.act = Gest::WHEEL;
			g.pos = a.centre();
			g.glow = a.rect;
			g.value = s.value;
			g.target = a;
			gests.push_back(g);
			break;
		}

		case Step::PATCH: {
			Target b = stage.find(s.target2);
			if (!b.ok) {
				fail("Step " + std::to_string(index + 1) + ": " + b.why + ".");
				return;
			}
			if (!onScreen(b.rect)) {
				fail("Step " + std::to_string(index + 1) + ": \"" + s.target2
					+ "\" is not on the screen. Frame it first with a zoom step.");
				return;
			}
			if (a.portId < 0 || b.portId < 0) {
				fail("Step " + std::to_string(index + 1) + ": a patch needs two ports.");
				return;
			}
			checkB = b;

			// A CABLE IS A HELD DRAG, not a click at each end. That is how one is really made
			// here, and the badge says so.
			Gest down;
			down.word = "button down";
			down.act = Gest::DOWN;
			down.pos = a.centre();
			down.glow = a.rect;
			down.target = a;
			gests.push_back(down);

			Gest drag;
			drag.word = "drag";
			drag.act = Gest::DRAG;
			drag.pos = b.centre();
			drag.target = b;
			gests.push_back(drag);

			Gest up;
			up.word = "button up";
			up.act = Gest::UP;
			up.pos = b.centre();
			up.glow = b.rect;
			up.target = b;
			gests.push_back(up);
			break;
		}

		case Step::UNPATCH: {
			if (a.portId < 0) {
				fail("Step " + std::to_string(index + 1) + ": \"" + s.target
					+ "\" is not a port.");
				return;
			}
			checkCount = gCableCount(a);

			Gest down;
			down.word = "button down";
			down.act = Gest::DOWN;
			down.pos = a.centre();
			down.glow = a.rect;
			down.target = a;
			gests.push_back(down);

			// Dropped on bare rack, which is what deletes a cable. Below the port rather than
			// beside it, because the module's own panel is what is beside it.
			math::Vec away = a.centre().plus(math::Vec(0.f, 150.f));
			away.y = std::fmin(away.y, APP->scene->box.size.y - 60.f);
			Gest drag;
			drag.word = "drag";
			drag.act = Gest::DRAG;
			drag.pos = away;
			gests.push_back(drag);

			Gest up;
			up.word = "button up";
			up.act = Gest::UP;
			up.pos = away;
			gests.push_back(up);
			break;
		}

		case Step::MOVE_MODULE: {
			if (!a.mw) {
				fail("Step " + std::to_string(index + 1) + ": \"" + s.target
					+ "\" is not a module.");
				return;
			}
			// Dragged by the top of the panel, which is the strip Rack itself treats as the
			// handle and the one part of a module guaranteed to carry no control.
			const math::Vec grip(a.rect.pos.x + a.rect.size.x / 2.f, a.rect.pos.y + 8.f);
			gests[0].pos = grip;

			const float zoom = a.mw->getAbsoluteZoom();
			const math::Vec to = grip.plus(math::Vec(s.value * RACK_GRID_WIDTH * zoom,
				s.value2 * RACK_GRID_HEIGHT * zoom));

			Gest down;
			down.word = "button down";
			down.act = Gest::DOWN;
			down.pos = grip;
			gests.push_back(down);

			Gest drag;
			drag.word = "drag";
			drag.act = Gest::DRAG;
			drag.pos = to;
			gests.push_back(drag);

			Gest up;
			up.word = "button up";
			up.act = Gest::UP;
			up.pos = to;
			gests.push_back(up);
			break;
		}

		default:
			break;
	}
}


bool Runner::verify(const Step& s, std::string* why) {
	switch (s.kind) {
		case Step::SET: {
			const float now = gParamUnit(checkA);
			if (std::fabs(now - s.value) <= SET_TOLERANCE)
				return true;
			*why = "the parameter did not reach the value it was set to";
			return false;
		}
		case Step::PATCH: {
			// Either way round: an author says "patch this to that" without minding which end is
			// the output, and Rack does not mind either.
			const bool ok =
				gCableExists(checkA.module, checkA.portId, checkB.module, checkB.portId)
				|| gCableExists(checkB.module, checkB.portId, checkA.module, checkA.portId);
			if (ok)
				return true;
			*why = "no cable was made";
			return false;
		}
		case Step::UNPATCH: {
			if (gCableCount(checkA) < checkCount)
				return true;
			*why = "the cable is still there";
			return false;
		}
		case Step::MENU: {
			// A menu left standing means the item was never clicked, and the next step would be
			// performed underneath it.
			for (widget::Widget* child : APP->scene->children) {
				ui::MenuOverlay* overlay = dynamic_cast<ui::MenuOverlay*>(child);
				if (overlay && overlay->visible && !overlay->requestedDelete) {
					*why = "the menu is still open, so \"" + s.arg + "\" was not chosen";
					return false;
				}
			}
			return true;
		}
		default:
			return true;
	}
}


void Runner::begin(int i) {
	index = i;
	if (i < 0 || i >= (int) steps.size()) {
		stop();
		return;
	}
	snapshot(i);
	const Step& s = steps[i];
	float spoken = 0.f;
	if (!s.note.empty()) {
		const math::Rect region = regionFor(stage, steps, i);
		// THE TRANSPORT MOVES FIRST, because it is an opaque window and a click aimed underneath
		// it would land on it instead — and because the card then chooses its berth against
		// where the transport has ended up rather than where it was.
		transportStepAside(region);
		card()->show(s.note, region);
		if (speak) {
			duckDown();
			spoken = speechPlay(s.note);
		}
	}
	expand(s);
	if (!running)
		return;   // expand() failed and stopped the run

	// NOTHING IS EVER TIME-STRETCHED. The sentence sets the floor and the rate multiplier
	// squeezes only the silence around it, so a hold is whichever is longer: the script's own
	// number, or however long the line actually takes to say. A note with no audio — nothing
	// rendered, or the voice switched off — falls back to the number.
	const float hold = s.note.empty() ? 0.f : pacing.hold;
	enter(NOTE, hold);
	if (spoken > 0.f)
		until = std::fmax(until, system::getTime() + spoken);
}


void Runner::run() {
	if (steps.empty())
		return;
	failure.clear();
	armSession();

	// ALWAYS FROM A CLEAN STAGE. A name left bound by the previous run would point at a module
	// that was deleted when the session went back, and the failure would read as a script error.
	std::string why;
	const bool bound = applyBindings(&why);
	(void) bound;
	if (!bindings.empty() && !bound) {
		// A patch step will fix this; without one there is nothing to run against.
		bool opensAPatch = !patchPath.empty();
		for (const Step& s : steps)
			opensAPatch = opensAPatch || s.kind == Step::OPEN || s.kind == Step::ADD;
		if (!opensAPatch) {
			failure = why;
			card()->show("The demo cannot start. " + why, math::Rect());
			return;
		}
	}

	duckCapture();

	running = true;
	theatre()->running = true;
	theatre()->live = true;
	raiseTheatre();
	if (phase == IDLE)
		begin(index);
}


void Runner::stop() {
	running = false;
	phase = IDLE;
	// A DEMO THAT STOPS STOPS TALKING, and gives the level back. Leaving a sentence running over
	// a rack that is no longer doing anything is the one thing a viewer cannot explain.
	speechSilence();
	duckUp();
	haveRest = false;
	camMoving = false;

	// THE BUTTON GOES BACK UP, whatever else happens. A demo stopped between a button-down and
	// its button-up leaves Rack believing a drag is still in progress, and the half-made cable
	// then follows the real mouse around the rack — which is not something the viewer can undo
	// by pressing anything on this window.
	if (buttonDown) {
		buttonDown = false;
		gRelease(theatre() ? theatre()->at() : math::Vec(), GLFW_MOUSE_BUTTON_LEFT);
	}
	// And if one is still hanging — a release that landed somewhere Rack did not accept — it is
	// taken off the rack rather than left for somebody to notice.
	for (app::CableWidget* cw : APP->scene->rack->getIncompleteCables()) {
		APP->scene->rack->removeCable(cw);
		delete cw;
	}

	if (Theatre* t = theatre()) {
		// The pointer stays drawn where it finished; only the performance ends. clear() puts
		// the operating system's cursor back.
		t->clear();
	}
}


void Runner::restart() {
	stop();
	card()->hide();
	index = 0;
	failure.clear();

	// THE RACK THE SCRIPT OPENS ON. A script that states a patch starts from it every time, so
	// two takes are the same take; one that does not starts from whatever is there.
	if (!patchPath.empty() && system::isFile(patchPath)) {
		armSession();
		try {
			APP->patch->load(patchPath);
			APP->patch->path = gSessionOwnPath;
		}
		catch (Exception& e) {
			WARN("DreamerDemo: could not open %s: %s", patchPath.c_str(), e.what());
		}
	}

	// The pointer starts in the middle rather than wherever the last run abandoned it, so two
	// takes of the same script open identically.
	theatre()->placeAt(APP->scene->box.size.mult(0.5f));
	run();
}


void Runner::stepOnce() {
	// EVERY WAIT COLLAPSED. An author walking a script is reading, not watching, and a sentence
	// per press would make stepping unusable. The step is performed by running its gestures with
	// no pauses between them.
	stop();
	theatre()->running = true;
	raiseTheatre();
	if (index < 0 || index >= (int) steps.size())
		return;

	snapshot(index);
	const Step& s = steps[index];
	if (!s.note.empty())
		card()->show(s.note, regionFor(stage, steps, index));

	running = true;
	expand(s);
	if (!running) {
		theatre()->running = true;
		return;
	}
	running = false;

	for (size_t k = 0; k < gests.size(); k++) {
		Gest& g = gests[k];
		switch (g.act) {
			case Gest::INSTANT: instant(s); break;
			case Gest::MOVE: theatre()->placeAt(g.pos); break;
			case Gest::MENU_MOVE: {
				math::Rect r;
				if (findMenuItem(g.arg, &r))
					theatre()->placeAt(r.pos.plus(r.size.div(2.f)));
				break;
			}
			// Even collapsed, a click has to span a frame or a momentary button will not see
			// it. Stepping is for an author, so a moment of waiting costs nothing.
			case Gest::CLICK_L: gClickHeld(g.pos, GLFW_MOUSE_BUTTON_LEFT); break;
			case Gest::CLICK_R: gClickHeld(g.pos, GLFW_MOUSE_BUTTON_RIGHT); break;
			case Gest::CLICK_HERE: gClickHeld(theatre()->at(), GLFW_MOUSE_BUTTON_LEFT); break;
			case Gest::DOWN:
				gHover(g.pos, math::Vec());
				gPress(g.pos, GLFW_MOUSE_BUTTON_LEFT);
				buttonDown = true;
				break;
			case Gest::DRAG:
				theatre()->placeAt(g.pos);
				gHover(g.pos, math::Vec(1.f, 1.f));
				break;
			case Gest::UP:
				gHover(g.pos, math::Vec());
				gRelease(g.pos, GLFW_MOUSE_BUTTON_LEFT);
				buttonDown = false;
				break;
			case Gest::SET_VALUE: gSetParam(g.target, g.value); break;
			case Gest::WHEEL:
				for (int p = 0; p < WHEEL_PULSES; p++)
					gScroll(g.pos, math::Vec(0.f, g.value >= 0.f ? 1.f : -1.f));
				break;
		}
	}

	std::string why;
	if (!verify(s, &why))
		failure = "Step " + std::to_string(index + 1) + ": " + why + ".";
	if (index < (int) steps.size() - 1)
		index++;
}


void Runner::back() {
	stop();
	theatre()->running = true;
	raiseTheatre();
	if (index > 0)
		index--;
	// A SNAPSHOT PER STEP is what makes going back as cheap as going forward: the patch as it
	// stood before that step is a file, and restoring it is a load.
	restoreSnapshot(index);
	const Step& s = steps[index];
	if (!s.note.empty())
		card()->show(s.note, regionFor(stage, steps, index));
	const Target a = stage.find(s.target);
	if (a.ok)
		theatre()->placeAt(a.centre());
}


void Runner::startGest() {
	Gest& g = gests[gi];
	theatre()->setBadge("");
	performStart = system::getTime();

	switch (g.act) {
		case Gest::MOVE:
			theatre()->travelTo(g.pos, pacing.perform);
			enter(PERFORM, pacing.perform);
			break;

		case Gest::MENU_MOVE: {
			math::Rect r;
			if (!findMenuItem(g.arg, &r)) {
				fail("Step " + std::to_string(index + 1) + ": the menu has no item called \""
					+ g.arg + "\".");
				return;
			}
			g.pos = r.pos.plus(r.size.div(2.f));
			g.glow = r;
			theatre()->travelTo(g.pos, pacing.perform);
			theatre()->glow(r, pacing.perform + pacing.arrive + pacing.beat);
			enter(PERFORM, pacing.perform);
			break;
		}

		case Gest::DRAG:
			lastPos = theatre()->at();
			theatre()->travelTo(g.pos, pacing.perform);
			enter(PERFORM, pacing.perform);
			break;

		// A CLICK IS HELD, NOT INSTANTANEOUS.
		//
		// Pressing and releasing in the same frame is invisible to anything that watches a
		// parameter for an edge — and a momentary button is exactly that: it rises on the press
		// and falls on the release, so a module stepping once per frame sees it at rest both
		// times and never learns it was pressed. That is why the chart button did nothing. The
		// button goes down here and comes up when the gesture ends, which is also what a real
		// click does.
		case Gest::CLICK_L:
		case Gest::CLICK_HERE:
			if (g.act == Gest::CLICK_HERE)
				g.pos = theatre()->at();
			gHover(g.pos, math::Vec());
			gPress(g.pos, GLFW_MOUSE_BUTTON_LEFT);
			buttonDown = true;
			theatre()->ripple();
			if (g.glow.size.x > 0.f)
				theatre()->glow(g.glow, 0.9f);
			enter(PERFORM, PRESS);
			break;

		case Gest::CLICK_R:
			gHover(g.pos, math::Vec());
			gPress(g.pos, GLFW_MOUSE_BUTTON_RIGHT);
			theatre()->ripple();
			if (g.glow.size.x > 0.f)
				theatre()->glow(g.glow, 0.9f);
			enter(PERFORM, PRESS);
			break;

		case Gest::DOWN:
			gHover(g.pos, math::Vec());
			gPress(g.pos, GLFW_MOUSE_BUTTON_LEFT);
			buttonDown = true;
			theatre()->ripple();
			if (g.glow.size.x > 0.f)
				theatre()->glow(g.glow, 0.9f);
			enter(PERFORM, PRESS);
			break;

		case Gest::UP:
			gHover(g.pos, math::Vec());
			gRelease(g.pos, GLFW_MOUSE_BUTTON_LEFT);
			buttonDown = false;
			if (g.glow.size.x > 0.f)
				theatre()->glow(g.glow, 0.9f);
			enter(PERFORM, PRESS);
			break;

		case Gest::SET_VALUE:
			setFrom = gParamUnit(g.target);
			setTo = g.value;
			if (g.glow.size.x > 0.f)
				theatre()->glow(g.glow, pacing.perform + 0.3f);
			enter(PERFORM, pacing.perform);
			break;

		case Gest::WHEEL:
			wheelDone = 0;
			if (g.glow.size.x > 0.f)
				theatre()->glow(g.glow, pacing.perform + 0.3f);
			enter(PERFORM, pacing.perform);
			break;

		case Gest::INSTANT:
			instant(steps[index]);
			if (!running)
				return;
			enter(PERFORM, PRESS);
			break;
	}
	performEnd = until;
}


void Runner::nextGest() {
	gi++;
	if (gi >= (int) gests.size()) {
		// The step is done. Ask it whether it did what it said.
		std::string why;
		if (!verify(steps[index], &why)) {
			fail("Step " + std::to_string(index + 1) + ": " + why + ".");
			return;
		}
		enter(SETTLE, pacing.settle + steps[index].wait);
		gi = (int) gests.size();   // SETTLE now means "the step has ended"
		return;
	}
	if (gests[gi].word.empty()) {
		// A step with nothing to announce: a zoom, a patch being opened.
		startGest();
		return;
	}
	theatre()->setBadge(gests[gi].word);
	enter(ANNOUNCE, pacing.beat);
}


void Runner::tick() {
	if (!running || phase == IDLE)
		return;

	camTick();

	const double now = system::getTime();

	// THE LEVEL COMES BACK THE MOMENT THE VOICE STOPS, not at the end of the step. A note holds
	// for as long as its sentence takes and often longer, and the patch should be at full level
	// for that remainder rather than under a voice that has finished.
	if (ducked && !speechSounding())
		duckUp();

	// PER-FRAME WORK, which happens whether or not the current pause has run out. A drag is a
	// stream of movements rather than an event, and a value travels rather than jumping.
	if (phase == PERFORM && gi >= 0 && gi < (int) gests.size()) {
		Gest& g = gests[gi];
		const float span = (float) std::fmax(0.001, performEnd - performStart);
		const float t = math::clamp((float) ((now - performStart) / span), 0.f, 1.f);
		if (g.act == Gest::DRAG) {
			const math::Vec p = theatre()->at();
			gHover(p, p.minus(lastPos));
			lastPos = p;
		}
		else if (g.act == Gest::MOVE || g.act == Gest::MENU_MOVE) {
			// A MENU HIGHLIGHTS WHAT THE POINTER IS OVER, and the pointer it believes in is the
			// last one it was told about. Hovering as the synthetic pointer travels is what puts
			// the highlight under it rather than under the viewer's real mouse.
			gHover(theatre()->at(), math::Vec());
		}
		else if (g.act == Gest::SET_VALUE) {
			gSetParam(g.target, setFrom + (setTo - setFrom) * t);
		}
		else if (g.act == Gest::WHEEL) {
			const int want = (int) (t * WHEEL_PULSES);
			while (wheelDone < want) {
				gScroll(g.pos, math::Vec(0.f, g.value >= 0.f ? 1.f : -1.f));
				wheelDone++;
			}
		}
	}

	if (now < until)
		return;

	switch (phase) {
		case NOTE:
			if (gests.empty()) {
				enter(SETTLE, pacing.settle + steps[index].wait);
				gi = 0;
			}
			else {
				gi = -1;
				nextGest();
			}
			break;

		case ANNOUNCE:
			startGest();
			break;

		case PERFORM: {
			// The button comes up at the end of the click it went down for.
			Gest& g = gests[gi];
			if (g.act == Gest::CLICK_L || g.act == Gest::CLICK_HERE) {
				gRelease(g.pos, GLFW_MOUSE_BUTTON_LEFT);
				buttonDown = false;
			}
			else if (g.act == Gest::CLICK_R) {
				gRelease(g.pos, GLFW_MOUSE_BUTTON_RIGHT);
			}
			// A MOVE WAITS WHERE IT LANDED. This pause is the whole of "announce, then do": the
			// pointer is on the control and nothing has happened yet, which is the moment the
			// viewer needs in order to see what is about to be acted on.
			enter(SETTLE, (g.act == Gest::MOVE || g.act == Gest::MENU_MOVE)
				? pacing.arrive : pacing.settle);
			break;
		}

		case SETTLE:
			if (gi >= (int) gests.size())
				begin(index + 1);
			else
				nextGest();
			break;

		default:
			break;
	}
}


} // namespace demo
