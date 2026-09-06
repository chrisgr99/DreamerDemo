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
	// THE POINT THE POINTER WILL GO TO, not whether the whole thing fits. Demanding that the
	// entire target lie inside the window is right for a jack and nonsense for a window seven
	// hundred pixels wide that quite properly reaches towards an edge — which is how a chart
	// window plainly in view came to be reported as not on the screen.
	const math::Vec size = APP->scene->box.size;
	const math::Vec p = r.getCenter();
	const float m = 8.f;
	return p.x > m && p.y > m && p.x < size.x - m && p.y < size.y - m;
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


/** One half of a place on a panel.

TWO WAYS OF SAYING IT, because panels are built in two ways. "50%" is a fraction of the side,
which is right for something laid out proportionally. A plain number is pixels in from the near
edge and a negative number is pixels in from the far one, which is right for a control pinned a
fixed distance from an edge — Clarity's scope carries a row of those, and on a widget the user
can resize to any size at all, no fraction names them twice running.

Pixels are the widget's own, so a rack zoomed in or out does not move the target. */
static float placeAlong(const std::string& text, float extent, float zoom) {
	const float n = (float) std::atof(text.c_str());
	if (text.find('%') != std::string::npos)
		return extent * n / 100.f;
	return (n < 0.f) ? extent + n * zoom : n * zoom;
}


/** THE WIDGET CLIPPED TO A PORT, found by ASKING THE PLUGIN THAT MADE IT.

A clip-on widget is an ordinary rack child with no name, no port of its own, and nothing this
plugin can read to say which jack it belongs to. Guessing by distance is not good enough: two
outputs on the same module are thirty pixels apart and a widget sits sixty pixels from its own
jack, so the nearest port to a widget is regularly the wrong one.

But the plugin that owns them writes exactly what is needed into its own patch data — the module
and port each widget is attached to, and the offset from that port at which it is drawn — and a
module's JSON can be asked for at any time. The offset is the answer: a clip's position IS its
port's centre plus that offset, so the widget can be identified by where it is rather than by
what it is, without knowing one of its types.

This only works for widgets from a plugin that publishes them this way, which today means Test
Gear. That is honest: a demo cannot point at another plugin's floating furniture unless that
plugin says where it put it. */
/** WHAT KIND OF WIDGET AN ENTRY IS, in the words a script uses. Two widgets can sit on the same
port — a switch and an attenuverter on the same input is an ordinary thing to want — so a script
must be able to say which of them it means. The list an entry is in names most of them; the
injectors share one list and are told apart by their type, in the order of the enum they were
declared in. */
static std::string clipKind(const char* list, json_t* eJ) {
	static const char* INJECTORS[] = {"gate", "pulse", "dc", "lfo", "vco", "note", "av",
		"noise", "clock", "switch"};
	if (std::string(list) != "injectors") {
		std::string k = list;
		return k.substr(0, k.size() - 1);   // scopes -> scope, meters -> meter
	}
	json_t* tJ = json_object_get(eJ, "type");
	const int type = tJ ? (int) json_integer_value(tJ) : -1;
	if (type < 0 || type >= (int) (sizeof(INJECTORS) / sizeof(INJECTORS[0])))
		return "injector";
	return INJECTORS[type];
}


static bool clipKindMatches(const std::string& want, const std::string& kind) {
	if (want.empty() || want == kind)
		return true;
	// The words a person would use for the same thing.
	if (want == "voltmeter" && kind == "meter")
		return true;
	if (want == "frequency" && kind == "freq")
		return true;
	if (want == "attenuverter" && kind == "av")
		return true;
	return false;
}


