#include "Gesture.hpp"
#include "Theatre.hpp"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <typeinfo>
#include <chrono>
#include <thread>

namespace demo {


/** The overlay swallows real mouse movement while a demo runs, so that the viewer's own mouse
cannot light a control the synthetic pointer is nowhere near. An INJECTED event has to get past
that, so it says so on the way through. */
struct Injecting {
	Injecting() { theatre()->injecting = true; }
	~Injecting() { theatre()->injecting = false; }
};


void gHover(math::Vec pos, math::Vec delta) {
	Injecting guard;
	APP->event->handleHover(pos, delta);
}


void gPress(math::Vec pos, int button) {
	Injecting guard;
	APP->event->handleButton(pos, button, GLFW_PRESS, 0);
}


void gRelease(math::Vec pos, int button) {
	Injecting guard;
	APP->event->handleButton(pos, button, GLFW_RELEASE, 0);
}


void gClick(math::Vec pos, int button) {
	// HOVERED FIRST. Rack sends a button event to the widget under the pointer, and what it
	// believes is under the pointer is whatever it last hovered. Clicking without moving there
	// first delivers the press to wherever the real mouse was left.
	gHover(pos, math::Vec(0.f, 0.f));
	gPress(pos, button);
	gRelease(pos, button);
}


void gClickHeld(math::Vec pos, int button) {
	gHover(pos, math::Vec(0.f, 0.f));
	gPress(pos, button);
	// Long enough for the engine and every module widget to see the button down at least once.
	// This runs only when an author is stepping through a script, never during a take.
	std::this_thread::sleep_for(std::chrono::milliseconds(40));
	gRelease(pos, button);
}


std::string gHoveredName() {
	widget::Widget* w = APP->event->getHoveredWidget();
	if (!w)
		return "nothing";
	std::string out;
	for (int depth = 0; w && depth < 4; w = w->parent, depth++) {
		char buf[160];
		std::snprintf(buf, sizeof(buf), "%s%s(%g,%g %gx%g)", out.empty() ? "" : " in ",
			typeid(*w).name(), w->box.pos.x, w->box.pos.y, w->box.size.x, w->box.size.y);
		out += buf;
	}
	return out;
}


bool gHoveredIs(widget::Widget* want) {
	if (!want)
		return true;
	for (widget::Widget* w = APP->event->getHoveredWidget(); w; w = w->parent) {
		if (w == want)
			return true;
	}
	return false;
}


void gScroll(math::Vec pos, math::Vec delta) {
	Injecting guard;
	APP->event->handleHover(pos, math::Vec(0.f, 0.f));
	APP->event->handleScroll(pos, delta);
}


void gKey(math::Vec pos, int key) {
	Injecting guard;
	APP->event->handleHover(pos, math::Vec(0.f, 0.f));
	APP->event->handleKey(pos, key, 0, GLFW_PRESS, 0);
	APP->event->handleKey(pos, key, 0, GLFW_RELEASE, 0);
}


void gSetParam(const Target& t, float unit) {
	if (!t.module || t.paramId < 0)
		return;
	engine::ParamQuantity* pq = t.module->paramQuantities[t.paramId];
	if (!pq)
		return;
	const float lo = pq->getMinValue();
	const float hi = pq->getMaxValue();
	pq->setValue(lo + math::clamp(unit, 0.f, 1.f) * (hi - lo));
}


float gParamUnit(const Target& t) {
	if (!t.module || t.paramId < 0)
		return 0.f;
	engine::ParamQuantity* pq = t.module->paramQuantities[t.paramId];
	if (!pq)
		return 0.f;
	const float lo = pq->getMinValue();
	const float hi = pq->getMaxValue();
	if (hi == lo)
		return 0.f;
	return (pq->getValue() - lo) / (hi - lo);
}


bool gCableExists(engine::Module* outModule, int outId,
	engine::Module* inModule, int inId) {
	for (int64_t id : APP->engine->getCableIds()) {
		engine::Cable* c = APP->engine->getCable(id);
		if (!c)
			continue;
		if (c->outputModule == outModule && c->outputId == outId
			&& c->inputModule == inModule && c->inputId == inId)
			return true;
	}
	return false;
}


int gCableCount(const Target& port) {
	if (!port.module || port.portId < 0)
		return 0;
	int n = 0;
	for (int64_t id : APP->engine->getCableIds()) {
		engine::Cable* c = APP->engine->getCable(id);
		if (!c)
			continue;
		if (port.portType == engine::Port::OUTPUT) {
			if (c->outputModule == port.module && c->outputId == port.portId)
				n++;
		}
		else if (c->inputModule == port.module && c->inputId == port.portId) {
			n++;
		}
	}
	return n;
}


} // namespace demo
