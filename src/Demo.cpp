/** The module, which exists only to open the transport.

A plugin has to have a module for Rack to have anything to put in the browser, and a demo has to
be startable from somewhere. That is the whole of it: no parameters, no ports, no audio. The
module can sit on any page of any patch, and a demo run from it can be about anything.
*/
#include "plugin.hpp"
#include "Capture.hpp"
#include "Report.hpp"

#include <patch.hpp>

#include <osdialog.h>

#include <GLFW/glfw3.h>

namespace demo {


struct DemoModule : Module {
	/** Whether the patch report keeps itself up to date. Saved with the patch, because it is a
	property of how you work rather than a preference of the program. */
	bool autoReport = true;

	DemoModule() {
		config(0, 0, 0, 0);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "autoReport", json_boolean(autoReport));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* j = json_object_get(rootJ, "autoReport");
		if (j)
			autoReport = json_boolean_value(j);
	}
};


/** The panel, drawn rather than loaded. There is no artwork to ship for a face carrying one
button, and a drawn panel cannot be out of step with the code that places things on it. */
/** The green of Dreamer Development's other panels, and of every lit thing on them. */
static const NVGcolor HOUSE_GREEN = nvgRGB(0x3d, 0xe0, 0x7a);


struct DemoPanel : widget::Widget {
	/** The green line around the edge is the Dreamer Development house mark, drawn here exactly
	as those panels draw it: the same colour, the same inset, the same corner. A rack holding
	several of these should look like it holds a set. */
	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, nvgRGB(0x16, 0x1a, 0x20));
		nvgFill(args.vg);

		std::shared_ptr<window::Font> font = uiFont();
		if (font && font->handle >= 0) {
			// The title band, inside the border rather than under it: drawn to the panel's own
			// edge it covers the line along the top, which reads as a border left unfinished.
			nvgBeginPath(args.vg);
			nvgRect(args.vg, 4.f, 4.f, box.size.x - 8.f, 25.f);
			nvgFillColor(args.vg, nvgRGB(0x24, 0x2a, 0x33));
			nvgFill(args.vg);

			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 15.f);
			nvgFillColor(args.vg, nvgRGB(0xe6, 0xe8, 0xec));
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgText(args.vg, box.size.x / 2.f, 15.f, "Demo", NULL);
		}

		// A green rule under the title, tying the head of the panel to its border.
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 8.f, 33.f);
		nvgLineTo(args.vg, box.size.x - 8.f, 33.f);
		nvgStrokeColor(args.vg, HOUSE_GREEN);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		Widget::draw(args);

		// LAST, so nothing is drawn over it.
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 3.f, 3.f, box.size.x - 6.f, box.size.y - 6.f, 6.f);
		nvgStrokeColor(args.vg, HOUSE_GREEN);
		nvgStrokeWidth(args.vg, 1.2f);
		nvgStroke(args.vg);
	}
};


/** THE CONTROLS, ON THE PANEL WHERE A MODULE'S CONTROLS BELONG.

Everything about preparing a demo is here: which script, reloading it after an edit, opening its
patch to correct it, the voice, the badges, the captions, the rate. It is a column of the same
chips the transport uses, because they are the same controls — the panel is a second view of
them rather than a copy.

Only the three that drive a take are left in the floating window, since those have to be
reachable while the demo owns the screen and this panel may be nowhere near the view. */
struct ControlColumn : widget::OpaqueWidget {
	static const int ROW_H = 22;
	static const int ROW_GAP = 3;

