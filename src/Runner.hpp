#pragma once
/** The step machine: what turns a list of steps into a paced performance.

ANNOUNCE, THEN DO. Each step puts its note up and holds it, then every gesture it expands into is
named on the badge, held for a beat, and only then performed. A fast movement you were told about
is easier to follow than a slow one that surprises you; this ordering, rather than a slower
overall rate, is what makes a demo followable.

A STEP IS SEMANTIC, which is the grain an author dictates in: "patch the chart's chord output
into the quantizer". The runner expands it into the gestures the interface really uses — move
pointer, button down, drag, button up — and the badge names them one at a time. An author never
writes a gesture.

EVERY STEP CHECKS ITSELF. A gesture goes through Rack's own event system, so it can miss: a press
two pixels off a jack does nothing at all. A step that claims to change something asks afterwards
whether it did, and a failure stops the run and names the step rather than letting a take carry
on with the rest of the script acting on a patch that was never made.
*/
#include "plugin.hpp"
#include "Resolve.hpp"

namespace demo {


/** The pacing, in seconds. A script sets these as its defaults and a step overrides any of them
for itself. The numbers are what a person can judge while watching; the rate multiplier is for
re-timing a finished script and is a last resort. */
struct Pacing {
	float perform = 1.0f;   /**< how long the pointer takes to travel, or a value to move */
	float arrive = 0.9f;    /**< the pause after it lands, before anything happens there */
	float beat = 0.7f;      /**< how long the badge is up before the gesture fires */
	float settle = 0.6f;    /**< the pause after a gesture, before the next one */
	float hold = 2.4f;      /**< how long a new note stays up before the demo acts on it */
};


struct Step {
	/** The note. Empty leaves the previous note up, which is how one note covers several
	steps. */
	std::string note;

	enum Kind {
		SAY,          /**< nothing happens; the note is the whole step */
		WAIT,         /**< let the patch play */
		POINT,        /**< go there and say the note; touch nothing */
		CLICK,
		RIGHT_CLICK,  /**< opens a context menu, since that is what it does in Rack */
		SET,          /**< move a parameter to `value`, a fraction of its own range */
		SCROLL,
		PATCH,        /**< join `target` to `target2`, either way round */
		UNPATCH,      /**< pull whatever is on `target` off and drop it */
		MENU,         /**< right-click, then choose `arg` from the menu that appears */
		MOVE_MODULE,  /**< drag a module by its panel, `value` HP across and `value2` rows down */
		ZOOM,         /**< frame a module, or the whole rack when `target` is empty */
		PAN,          /**< move the view without changing how close it is */
		OPEN,         /**< load a patch file */
		ADD,          /**< add a module of model `arg` and bind `target` to it */
	};
	Kind kind = SAY;

	std::string target;    /**< "name" or "name:control" */
	std::string target2;   /**< PATCH only */
	std::string arg;       /**< MENU: the item. OPEN: a path. ADD: "Plugin/Model". */
	float value = 0.f;     /**< SET: nought to one across the parameter's range */
	float value2 = 0.f;
	float wait = 0.f;      /**< extra pause after the step */

	/** The line of the script this came from, for saying which step went wrong. */
	int line = 0;
};


struct Runner {
	Pacing pacing;
	float rate = 1.0f;     /**< scales every wait; speech, when it arrives, is never scaled */
	std::vector<Step> steps;
	Stage stage;

	/** The bindings the script's header declared, kept so they can be applied again after a
	patch is loaded — every module in the rack is a different object then. */
	std::vector<std::pair<std::string, std::string> > bindings;
	/** The patch the script opens on, and where the script came from. */
	std::string patchPath;
	std::string scriptPath, title;

	/** THE NARRATION. A note's hold becomes however long its own sentence takes: speech sets the
	floor and the rate multiplier squeezes only the silences around it, because a sentence played
	faster is a sentence nobody can follow. */
	bool speak = true;
	std::string voice = "Karen (Premium)";
	int voiceRate = 175;

	/** The parameter pulled down while a line is spoken, and what it falls to. One recorded
	audio track cannot be rebalanced afterwards, so the balance is made while it plays. */
	std::string master;
	float duck = 0.35f;

	/** Renders any line of this script that has no audio yet. Called when a script is loaded,
	not when it is run, because rendering is slow the first time and instant after. */
	int render();

	/** Empty unless a step's own check failed. The run stops and this is what it says. */
	std::string failure;

	bool isRunning() const { return running; }
	int at() const { return index; }

