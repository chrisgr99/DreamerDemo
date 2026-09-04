#include "Picker.hpp"
#include "Script.hpp"

#include <cstdio>

#include <GLFW/glfw3.h>
#include <sys/stat.h>

namespace demo {


static const float ROW = 30.f;
static const float PAD = 4.f;
static const float MAX_H = 340.f;
static const float TEXT = 15.f;


struct PickerList : widget::OpaqueWidget, OurWidget {
	std::vector<std::string> paths;
	std::string current;
	std::function<void(std::string)> pick;
	float scroll = 0.f;
	int hovered = -1;

	float contentHeight() const {
		return paths.size() * ROW + PAD * 2.f;
	}

	float maxScroll() const {
		return std::fmax(0.f, contentHeight() - box.size.y);
	}

	int rowAt(math::Vec pos) const {
		const int i = (int) ((pos.y - PAD + scroll) / ROW);
		return (i >= 0 && i < (int) paths.size()) ? i : -1;
	}

	/** Opened with the current script in view rather than at the top. A list that always starts
	at the beginning makes you hunt for the row that is already ticked. */
	void showCurrent() {
		for (size_t i = 0; i < paths.size(); i++) {
			if (paths[i] != current)
				continue;
			const float top = PAD + i * ROW;
			if (top < scroll || top + ROW > scroll + box.size.y)
				scroll = math::clamp(top - box.size.y / 2.f + ROW / 2.f, 0.f, maxScroll());
			return;
		}
	}

	void onHover(const HoverEvent& e) override {
		hovered = rowAt(e.pos);
		OpaqueWidget::onHover(e);
	}

	void onLeave(const LeaveEvent& e) override {
		hovered = -1;
		OpaqueWidget::onLeave(e);
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		scroll = math::clamp(scroll - e.scrollDelta.y, 0.f, maxScroll());
		e.consume(this);
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			const int i = rowAt(e.pos);
			if (i >= 0 && pick)
				pick(paths[i]);
			e.consume(this);
			e.stopPropagating();
			// The overlay above owns this widget and takes it away with itself.
			if (parent)
				parent->requestDelete();
			return;
		}
		OpaqueWidget::onButton(e);
	}

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 5.f);
		nvgFillColor(args.vg, nvgRGBA(0x14, 0x14, 0x18, 0xfa));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(0xcf, 0xcf, 0xcf, 0xc0));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		std::shared_ptr<window::Font> font = uiFont();
		if (!font || font->handle < 0)
			return;

		nvgSave(args.vg);
		nvgScissor(args.vg, 1.f, 1.f, box.size.x - 2.f, box.size.y - 2.f);
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, TEXT);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

		for (size_t i = 0; i < paths.size(); i++) {
			const float y = PAD + i * ROW - scroll;
			if (y + ROW < 0.f || y > box.size.y)
				continue;
			const bool isCurrent = (paths[i] == current);
			if ((int) i == hovered) {
				nvgBeginPath(args.vg);
				nvgRect(args.vg, 2.f, y, box.size.x - 4.f, ROW);
				nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x18));
				nvgFill(args.vg);
			}
			if (isCurrent) {
				// A bar down the left edge rather than a tick in a column: it reads at a glance
				// and costs no width.
				nvgBeginPath(args.vg);
				nvgRect(args.vg, 2.f, y + 3.f, 3.f, ROW - 6.f);
				nvgFillColor(args.vg, ACCENT);
				nvgFill(args.vg);
			}
			nvgFillColor(args.vg, isCurrent ? ACCENT : INK);
			nvgText(args.vg, 14.f, y + ROW / 2.f + 0.5f,
				system::getFilename(paths[i]).c_str(), NULL);
		}

		// A scrollbar, only when there is more than fits.
		if (maxScroll() > 0.f) {
			const float track = box.size.y - 8.f;
			const float thumb = std::fmax(24.f, track * box.size.y / contentHeight());
			const float at = 4.f + (track - thumb) * (scroll / maxScroll());
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, box.size.x - 7.f, at, 3.f, thumb, 1.5f);
			nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x60));
			nvgFill(args.vg);
		}

		nvgRestore(args.vg);
		OpaqueWidget::draw(args);
	}
};