static bool clipOffsetFor(json_t* dataJ, int64_t moduleId, int portId, bool isOutput,
	const std::string& kind, math::Vec* out, float* faceHeight) {
	static const char* LISTS[] = {"scopes", "analysers", "monitors", "meters", "freqs",
		"injectors"};
	for (size_t l = 0; l < sizeof(LISTS) / sizeof(LISTS[0]); l++) {
		json_t* arrayJ = json_object_get(dataJ, LISTS[l]);
		if (!arrayJ || !json_is_array(arrayJ))
			continue;
		size_t i;
		json_t* eJ;
		json_array_foreach(arrayJ, i, eJ) {
			json_t* mJ = json_object_get(eJ, "moduleId");
			json_t* pJ = json_object_get(eJ, "portId");
			if (!mJ || !pJ)
				continue;
			if (json_integer_value(mJ) != moduleId || json_integer_value(pJ) != portId)
				continue;
			if (!clipKindMatches(kind, clipKind(LISTS[l], eJ)))
				continue;
			// The injectors carry no side: they only ever go on inputs.
			json_t* oJ = json_object_get(eJ, "isOutput");
			if (oJ && json_boolean_value(oJ) != isOutput)
				continue;
			math::Vec off;
			if (json_t* j = json_object_get(eJ, "offsetX"))
				off.x = json_number_value(j);
			if (json_t* j = json_object_get(eJ, "offsetY"))
				off.y = json_number_value(j);
			*out = off;
			// A scope says how tall its own face is, which is not the same as how tall the
			// widget is: a readout opens underneath it and makes the widget taller. Anything
			// pinned to the bottom of the FACE — which is the whole row of buttons — can only
			// be addressed if that height is known.
			if (faceHeight) {
				json_t* fJ = json_object_get(eJ, "faceHeight");
				*faceHeight = fJ ? (float) json_number_value(fJ) : 0.f;
			}
			return true;
		}
	}
	return false;
}


widget::Widget* clipOnPort(app::PortWidget* pw, const std::string& kind, float* faceHeight) {
	if (!pw || !pw->module)
		return NULL;
	const bool isOutput = (pw->type == engine::Port::OUTPUT);

	math::Vec offset;
	bool have = false;
	for (app::ModuleWidget* mw : APP->scene->rack->getModules()) {
		if (!mw->module || !mw->model || !mw->model->plugin)
			continue;
		if (mw->model->plugin->slug != "DreamerDevelopment" || mw->model->slug != "TestGear")
			continue;
		json_t* dataJ = mw->module->dataToJson();
		if (!dataJ)
			continue;
		have = clipOffsetFor(dataJ, pw->module->id, pw->portId, isOutput, kind, &offset,
			faceHeight);
		json_decref(dataJ);
		if (have)
			break;
	}
	if (!have)
		return NULL;

	// Where the widget must therefore be, in rack coordinates, and the rack child that is
	// actually there. Within a pixel: the position is arithmetic, not an estimate.
	const math::Vec want = pw->getRelativeOffset(pw->box.zeroPos().getCenter(),
		APP->scene->rack).plus(offset);
	for (widget::Widget* child : APP->scene->rack->children) {
		if (dynamic_cast<app::ModuleWidget*>(child) || dynamic_cast<app::CableWidget*>(child))
			continue;
		if (child->box.pos.minus(want).norm() < 1.5f)
			return child;
	}
	return NULL;
}


/** The close cross belonging to a clip: a small disc of its own, centred on the clip's top left
corner. It is a separate child of the rack rather than part of the widget, so it is found the
same way — by where it has to be. */
widget::Widget* clipCloseOf(widget::Widget* clip) {
	if (!clip)
		return NULL;
	for (widget::Widget* child : APP->scene->rack->children) {
		if (child == clip || child->box.size.x > 16.f || child->box.size.y > 16.f)
			continue;
		if (child->box.getCenter().minus(clip->box.pos).norm() < 2.f)
			return child;
	}
	return NULL;
}


/** The words that may follow a slash in a widget's address: the kinds a widget can be, and its
close cross. Everything else after a slash is part of a port's name — "1V/Oct" is a jack, not an
octave-flavoured widget. Several may be given at once, separated by commas. */
static bool isClipPart(const std::string& text) {
	static const char* PARTS[] = {"close", "face", "scope", "analyser", "analyzer", "monitor", "meter",
		"voltmeter", "freq", "frequency", "injector", "gate", "pulse", "dc", "lfo", "vco",
		"note", "av", "attenuverter", "noise", "clock", "switch"};
	if (text.empty())
		return false;
	std::string tok;
	std::string rest = text + ",";
	for (size_t i = 0; i < rest.size(); i++) {
		if (rest[i] != ',') {
			tok += rest[i];
			continue;
		}
		bool known = false;
		for (size_t k = 0; k < sizeof(PARTS) / sizeof(PARTS[0]); k++)
			known = known || (lower(tok) == PARTS[k]);
		if (!known)
			return false;
		tok.clear();
	}
	return true;
}


