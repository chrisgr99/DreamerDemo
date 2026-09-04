#include "Resolve.hpp"

#include <algorithm>

namespace demo {


static std::string lower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return (char) std::tolower(c); });
	return s;
}


math::Rect sceneRect(widget::Widget* w) {
	if (!w)
		return math::Rect();
	const float zoom = w->getAbsoluteZoom();
	return math::Rect(w->getAbsoluteOffset(math::Vec(0.f, 0.f)), w->box.size.mult(zoom));
}


bool onScreen(math::Rect r) {
	const math::Vec size = APP->scene->box.size;
	const float m = 8.f;
	return r.pos.x > m && r.pos.y > m
		&& r.pos.x + r.size.x < size.x - m
		&& r.pos.y + r.size.y < size.y - m;
}


bool Stage::bindModel(const std::string& name, const std::string& modelRef) {
	const size_t slash = modelRef.find('/');
	if (slash == std::string::npos)
		return false;
	const std::string pluginSlug = modelRef.substr(0, slash);
	const std::string modelSlug = modelRef.substr(slash + 1);

	for (app::ModuleWidget* mw : APP->scene->rack->getModules()) {
		if (!mw->module || !mw->module->model || !mw->module->model->plugin)
			continue;
		if (mw->module->model->plugin->slug == pluginSlug
			&& mw->module->model->slug == modelSlug) {
			ids[name] = mw->module->id;
			return true;
		}
	}
	return false;
}


/** Exact first, then any name containing it. Two passes rather than one, because a module with
both "Gate" and "Gate length" would otherwise answer the first with the second. */
static int matchName(const std::vector<std::string>& names, const std::string& want) {
	const std::string w = lower(want);
	for (size_t i = 0; i < names.size(); i++) {
		if (lower(names[i]) == w)
			return (int) i;
	}
	for (size_t i = 0; i < names.size(); i++) {
		if (lower(names[i]).find(w) != std::string::npos)
			return (int) i;
	}
	return -1;
}


Target Stage::find(const std::string& ref) const {
	Target t;
	if (ref.empty()) {
		t.why = "no target";
		return t;
	}

	const size_t colon = ref.find(':');
	const std::string name = ref.substr(0, colon);
	std::string control = (colon == std::string::npos) ? "" : ref.substr(colon + 1);

	// THE FLOATING WINDOW AT THE FRONT, which no name can be bound to because it is not a
	// module. A window a module opened has no controls a script could address, and the realistic
	// way to shut one is its own close cross rather than pressing the button that opened it a
	// second time — so the corners where a close control lives are addressable.
	if (name == "window") {
		widget::Widget* w = frontWindow();
		if (!w) {
			t.why = "no window is open; the scene holds " + sceneContents();
			return t;
		}
		t.widget = w;
		t.rect = sceneRect(w);
		const float corner = 22.f;
		if (control == "close" || control == "close-left") {
			t.rect = math::Rect(t.rect.pos, math::Vec(corner, corner));
		}
		else if (control == "close-right") {
			t.rect = math::Rect(
				math::Vec(t.rect.pos.x + t.rect.size.x - corner, t.rect.pos.y),
				math::Vec(corner, corner));
		}
		else if (!control.empty()) {
			t.why = "a window has no \"" + control + "\"; try close, or close-right";
			return t;
		}
		t.ok = true;
		return t;
	}

	std::map<std::string, int64_t>::const_iterator it = ids.find(name);
	if (it == ids.end()) {
		t.why = "no module bound to the name \"" + name + "\"";
		return t;
	}
	app::ModuleWidget* mw = APP->scene->rack->getModule(it->second);
	if (!mw) {
		t.why = "the module \"" + name + "\" is no longer on the rack";
		return t;
	}
	t.mw = mw;
	t.module = mw->module;

	if (control.empty()) {
		// The module itself: its panel, which is what a `move` step drags and what a `zoom` step
		// frames.
		t.widget = mw;
		t.rect = sceneRect(mw);
		t.ok = true;
		return t;
	}

	bool wantIn = false, wantOut = false;
	if (control.compare(0, 3, "in:") == 0) {
		wantIn = true;
		control = control.substr(3);
	}
	else if (control.compare(0, 4, "out:") == 0) {
		wantOut = true;
		control = control.substr(4);
	}

	// A number, for a module whose controls are unnamed.
	int index = -1;
	if (control.size() > 1 && control[0] == '#')
		index = std::atoi(control.c_str() + 1);

	if (!wantIn && !wantOut) {
		std::vector<app::ParamWidget*> params = mw->getParams();
		if (index >= 0 && index < (int) params.size()) {
			t.widget = params[index];
			t.paramId = params[index]->paramId;
		}
		else if (index < 0) {
			std::vector<std::string> names;
			for (app::ParamWidget* p : params) {
				engine::ParamQuantity* pq = p->getParamQuantity();
				names.push_back(pq ? pq->name : std::string());
			}
			const int k = matchName(names, control);
			if (k >= 0) {
				t.widget = params[k];
				t.paramId = params[k]->paramId;
			}
		}
	}

	if (!t.widget && !wantIn) {
		std::vector<app::PortWidget*> outs = mw->getOutputs();
		std::vector<std::string> names;
		for (app::PortWidget* p : outs) {
			engine::PortInfo* info = p->getPortInfo();
			names.push_back(info ? info->name : std::string());
		}
		const int k = (index >= 0 && index < (int) outs.size()) ? index
			: (index < 0 ? matchName(names, control) : -1);
		if (k >= 0) {
			t.widget = outs[k];
			t.portId = outs[k]->portId;
			t.portType = engine::Port::OUTPUT;
		}
	}

	if (!t.widget && !wantOut) {
		std::vector<app::PortWidget*> ins = mw->getInputs();
		std::vector<std::string> names;
		for (app::PortWidget* p : ins) {
			engine::PortInfo* info = p->getPortInfo();
			names.push_back(info ? info->name : std::string());
		}
		const int k = (index >= 0 && index < (int) ins.size()) ? index
			: (index < 0 ? matchName(names, control) : -1);
		if (k >= 0) {
			t.widget = ins[k];
			t.portId = ins[k]->portId;
			t.portType = engine::Port::INPUT;
		}
	}

	if (!t.widget) {
		t.why = "\"" + name + "\" has no control called \"" + control + "\"";
		return t;
	}

	t.rect = sceneRect(t.widget);
	t.ok = true;
	return t;
}


} // namespace demo
