#pragma once
/** Names to widgets to places on the screen.

A script addresses controls LOGICALLY — "chart:Tempo", "quant:Chord" — and they are turned into
screen coordinates at the last moment, every time. That is what lets one script survive any
window size, any zoom, any scroll, and a rack rearranged between takes. Nothing a script says
depends on where anything happens to be.

A CONTROL IS FOUND BY ITS NAME, not by its number. A parameter's name is the label its module
gave it with configParam, and a port's is what configInput or configOutput gave it; both are what
Rack itself shows in a tooltip. So a step says what the author would say out loud, and a module
that renumbers its parameters does not silently break a script.
*/
#include "plugin.hpp"

#include <map>
#include <string>

namespace demo {


/** One control, found. */
struct Target {
	bool ok = false;
	/** Why not, when it was not found. Shown to the author rather than swallowed. */
	std::string why;

	widget::Widget* widget = NULL;      /**< the ParamWidget, PortWidget or ModuleWidget */
	app::ModuleWidget* mw = NULL;
	engine::Module* module = NULL;
	int paramId = -1;
	int portId = -1;
	engine::Port::Type portType = engine::Port::INPUT;

	/** Where it is, in scene coordinates, at the moment it was resolved. */
	math::Rect rect;

	math::Vec centre() const { return rect.pos.plus(rect.size.div(2.f)); }
};


/** The names a script has bound to the modules it addresses. */
struct Stage {
	std::map<std::string, int64_t> ids;

	void clear() { ids.clear(); }

	/** Bind a name to a module already on the rack, by its model: "DreamerMPX/mpxChart". The
	FIRST such module, in the order Rack holds them. A script that needs two of a kind binds the
	second when it adds it. */
	bool bindModel(const std::string& name, const std::string& modelRef);

	void bindId(const std::string& name, int64_t moduleId) { ids[name] = moduleId; }

	/** Resolve "name" (the module itself) or "name:control".

	A control is looked for among the parameters first, then the outputs, then the inputs, by
	exact name and then by any name containing it, ignoring case. `in:` and `out:` in front of
	the control force the side, for the rare module that names an input and an output the same
	thing; `#3` addresses a control by number, for one that names nothing at all. */
	Target find(const std::string& ref) const;
};


/** Where a widget is on the screen: its own box carried up through every ancestor, with the
rack's zoom applied. Rack's ZoomWidget does the arithmetic; this only names it. */
math::Rect sceneRect(widget::Widget* w);

/** Whether the point a click would go to is inside the window. A control scrolled off the rack
has a position, and it is a lie — pointing at it would send the pointer off the edge of the
picture and act on whatever happened to be there.

THE POINT, not the whole rectangle: a window is allowed to reach towards an edge. */
bool onScreen(math::Rect r);


} // namespace demo