	void load(const std::vector<Step>& s);
	/** Bind every name the header declared. Called at the start of a run and after any step
	that replaces the rack. */
	bool applyBindings(std::string* why);

	void run();
	/** Stops where it stands, leaving the rack as the demo built it. */
	void stop();
	void restart();
	/** Perform one step with every wait collapsed, and stand on the next. Silent: an author
	walking a script is reading, not listening. */
	void stepOnce();
	void back();

	/** THE SESSION GUARD. A demo rebuilds the rack, so the patch that was there when it started
	is put back when it ends, however it ends. Armed by the first run and released by Reset, by
	closing the transport, or by loading another script. */
	void armSession();
	void releaseSession();
	bool sessionArmed() const { return armed; }

	/** Called once a frame by the transport. */
	void tick();

private:
	/** One gesture, which is what a step expands into. The badge names it; the runner performs
	it; nothing here is ever authored. */
	struct Gest {
		std::string word;
		enum Do {
			MOVE, CLICK_L, CLICK_R, DOWN, DRAG, UP, SET_VALUE, WHEEL,
			MENU_MOVE,   /**< travel to a menu item, whose position only exists once the menu is
			             open, so it is resolved when this gesture starts rather than when the
			             step was expanded */
			CLICK_HERE,  /**< click wherever the pointer is */
			INSTANT,     /**< something with no pointer in it: a zoom, a patch being loaded */
		};
		Do act = MOVE;
		math::Vec pos;
		math::Rect glow;
		float value = 0.f;
		std::string arg;
		Target target;
	};

	enum Phase { IDLE, NOTE, ANNOUNCE, PERFORM, SETTLE };
	Phase phase = IDLE;
	bool running = false;
	bool armed = false;
	int index = 0;
	double until = 0.0;

	std::vector<Gest> gests;
	int gi = 0;
	double performStart = 0.0, performEnd = 0.0;
	math::Vec lastPos;
	/** True between a button-down gesture and its button-up. A run that ends in between — Stop,
	a failed check, the end of the script, the window closing — must put the button back up, or
	Rack is left mid-drag with a cable hanging off the real mouse pointer. */
	bool buttonDown = false;
	float setFrom = 0.f, setTo = 0.f;
	int wheelDone = 0;

	Target checkA, checkB;
	int checkCount = 0;

	/** THE CAMERA, which runs on its own clock rather than as a gesture.

	A view change is not something the pointer does, and it should happen WHILE the sentence
	about it is being read rather than after — a demo that talks about a module for four seconds
	and only then brings it into view has described something the viewer cannot see. So a zoom or
	a pan starts as its step's note goes up and eases from where the view is to where it should
	be, over the same clock as everything else.

	Zoom is interpolated geometrically. Halfway between one and four is two, not two and a half:
	a linear ride between two zoom levels rushes at one end and crawls at the other. */
	bool camMoving = false;
	double camStart = 0.0, camEnd = 0.0;
	float camZoomFrom = 1.f, camZoomTo = 1.f;
	math::Vec camGridFrom, camGridTo;
	/** THE POINTER TRAVELS WITH THE CAMERA when the move is a zoom onto something.

	Zooming to a module means "look at this", and the pointer is what says which thing is being
	looked at — so it walks there over the same seconds, arriving as the view settles. Its
	destination is recomputed every frame, because the module is moving across the screen while
	the camera is moving towards it.

	A pan does not do this. Panning is framing, not attention. */
	std::string camPointTarget;
	math::Vec camPointFrom;

	/** Aims the camera at a bound in module coordinates, and starts the move. */
	void camTo(math::Rect bound, float seconds);
	void camTick();

	/** WHERE THE MASTER RESTS, captured once when a run starts rather than each time the voice
	begins. Capturing it per line meant a duck landing on an already-ducked value took THAT as
	the resting level: multiply a level by a third a few times and it reaches silence, with
	nothing left that knows what to put back. Once per run cannot compound, and one restore puts
	it right however many lines were spoken. */
	bool haveRest = false;
	float masterRest = 0.f;
	bool ducked = false;
	void duckCapture();
	void duckDown();
	void duckUp();

	void begin(int i);
	void enter(Phase p, float seconds);
	void expand(const Step& s);
	void startGest();
	void nextGest();
	bool verify(const Step& s, std::string* why);
	void fail(const std::string& why);
	void instant(const Step& s);
	void snapshot(int i);
	void restoreSnapshot(int i);
};


} // namespace demo
