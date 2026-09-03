#include "Runner.hpp"
#include "Theatre.hpp"
#include "Card.hpp"

namespace demo {


/** How long the pointer is shown pressed. Short, because it is punctuation rather than an
event: the ripple is what says a click happened. */
static const float PRESS = 0.18f;


math::Vec Runner::scene() const {
	return APP->scene->box.size;
}


void Runner::load(const std::vector<Step>& s) {
	stop();
	steps = s;
	index = 0;
}


void Runner::enter(Phase p, float seconds) {
	phase = p;
	until = system::getTime() + std::fmax(0.f, seconds) / std::fmax(0.1f, rate);
}


/** THE REGION THE COMING STEPS WILL TOUCH, so the card can take a berth clear of it. It runs to
the next note rather than to the next step, because the card is up for exactly that long. */
static math::Rect regionFrom(const std::vector<Step>& steps, int from, math::Vec scene) {
	math::Rect r;
	bool any = false;
	for (size_t i = (size_t) from; i < steps.size(); i++) {
		if (i > (size_t) from && !steps[i].note.empty())
			break;
		if (steps[i].frac.x < 0.f)
			continue;
		const math::Vec p = math::Vec(steps[i].frac.x * scene.x, steps[i].frac.y * scene.y);
		// Generous: a pointer at a control is a hand's worth of screen, not a point.
		const math::Rect one(p.minus(math::Vec(90.f, 70.f)), math::Vec(180.f, 140.f));
		r = any ? r.expand(one) : one;
		any = true;
	}
	return any ? r : math::Rect();
}


void Runner::begin(int i) {
	index = i;
	if (i < 0 || i >= (int) steps.size()) {
		stop();
		return;
	}
	const Step& s = steps[i];
	if (!s.note.empty()) {
		card()->show(s.note, regionFrom(steps, i, scene()));
		enter(NOTE, pacing.hold);
	}
	else {
		enter(NOTE, 0.f);
	}
}


void Runner::run() {
	if (steps.empty())
		return;
	running = true;
	theatre()->running = true;
	raiseTheatre();
	if (phase == IDLE)
		begin(index);
}


void Runner::stop() {
	running = false;
	phase = IDLE;
	if (Theatre* t = theatre()) {
		t->running = false;
		t->clear();
	}
}


void Runner::restart() {
	stop();
	card()->hide();
	index = 0;
	// The pointer starts in the middle rather than wherever the last run abandoned it, so two
	// takes of the same script open identically.
	theatre()->placeAt(scene().mult(0.5f));
	run();
}


void Runner::perform(int i, bool silent) {
	// Everything a step does, with none of its waits. Used by Step and Back, where the author is
	// reading rather than watching.
	if (i < 0 || i >= (int) steps.size())
		return;
	const Step& s = steps[i];
	if (!s.note.empty())
		card()->show(s.note, regionFrom(steps, i, scene()));
	if (s.frac.x >= 0.f)
		theatre()->placeAt(math::Vec(s.frac.x * scene().x, s.frac.y * scene().y));
	if (!silent && s.act)
		theatre()->ripple();
}


void Runner::stepOnce() {
	stop();
	theatre()->running = true;   // the pointer stays visible while an author walks a script
	raiseTheatre();
	perform(index, false);
	if (index < (int) steps.size() - 1)
		index++;
}


void Runner::back() {
	stop();
	theatre()->running = true;
	raiseTheatre();
	if (index > 0)
		index--;
	// EVERY STEP FROM THE TOP, silently. A step back has to arrive at the state that step left,
	// and in phase one that state is only where the pointer is and which note is up. Phase three
	// replaces this with a patch snapshot per step, which is what makes it cheap.
	for (int i = 0; i <= index; i++)
		perform(i, true);
}


void Runner::tick() {
	if (!running || phase == IDLE)
		return;
	const double now = system::getTime();
	if (now < until)
		return;

	const Step& s = steps[index];
	const math::Vec sc = scene();

	switch (phase) {
		case NOTE:
			if (s.frac.x >= 0.f) {
				theatre()->setBadge("move pointer");
				enter(ANNOUNCE_MOVE, pacing.beat);
			}
			else if (!s.gesture.empty()) {
				theatre()->setBadge(s.gesture);
				enter(ANNOUNCE_ACT, pacing.beat);
			}
			else {
				enter(SETTLE, pacing.settle + s.wait);
			}
			break;

		case ANNOUNCE_MOVE:
			theatre()->setBadge("");
			theatre()->travelTo(math::Vec(s.frac.x * sc.x, s.frac.y * sc.y), pacing.perform);
			enter(TRAVEL, pacing.perform);
			break;

		case TRAVEL:
			// WAITS WHERE IT LANDED. This pause is the whole of "announce, then do": the pointer
			// is on the control and nothing has happened yet, which is the moment the viewer
			// needs in order to see what is about to be acted on.
			if (!s.gesture.empty())
				enter(ARRIVE, pacing.arrive);
			else
				enter(SETTLE, pacing.settle + s.wait);
			break;

		case ARRIVE:
			theatre()->setBadge(s.gesture);
			enter(ANNOUNCE_ACT, pacing.beat);
			break;

		// The badge has had its beat. Now the gesture.
		case ANNOUNCE_ACT:
			if (s.act) {
				theatre()->ripple();
				if (s.glowFrac.size.x > 0.f) {
					theatre()->glow(math::Rect(
						math::Vec(s.glowFrac.pos.x * sc.x, s.glowFrac.pos.y * sc.y),
						math::Vec(s.glowFrac.size.x * sc.x, s.glowFrac.size.y * sc.y)), 0.9f);
				}
			}
			theatre()->setBadge("");
			enter(SETTLE, PRESS + pacing.settle + s.wait);
			break;

		case SETTLE:
			begin(index + 1);
			break;

		default:
			break;
	}
}


} // namespace demo