/** A layer over the whole scene, so a press anywhere else closes the list. It is opaque, which
also stops that press reaching whatever was under it — clicking away from a list should dismiss
it and nothing more. */
struct PickerOverlay : widget::OpaqueWidget, OurWidget {
	PickerList* list = NULL;

	void step() override {
		box.pos = math::Vec(0.f, 0.f);
		box.size = APP->scene->box.size;

		// Escape closes it, asked of the keyboard rather than waited for, since the list may
		// never have been the selected widget.
		if (APP->window && APP->window->win
			&& glfwGetKey(APP->window->win, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			requestDelete();
			return;
		}
		OpaqueWidget::step();
	}

	void onButton(const ButtonEvent& e) override {
		OpaqueWidget::onButton(e);
		if (e.isConsumed())
			return;
		if (e.action == GLFW_PRESS) {
			requestDelete();
			e.consume(this);
		}
	}

	void draw(const DrawArgs& args) override {
		// Nothing of its own: the layer exists to catch a press, not to dim the rack.
		OpaqueWidget::draw(args);
	}
};


void pickerOpen(math::Rect field, const std::vector<std::string>& paths,
	const std::string& current, std::function<void(std::string)> pick) {
	if (paths.empty())
		return;

	PickerOverlay* overlay = new PickerOverlay;
	PickerList* list = new PickerList;
	list->paths = paths;
	list->current = current;
	list->pick = pick;

	const math::Vec scene = APP->scene->box.size;
	const float h = std::fmin(MAX_H, paths.size() * ROW + PAD * 2.f);
	list->box.size = math::Vec(std::fmax(field.size.x, 260.f), h);

	// Below the field, or above it when there is no room below — which is where the transport
	// usually is, since it prefers the bottom of the window.
	float y = field.pos.y + field.size.y + 4.f;
	if (y + h > scene.y - 8.f)
		y = std::fmax(8.f, field.pos.y - h - 4.f);
	list->box.pos = math::Vec(
		math::clamp(field.pos.x, 8.f, std::fmax(8.f, scene.x - list->box.size.x - 8.f)), y);

	list->showCurrent();
	overlay->list = list;
	overlay->addChild(list);
	APP->scene->addChild(overlay);
}


// ---------------------------------------------------------------- what is current

static std::string statePath() {
	const std::string dir = asset::user("DreamerDemo");
	if (!system::isDirectory(dir))
		system::createDirectories(dir);
	return dir + "/state.json";
}


static double fileTime(const std::string& path) {
	struct stat st;
	return (::stat(path.c_str(), &st) == 0) ? (double) st.st_mtime : 0.0;
}


void pickerRemember(const std::string& path) {
	json_t* rootJ = json_object();
	json_object_set_new(rootJ, "lastScript", json_string(path.c_str()));
	json_object_set_new(rootJ, "lastPlayed", json_real(system::getUnixTime()));
	json_dump_file(rootJ, statePath().c_str(), JSON_INDENT(1));
	json_decref(rootJ);
}


std::string pickerRemembered() {
	std::string remembered;
	double played = 0.0;

	json_error_t err;
	json_t* rootJ = json_load_file(statePath().c_str(), 0, &err);
	if (rootJ) {
		json_t* j = json_object_get(rootJ, "lastScript");
		if (j)
			remembered = json_string_value(j);
		j = json_object_get(rootJ, "lastPlayed");
		if (j)
			played = json_number_value(j);
		json_decref(rootJ);
	}
	if (!remembered.empty() && !system::isFile(remembered))
		remembered.clear();

	// The newest script on disk. scriptList() already returns them most recently edited first.
	const std::vector<std::string> all = scriptList();
	if (all.empty())
		return remembered;
	const std::string newest = all[0];

	// WHICHEVER HAPPENED LATER. Editing a script in a text editor makes it the one you are
	// working on; so does playing one. Neither should be able to override the other by being
	// the only thing that is remembered.
	if (remembered.empty())
		return newest;
	return (fileTime(newest) > played) ? newest : remembered;
}


} // namespace demo