	math::Rect rowRect(int row) {
		return math::Rect(math::Vec(0.f, (float) (row * (ROW_H + ROW_GAP))),
			math::Vec(box.size.x, (float) ROW_H));
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			for (int i = 0; i < demoPanelRows(); i++) {
				if (!rowRect(i).contains(e.pos))
					continue;
				// The list of scripts drops out of the row that was pressed, so it appears
				// where the hand already is rather than beside a window somewhere else.
				const math::Rect r = rowRect(i);
				demoPanelPress(i, math::Rect(getAbsoluteOffset(r.pos), r.size));
				e.consume(this);
				e.stopPropagating();
				return;
			}
		}
		OpaqueWidget::onButton(e);
	}

	/** A ROUND BUTTON WITH ITS NAME BESIDE IT, which is what a control on a module's panel looks
	like in Rack. The chips this started as belong on a floating window, where there is no panel
	to be part of; on a faceplate they read as something borrowed from another program.

	Green when the button will start something, red when it will stop it — the two states worth
	telling apart from across a room — and the accent colour for the settings that are simply on
	or off. The whole row answers the click, not just the disc: it is a wide, easy target for
	something pressed while looking at the rack rather than at the panel. */
	void draw(const DrawArgs& args) override {
		std::shared_ptr<window::Font> font = uiFont();
		for (int i = 0; i < demoPanelRows(); i++) {
			const math::Rect r = rowRect(i);
			const std::string label = demoPanelLabel(i);
			if (demoPanelIsField(i)) {
				demoDrawField(args.vg, r, label);
				continue;
			}

			const float d = 13.f;
			const math::Vec c(r.pos.x + 2.f + d / 2.f, r.pos.y + r.size.y / 2.f);
			NVGcolor fill = nvgRGB(0x22, 0x22, 0x26);
			NVGcolor ring = nvgRGBA(0xcf, 0xcf, 0xcf, 0x90);
			if (label == "Stop") {
				fill = nvgRGB(0xe0, 0x3b, 0x3b);
				ring = fill;
			}
			else if (label == "Run" || label == "Reset") {
				fill = nvgRGB(0x3d, 0xe0, 0x7a);
				ring = fill;
			}
			else if (demoPanelLit(i)) {
				fill = ACCENT;
				ring = ACCENT;
			}

			nvgBeginPath(args.vg);
			nvgCircle(args.vg, c.x, c.y, d / 2.f);
			nvgFillColor(args.vg, fill);
			nvgFill(args.vg);
			nvgStrokeColor(args.vg, ring);
			nvgStrokeWidth(args.vg, 1.f);
			nvgStroke(args.vg);

			if (!font || font->handle < 0)
				continue;
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 13.f);
			nvgFillColor(args.vg, INK);
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgText(args.vg, c.x + d / 2.f + 8.f, r.pos.y + r.size.y / 2.f + 0.5f,
				label.c_str(), NULL);
		}
		OpaqueWidget::draw(args);
	}
};


struct DemoWidget : ModuleWidget {
	/** WRITTEN ON THE SAME BEAT AS RACK'S OWN AUTOSAVE. Fifteen seconds is Rack's autosave
	interval, so the report and the patch file it explains are never more than one interval
	apart, and nobody has to remember to press anything before asking a question about a patch.

	It is the widget rather than the module that does this: the report is a walk of the widget
	tree and the engine's cable list, which is main-thread work and has no business happening in
	an audio callback. */
	double nextReport = 0.0;

	void step() override {
		ModuleWidget::step();
		// Only until the runner window exists, which the first pending press opens; after that
		// it does its own pumping, out of reach of a patch load that would take this away.
		if (!transportOpen())
			demoPanelPump();

		// A TAKE THAT HAS JUST BEEN WRITTEN SAYS SO, here rather than where it happened: a run
		// usually ends with Escape, which closes the runner and takes the demo off the screen,
		// and a message put up at that moment would go with it. The panel is still standing.
		//
		// The path goes on the clipboard as well as into the dialogue, because the useful thing
		// to do with it next is paste it somewhere.
		std::string file;
		unsigned long long bytes = 0;
		if (captureFinished(&file, &bytes)) {
			if (APP->window && APP->window->win)
				glfwSetClipboardString(APP->window->win, file.c_str());
			const std::string text = "Recording saved.\n\n" + file + "\n\n"
				+ string::f("%.1f MB. The path is on the clipboard.",
					(double) bytes / (1024.0 * 1024.0));
			osdialog_message(OSDIALOG_INFO, OSDIALOG_OK, text.c_str());
		}
		DemoModule* demo = dynamic_cast<DemoModule*>(module);
		if (!demo || !demo->autoReport)
			return;
		const double now = system::getTime();
		if (now < nextReport)
			return;
		nextReport = now + 15.0;
		writePatchReportIfChanged();
	}