/** One group of names, matched either exactly or by containment. The caller runs the exact pass
over every group before letting any group answer loosely — a module with both "Gate" and "Gate
length" must answer "Gate" with the first, and a jack named exactly what was asked for must beat
a knob that merely contains it. */
static int matchName(const std::vector<std::string>& names, const std::string& want,
	bool exact) {
	const std::string w = lower(want);
	for (size_t i = 0; i < names.size(); i++) {
		const std::string n = lower(names[i]);
		if (exact ? (n == w) : (n.find(w) != std::string::npos))
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

	// A PLACE ON A PANEL, for what a module draws itself. A readout is not a parameter and not a
	// port, so nothing about it can be addressed by name — yet the title in mpxChart's readout
	// is the way to another song, and a demo that cannot press it cannot show that. So a target
	// may name a point on a panel, either as fractions of its width and height or as pixels in
	// from an edge:
	//
	//     chart@50%,12%      halfway across, a little down
	//     widget@30,9        thirty pixels in from the left, nine down from the top
	//     widget@-14,9       fourteen pixels in from the RIGHT
	//
	// It is a last resort, and reads like one: it is a guess about a layout that could change.
	// Where a control has a name, use the name.
	const size_t at = ref.find('@');
	if (at != std::string::npos) {
		const std::string who = ref.substr(0, at);
		const std::string where = ref.substr(at + 1);
		const size_t comma = where.find(',');
		if (comma == std::string::npos) {
			t.why = "a place on a panel is written name@across,down";
			return t;
		}
		const Target on = find(who);
		if (!on.ok)
			return on;
		const float zoom = on.widget ? on.widget->getAbsoluteZoom() : 1.f;
		t = on;
		// THE THING THE POINT IS ON IS THE MODULE, not the control the offset was measured from.
		// A place is deliberately somewhere other than its anchor, so whatever happens to be at
		// that point — another jack, a knob, bare panel — is what a click there will land on,
		// and a check that insisted on the anchor itself refused every offset that reached past
		// its own neighbour. The panel is still the right thing to insist on: a window over the
		// rack is not it.
		if (on.mw)
			t.widget = on.mw;
		const math::Vec p = on.rect.pos.plus(math::Vec(
			placeAlong(where.substr(0, comma), on.rect.size.x, zoom),
			placeAlong(where.substr(comma + 1), on.rect.size.y, zoom)));
		t.rect = math::Rect(p.minus(math::Vec(6.f, 6.f)), math::Vec(12.f, 12.f));
		// The widget is the module, so a click landing anywhere on that module counts as
		// landing on the target: the thing being pressed is whatever the module draws there.
		t.paramId = -1;
		t.portId = -1;
		t.ok = true;
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

	// A CLIP-ON WIDGET. It is not a module, so it has no name and none of its controls can be
	// addressed — but two things can be said about one, and between them they cover every demo:
	//
	//     widget                  the newest, which during a demo is the one just made
	//     widget:vco:out:#1       the one clipped to that port, in a rack that already has some
	//     widget:vco:out:#1/close its close cross, which is how a person takes one off
	//
	// The second matters more than it looks. A demo that has to build every widget it talks
	// about spends its length on one gesture repeated, and every addition is another chance to
	// place something badly on camera. A rack that already carries them lets the demo say what
	// they TELL you, which is the part worth watching.
	if (name == "widget") {
		// THE SLASH IS ONLY A SEPARATOR WHEN WHAT FOLLOWS IT IS A PART. Port names contain
		// slashes — tòna's pitch input is called "1V/Oct" — so a trailing word is taken as a
		// kind or a close cross only when it is one of the words this understands. Anything
		// else belongs to the name.
		std::string part;
		std::string portRef = control;
		const size_t slash = portRef.rfind('/');
		if (slash != std::string::npos && isClipPart(portRef.substr(slash + 1))) {
			part = portRef.substr(slash + 1);
			portRef = portRef.substr(0, slash);
		}

		// The part is a kind ("switch"), the close cross ("close"), or both ("switch,close").
		std::string kind, wantClose, wantFace;
		{
			std::string tok;
			std::string rest = part + ",";
			for (size_t k = 0; k < rest.size(); k++) {
				if (rest[k] != ',') {
					tok += rest[k];
					continue;
				}
				if (tok == "close")
					wantClose = tok;
				else if (tok == "face")
					wantFace = tok;
				else if (!tok.empty())
					kind = tok;
				tok.clear();
			}
		}

		widget::Widget* w = NULL;
		float face = 0.f;
		if (portRef.empty()) {
			w = frontRackWidget();
			if (!w) {
				t.why = "nothing is clipped onto the rack";
				return t;
			}
		}
		else {
			const Target on = find(portRef);
			if (!on.ok)
				return on;
			if (on.portId < 0) {
				t.why = "\"" + portRef + "\" is not a port, so nothing can be clipped to it";
				return t;
			}
			w = clipOnPort((app::PortWidget*) on.widget, kind, &face);
			if (!w) {
				t.why = kind.empty()
					? "no widget is clipped to \"" + portRef + "\""
					: "no " + kind + " is clipped to \"" + portRef + "\"";
				return t;
			}
		}

		if (!wantClose.empty()) {
			widget::Widget* x = clipCloseOf(w);
			if (!x) {
				t.why = "that widget has no close cross of its own";
				return t;
			}
			w = x;
		}

		t.widget = w;
		t.rect = sceneRect(w);
		// THE FACE, not the whole widget. A scope's readout hangs below its face and changes
		// the widget's height, so an offset measured from the bottom of the widget lands
		// somewhere different depending on whether the readout is open. Naming the face gives
		// the row of buttons a bottom edge that does not move.
		if (!wantFace.empty() && face > 0.f)
			t.rect.size.y = face * w->getAbsoluteZoom();
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

	std::vector<app::ParamWidget*> params = mw->getParams();
	std::vector<app::PortWidget*> outs = mw->getOutputs();
	std::vector<app::PortWidget*> ins = mw->getInputs();

	std::vector<std::string> paramNames, outNames, inNames;
	for (app::ParamWidget* p : params) {
		engine::ParamQuantity* pq = p->getParamQuantity();
		paramNames.push_back(pq ? pq->name : std::string());
	}
	for (app::PortWidget* p : outs) {
		engine::PortInfo* info = p->getPortInfo();
		outNames.push_back(info ? info->name : std::string());
	}
	for (app::PortWidget* p : ins) {
		engine::PortInfo* info = p->getPortInfo();
		inNames.push_back(info ? info->name : std::string());
	}

	if (index >= 0) {
		// By number, within whichever group the prefix named — parameters when it named none.
		if (!wantIn && !wantOut && index < (int) params.size()) {
			t.widget = params[index];
			t.paramId = params[index]->paramId;
		}
		else if (wantOut && index < (int) outs.size()) {
			t.widget = outs[index];
			t.portId = outs[index]->portId;
			t.portType = engine::Port::OUTPUT;
		}
		else if (wantIn && index < (int) ins.size()) {
			t.widget = ins[index];
			t.portId = ins[index]->portId;
			t.portType = engine::Port::INPUT;
		}
	}
	else {
		// EXACT ACROSS ALL THREE GROUPS BEFORE ANY LOOSE MATCH ANYWHERE. Looking through the
		// parameters completely and only then at the ports meant a jack named exactly what was
		// asked for lost to a knob that merely contained it: "FM" on an oscillator with an
		// input called FM found "Linear FM CV Attenuator" instead, and a step that wanted to
		// clip something onto a jack was handed a knob.
		for (int pass = 0; pass < 2 && !t.widget; pass++) {
			const bool exact = (pass == 0);
			int k = -1;
			if (!wantIn && !wantOut
				&& (k = matchName(paramNames, control, exact)) >= 0) {
				t.widget = params[k];
				t.paramId = params[k]->paramId;
			}
			else if (!wantIn && (k = matchName(outNames, control, exact)) >= 0) {
				t.widget = outs[k];
				t.portId = outs[k]->portId;
				t.portType = engine::Port::OUTPUT;
			}
			else if (!wantOut && (k = matchName(inNames, control, exact)) >= 0) {
				t.widget = ins[k];
				t.portId = ins[k]->portId;
				t.portType = engine::Port::INPUT;
			}
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
