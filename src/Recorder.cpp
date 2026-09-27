/** RECORDING A TAKE PERFORMED BY HAND, and writing down what happened in it.

NOT A DEMO. Everything else in this plugin plays a script: it moves a pointer nobody is holding,
presses things and says what it is doing. This module does the opposite — it watches somebody
work and keeps a record of it. The two share a plugin because they share one screen-recording
permission and one build, and nothing else.

WHY BY HAND AT ALL. A scripted take is repeatable and exact, and it is also a great deal of work
for a demonstration of something a person could simply do: the script has to be able to express
every gesture, and any gesture it cannot express has to be built into it first. A take performed
by hand needs none of that. What it lacks is the script's other half — the words — and that is
what the record of events is for: the captions are written afterwards, against the film, and
each one needs a time.

WHAT IS WRITTEN DOWN is not the input but its effect. That a button went down says nothing worth
captioning; that the view moved to the row below, or that a cable appeared, is exactly what a
caption would say. Presses are noted too, with the name of whatever was under the pointer, since
a press that changes nothing visible — arming a knob, opening a menu — is still a moment.

THE FILE is a plain text one beside the MP4, under the same name. One line per moment, the time
first, counted from the instant the recording started rather than from anything in Rack:

     0.00  recording started
     4.21  Fundamental VCO, Frequency, pressed
     5.02  Fundamental VCO, Frequency set to 440.000 Hz
     6.80  view moved down to row 1
     9.15  2 rows on show
    12.44  cable made

F9 STARTS AND STOPS IT. A key rather than the button on the panel, because a take should not
open with the pointer travelling to a control and close with it travelling back — two seconds at
each end that nobody wants to watch and that cannot be trimmed without opening an editor. The
button is there for when the keyboard is not.

THE CURSOR IS NOT IN THE FILM. The recorder is told not to draw it, because a scripted take
draws its own; for a take performed by hand, switch on Clarity's drawn pointer, which is in the
rack itself and therefore in the picture, and which flashes on a click and names a held
modifier.
*/
#include "plugin.hpp"
#include "Capture.hpp"
#include "Runner.hpp"

#include <GLFW/glfw3.h>
#include <osdialog.h>

#include <cstdio>
#include <dlfcn.h>
#include <string>

namespace demo {


static const NVGcolor HOUSE_GREEN = nvgRGB(0x3d, 0xe0, 0x7a);
static const NVGcolor LIVE_RED = nvgRGB(0xe0, 0x45, 0x45);


struct RecorderModule : Module {
	enum ParamId { P_RECORD, NUM_PARAMS };

	RecorderModule() {
		config(NUM_PARAMS, 0, 0, 0);
		configButton(P_RECORD, "Record");
	}
};


/** WHAT IS UNDER THE POINTER, IN WORDS. Rack's own name for it where it has one — a parameter's
label, a port's, and the module they belong to — and nothing at all where it does not, since a
mangled C++ class name in a list of moments is worse than silence. */
static std::string whatIsUnder() {
	widget::Widget* w = APP->event ? APP->event->getHoveredWidget() : NULL;
	for (; w; w = w->parent) {
		if (app::ParamWidget* pw = dynamic_cast<app::ParamWidget*>(w)) {
			if (engine::ParamQuantity* pq = pw->getParamQuantity())
				return pq->module->model->name + ", " + pq->getLabel();
		}
		if (app::PortWidget* port = dynamic_cast<app::PortWidget*>(w)) {
			if (port->module) {
				engine::PortInfo* info = (port->type == engine::Port::INPUT)
					? (engine::PortInfo*) port->module->inputInfos[port->portId]
					: (engine::PortInfo*) port->module->outputInfos[port->portId];
				return port->module->model->name + ", "
					+ (info ? info->getName() : std::string("jack"));
			}
		}
		if (app::ModuleWidget* mw = dynamic_cast<app::ModuleWidget*>(w)) {
			if (mw->model)
				return mw->model->name;
		}
	}
	return "";
}


struct RecorderPanel : widget::Widget {
	RecorderModule* module = NULL;
	double* since = NULL;
	int* events = NULL;

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, nvgRGB(0x16, 0x1a, 0x20));
		nvgFill(args.vg);

