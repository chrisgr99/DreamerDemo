#pragma once
/** The step machine: what turns a list of steps into a paced performance.

ANNOUNCE, THEN DO. Each step puts its note up and holds it, the pointer travels and WAITS at the
destination, the badge names the gesture and holds a beat, and only then does the gesture happen.
A fast movement you were told about is easier to follow than a slow one that surprises you; this
ordering, rather than a slower overall rate, is what makes a demo followable.

PHASE ONE. A step's target is a fraction of the window rather than a control, and a gesture is a
ripple rather than an injected event. Everything about the pacing, the phases and the transport's
behaviour is final; what is temporary is where a position comes from and what happens when the
gesture fires. Phase two replaces both without touching this machine.
*/
#include "plugin.hpp"

namespace demo {


/** The pacing, in seconds. A script sets these as its defaults and a step overrides any of them
for itself. The numbers are what a person can judge while watching; the rate multiplier is for
re-timing a finished script and is a last resort. */
struct Pacing {
	float perform = 1.0f;   /**< how long the pointer takes to travel */
	float arrive = 0.9f;    /**< the pause after it lands, before anything happens there */
	float beat = 0.7f;      /**< how long the badge is up before the gesture fires */
	float settle = 0.8f;    /**< the pause after the action, before the next step */
	float hold = 2.4f;      /**< how long a new note stays up before the demo acts on it */
};


struct Step {
	/** The note. Empty leaves the previous note up, which is how one note covers several
	steps. */
	std::string note;
	/** Where to go, as a fraction of the window. A negative x means stay where you are. */
	math::Vec frac = math::Vec(-1.f, -1.f);
	/** One of GESTURES, or empty for a step that only moves. */
	std::string gesture;
	/** Whether the gesture ends in a ripple. Phase two makes this an injected event. */
	bool act = false;
	/** A region to ring while the gesture happens, as fractions of the window. */
	math::Rect glowFrac;
	/** Extra pause after the step, on top of `settle`. */
	float wait = 0.f;
};


struct Runner {
	Pacing pacing;
	float rate = 1.0f;      /**< scales every wait; speech, when it arrives, is never scaled */
	std::vector<Step> steps;

	bool isRunning() const { return running; }
	int at() const { return index; }

	void load(const std::vector<Step>& s);
	void run();
	/** Stops where it stands, leaving the rack as the demo built it. */
	void stop();
	void restart();
	/** Perform one step with every wait collapsed, and stand on the next. Silent: an author
	walking a script is reading, not listening. */
	void stepOnce();
	void back();

	/** Called once a frame by the transport. */
	void tick();

private:
	/** One step in order: its note goes up and holds, the move is announced, the pointer
	travels, it waits where it landed, the gesture is announced, the gesture happens, and the
	step settles. Every one of those is a named pause somebody can change. */
	enum Phase { IDLE, NOTE, ANNOUNCE_MOVE, TRAVEL, ARRIVE, ANNOUNCE_ACT, SETTLE };
	Phase phase = IDLE;
	bool running = false;
	int index = 0;
	double until = 0.0;

	math::Vec scene() const;
	void begin(int i);
	void enter(Phase p, float seconds);
	void perform(int i, bool silent);
};


} // namespace demo
