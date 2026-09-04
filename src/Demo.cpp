/** The module, which exists only to open the transport.

A plugin has to have a module for Rack to have anything to put in the browser, and a demo has to
be startable from somewhere. That is the whole of it: no parameters, no ports, no audio. The
module can sit on any page of any patch, and a demo run from it can be about anything.
*/
#include "plugin.hpp"
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
struct DemoPanel : widget::Widget {
	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillColor(args.vg, nvgRGB(0x17, 0x17, 0x1a));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x30));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		std::shared_ptr<window::Font> font = uiFont();
		if (!font || font->handle < 0)
			return;
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 13.f);
		nvgFillColor(args.vg, INK);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(args.vg, box.size.x / 2.f, 26.f, "DEMO", NULL);
		Widget::draw(args);
	}
};


struct OpenButton : widget::OpaqueWidget {
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			transportShow();
			e.consume(this);
			e.stopPropagating();
			return;
		}
		OpaqueWidget::onButton(e);
	}

	void draw(const DrawArgs& args) override {
		const bool hot = box.contains(APP->scene->getMousePos().minus(
			getAbsoluteOffset(math::Vec()).minus(box.pos)));
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 4.f);
		nvgFillColor(args.vg, hot ? nvgRGB(0x3a, 0x2c, 0x18) : nvgRGB(0x26, 0x26, 0x2b));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, ACCENT);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		std::shared_ptr<window::Font> font = uiFont();
		if (!font || font->handle < 0)
			return;
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 12.f);
		nvgFillColor(args.vg, INK);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f + 0.5f, "OPEN", NULL);
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
		box.size = math::Vec(4 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

		DemoPanel* panel = new DemoPanel;
		panel->box.size = box.size;
		addChild(panel);

		OpenButton* open = new OpenButton;
		open->box.size = math::Vec(box.size.x - 16.f, 26.f);
		open->box.pos = math::Vec(8.f, 46.f);
		addChild(open);

		addChild(createWidget<ScrewSilver>(math::Vec(0.f, 0.f)));
		addChild(createWidget<ScrewSilver>(
			math::Vec(box.size.x - RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}

	void appendContextMenu(Menu* menu) override {
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Open the transport", "", []() {
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