		// The house mark: a green hairline inset from the edge, as on the other panels.
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 3.f, 3.f, box.size.x - 6.f, box.size.y - 6.f, 4.f);
		nvgStrokeColor(args.vg, nvgTransRGBA(HOUSE_GREEN, 0x60));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		std::shared_ptr<window::Font> font = uiFont();
		if (!font || !font->handle)
			return;
		nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		nvgFontSize(args.vg, 13.f);
		nvgFillColor(args.vg, INK);
		nvgText(args.vg, box.size.x / 2.f, 16.f, "Recorder", NULL);

		// THE LAMP: dark when idle, red while the film is being written.
		const bool live = captureRunning();
		const float cx = box.size.x / 2.f;
		const float cy = 52.f;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, 11.f);
		nvgFillColor(args.vg, live ? LIVE_RED : nvgRGB(0x26, 0x2b, 0x33));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgTransRGBA(INK, live ? 0xc0 : 0x50));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		nvgFontSize(args.vg, 11.f);
		nvgFillColor(args.vg, nvgTransRGBA(INK, 0xcc));
		if (live && since) {
			const double seconds = system::getTime() - *since;
			nvgText(args.vg, cx, 78.f,
				string::f("%d:%02d", (int) seconds / 60, (int) seconds % 60).c_str(), NULL);
			nvgFillColor(args.vg, nvgTransRGBA(INK, 0x88));
			nvgText(args.vg, cx, 94.f,
				string::f("%d noted", events ? *events : 0).c_str(), NULL);
		}
		else {
			nvgText(args.vg, cx, 78.f, "F9", NULL);
			nvgFillColor(args.vg, nvgTransRGBA(INK, 0x88));
			nvgText(args.vg, cx, 94.f, "to record", NULL);
		}
	}
};


struct RecorderWidget : ModuleWidget {
	RecorderPanel* face = NULL;

	/** The recording, and what has been noticed about it. */
	std::string logPath;
	double recordFrom = 0.0;
	int events = 0;
	int wasTop = 0, wasRows = 0, wasCables = -1, wasModules = -1;
	bool wasHeld = false, keyHeld = false, buttonWas = false, wasMenu = false;
	/** THE CONTROL A PRESS LANDED ON, and what it read before the hand moved it. A knob turned
	is one of the few things worth captioning that leaves no other trace: no cable appears, the
	view does not move, and the only evidence is a number that is now different. Noted when the
	button comes up rather than while it is down, so a sweep of a knob is one line and not sixty.
	*/
	WeakPtr<app::ParamWidget> pressed;
	float pressedWas = 0.f;

	RecorderWidget(RecorderModule* module) {
		setModule(module);
		box.size = math::Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
		face = new RecorderPanel;
		face->module = module;
		face->since = &recordFrom;
		face->events = &events;
		face->box.size = box.size;
		addChild(face);
	}

	void note(const std::string& what) {
		if (logPath.empty())
			return;
		FILE* f = std::fopen(logPath.c_str(), "a");
		if (!f)
			return;
		std::fprintf(f, "%6.2f  %s\n", system::getTime() - recordFrom, what.c_str());
		std::fclose(f);
		events++;
	}

	void start() {
		std::string why;
		const std::string path = takePath("Rack");
		if (!captureStart(path, &why)) {
			osdialog_message(OSDIALOG_ERROR, OSDIALOG_OK, why.c_str());
			return;
		}
		const size_t dot = path.rfind('.');
		logPath = (dot == std::string::npos ? path : path.substr(0, dot)) + ".txt";
		recordFrom = system::getTime();
		events = 0;
		wasCables = -1;
		wasModules = -1;
		wasTop = wasRows = 0;
		wasMenu = false;
		note("recording started");
	}

	void stop() {
		note("recording stopped");
		logPath.clear();
		captureStop();
	}

	void toggle() {
		if (captureRunning())
			stop();
		else
			start();
	}

	/** READ FROM THE KEYBOARD rather than waited for: Rack's scene answers keys before anything
	in it is offered them, and a recording should start whatever the pointer happens to be over.
	Not while something is being typed into, which has first claim on every key. */
	void recordKey() {
		GLFWwindow* win = APP->window ? APP->window->win : NULL;
		if (!win)
			return;
		if (APP->event && dynamic_cast<ui::TextField*>(APP->event->selectedWidget)) {
			keyHeld = false;
			return;
		}
		const bool down = glfwGetKey(win, GLFW_KEY_F9) == GLFW_PRESS
			&& (APP->window->getMods() & RACK_MOD_MASK) == 0;
		if (down != keyHeld) {
			keyHeld = down;
			if (down)
				toggle();
		}
	}

	/** Clarity's account of where the view is, if Clarity is installed. Asked of that plugin's
	own library by path, since Rack opens every plugin with RTLD_LOCAL and nothing they export
	is in the program's global namespace. */
	typedef bool (*WhereFn)(int*, int*);
	static WhereFn rowWhere() {
		static WhereFn fn = NULL;
		static bool looked = false;
		if (!looked) {
			looked = true;
			if (plugin::Plugin* p = plugin::getPlugin("DreamerDevelopment")) {
				const std::string lib = p->path + "/plugin.dylib";
				if (void* h = dlopen(lib.c_str(), RTLD_NOW | RTLD_LOCAL))
					fn = (WhereFn) dlsym(h, "drRowViewWhere");
			}
		}
		return fn;
	}

