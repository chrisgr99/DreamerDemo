/** The module, which exists only to open the transport.

A plugin has to have a module for Rack to have anything to put in the browser, and a demo has to
be startable from somewhere. That is the whole of it: no parameters, no ports, no audio. The
module can sit on any page of any patch, and a demo run from it can be about anything.
*/
#include "plugin.hpp"

#include <GLFW/glfw3.h>

namespace demo {


struct DemoModule : Module {
	DemoModule() {
		config(0, 0, 0, 0);
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
	}
};


} // namespace demo


Model* modelDemo = createModel<demo::DemoModule, demo::DemoWidget>("Demo");