	DemoWidget(DemoModule* module) {
		setModule(module);
		// TEN HP, WHICH IS AS MUCH AS THE CONTROLS NEED. A module that is only in a rack
		// while a video is being made can afford the room, and the alternative — a floating
		// window carrying everything — is a window that has to be moved out of the way of the
		// thing it is demonstrating.
		box.size = math::Vec(10 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

		DemoPanel* panel = new DemoPanel;
		panel->box.size = box.size;
		addChild(panel);

		ControlColumn* controls = new ControlColumn;
		controls->box.pos = math::Vec(8.f, 44.f);
		controls->box.size = math::Vec(box.size.x - 16.f,
			(float) (demoPanelRows() * (ControlColumn::ROW_H + ControlColumn::ROW_GAP)));
		addChild(controls);

		addChild(createWidget<ScrewSilver>(math::Vec(0.f, 0.f)));
		addChild(createWidget<ScrewSilver>(
			math::Vec(box.size.x - RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}

	void appendContextMenu(Menu* menu) override {
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Show the runner", "", []() {
			transportShow();
		}));

		// TURNING NUMBERS BACK INTO NAMES. A patch file records a cable as a port number, and
		// what that port is called lives only in the running program. These write it down.
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Write a port index", "a few seconds", []() {
			const std::string msg = writePortIndex();
			INFO("DreamerDemo port index: %s", msg.c_str());
			osdialog_message(OSDIALOG_INFO, OSDIALOG_OK, msg.c_str());
		}));
		menu->addChild(createMenuItem("Write a patch report", "", []() {
			const std::string msg = writePatchReport();
			INFO("DreamerDemo patch report: %s", msg.c_str());
			osdialog_message(OSDIALOG_INFO, OSDIALOG_OK, msg.c_str());
		}));

		// GOING BACK TO A PATCH THAT IS GONE. Rack keeps one autosave and overwrites it, so a
		// crash or a mistake costs whatever was there. This is the list of everything the rack
		// has been, newest first.
		menu->addChild(createSubmenuItem("Restore a saved patch", "", [](Menu* sub) {
			const std::vector<HistoryEntry> entries = historyList(40);
			if (entries.empty()) {
				sub->addChild(createMenuLabel("nothing saved yet"));
				return;
			}
			for (const HistoryEntry& e : entries) {
				char right[32];
				std::snprintf(right, sizeof(right), "%d modules", e.modules);
				const std::string path = e.path;
				sub->addChild(createMenuItem(e.when, right, [path]() {
					// REPLACING THE RACK IS NOT UNDOABLE, so it is asked before it is done.
					const std::string q = "Replace the rack with the patch saved at "
						+ system::getFilename(path) + "?";
					if (!osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK_CANCEL, q.c_str()))
						return;
					try {
						APP->patch->load(path);
						APP->patch->path = "";
					}
					catch (Exception& e) {
						osdialog_message(OSDIALOG_ERROR, OSDIALOG_OK, e.what());
					}
				}));
			}
		}));
		menu->addChild(createMenuItem("Show the saved patches", "", []() {
			system::openDirectory(historyDir());
		}));

		DemoModule* demo = dynamic_cast<DemoModule*>(module);
		if (demo) {
			menu->addChild(createBoolPtrMenuItem<bool>(
				"Keep the patch report up to date", "every 15 s", &demo->autoReport));
		}
	}
};


} // namespace demo


Model* modelDemo = createModel<demo::DemoModule, demo::DemoWidget>("Demo");