	void watch() {
		if (logPath.empty() || !captureRunning())
			return;

		if (WhereFn where = rowWhere()) {
			int top = 0, rows = 0;
			if (where(&top, &rows)) {
				if (rows != wasRows) {
					const bool first = wasRows == 0;
					wasRows = rows;
					if (!first)
						note(string::f("%d row%s on show", rows, rows == 1 ? "" : "s"));
				}
				if (top != wasTop) {
					const bool down = top > wasTop;
					wasTop = top;
					note(string::f("view moved %s to row %d", down ? "down" : "up", top));
				}
			}
		}

		const int cables = (int) APP->engine->getCableIds().size();
		if (cables != wasCables) {
			if (wasCables >= 0)
				note(cables > wasCables ? "cable made" : "cable removed");
			wasCables = cables;
		}

		const int modules = (int) APP->engine->getModuleIds().size();
		if (modules != wasModules) {
			if (wasModules >= 0)
				note(modules > wasModules ? "module added" : "module removed");
			wasModules = modules;
		}

		// A MENU IS A MOMENT, and a long one: it is open while somebody reads it.
		bool menu = false;
		if (APP->scene) {
			for (widget::Widget* child : APP->scene->children) {
				ui::MenuOverlay* over = dynamic_cast<ui::MenuOverlay*>(child);
				if (over && over->isVisible() && !over->requestedDelete)
					menu = true;
			}
		}
		if (menu != wasMenu) {
			wasMenu = menu;
			note(menu ? "menu opened" : "menu closed");
		}

		// A PRESS IS A MOMENT even when nothing visible comes of it, and what it was on is the
		// useful half of it.
		GLFWwindow* win = APP->window ? APP->window->win : NULL;
		const bool held = win && glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
		if (held != wasHeld) {
			wasHeld = held;
			if (held) {
				const std::string what = whatIsUnder();
				note(what.empty() ? "pressed" : (what + ", pressed"));
				pressed = dynamic_cast<app::ParamWidget*>(
					APP->event ? APP->event->getHoveredWidget() : NULL);
				if (!pressed) {
					// A knob is usually a child of the widget that carries the parameter.
					for (widget::Widget* w = APP->event ? APP->event->getHoveredWidget() : NULL;
						w; w = w->parent) {
						if (app::ParamWidget* pw = dynamic_cast<app::ParamWidget*>(w)) {
							pressed = pw;
							break;
						}
					}
				}
				engine::ParamQuantity* pq = pressed ? pressed->getParamQuantity() : NULL;
				pressedWas = pq ? pq->getValue() : 0.f;
			}
			else if (pressed) {
				// WHAT IT WAS TURNED TO, in the words Rack itself would show: a tooltip's
				// reading, so "440.000 Hz" rather than a number that means nothing outside
				// that module.
				engine::ParamQuantity* pq = pressed->getParamQuantity();
				if (pq && pq->getValue() != pressedWas) {
					note(pq->module->model->name + ", " + pq->getLabel()
						+ " set to " + pq->getDisplayValueString() + pq->getUnit());
				}
				pressed = NULL;
			}
		}
	}

	void step() override {
		ModuleWidget::step();
		recordKey();
		// The panel's own button, for when the keyboard is not to hand.
		if (module) {
			const bool down = module->params[RecorderModule::P_RECORD].getValue() > 0.5f;
			if (down != buttonWas) {
				buttonWas = down;
				if (down)
					toggle();
			}
		}

		watch();

		// A TAKE THAT HAS JUST BEEN WRITTEN SAYS SO, with its path on the clipboard, which is
		// what anybody does with it next.
		std::string file;
		unsigned long long bytes = 0;
		if (captureFinished(&file, &bytes)) {
			if (APP->window && APP->window->win)
				glfwSetClipboardString(APP->window->win, file.c_str());
			osdialog_message(OSDIALOG_INFO, OSDIALOG_OK,
				("Recording saved.\n\n" + file + "\n\n"
				 + string::f("%.1f MB. The path is on the clipboard.",
					(double) bytes / (1024.0 * 1024.0))).c_str());
		}
	}

	void onButton(const ButtonEvent& e) override {
		// The lamp is the button: a press anywhere on the face starts or stops it, since there
		// is nothing else on this panel to press.
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT
			&& (e.mods & RACK_MOD_MASK) == 0) {
			toggle();
			e.consume(this);
			return;
		}
		ModuleWidget::onButton(e);
	}
};


} // namespace demo


Model* modelRecorder = createModel<demo::RecorderModule, demo::RecorderWidget>("Recorder");
