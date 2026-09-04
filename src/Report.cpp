#include "Report.hpp"

#include <patch.hpp>

#include <cstdio>
#include <ctime>
#include <algorithm>
#include <set>

namespace demo {


static std::string outDir() {
	const std::string dir = asset::user("DreamerDemo");
	if (!system::isDirectory(dir))
		system::createDirectories(dir);
	return dir;
}


/** One line of JSON per model, rather than one JSON document.

WRITTEN AND FLUSHED AS IT GOES, because this builds one of every module of every installed
plugin and any one of them may be the module that does something rash in its constructor. A
document would be lost whole; lines survive up to the offender, and the last line names it. */
std::string writePortIndex() {
	const std::string path = outDir() + "/ports.jsonl";
	FILE* f = std::fopen(path.c_str(), "w");
	if (!f)
		return "could not write " + path;

	int models = 0, built = 0, failed = 0;

	for (plugin::Plugin* p : plugin::plugins) {
		if (!p)
			continue;

		// THE PLUGIN'S OWN LINE, BEFORE ANY OF ITS MODULES. Whether a plugin publishes its
		// source decides whether a question about how one of its modules behaves can be answered
		// by reading the code or only by patching and listening — so it belongs in the same
		// file. It is written before anything is built, so it survives a module that does not.
		json_t* pluginJ = json_object();
		json_object_set_new(pluginJ, "plugin", json_string(p->slug.c_str()));
		json_object_set_new(pluginJ, "pluginName", json_string(p->name.c_str()));
		json_object_set_new(pluginJ, "version", json_string(p->version.c_str()));
		json_object_set_new(pluginJ, "license", json_string(p->license.c_str()));
		json_object_set_new(pluginJ, "source", json_string(p->sourceUrl.c_str()));
		json_object_set_new(pluginJ, "manual", json_string(p->manualUrl.c_str()));
		json_object_set_new(pluginJ, "models", json_integer((json_int_t) p->models.size()));
		char* pluginS = json_dumps(pluginJ, JSON_COMPACT);
		if (pluginS) {
			std::fprintf(f, "%s\n", pluginS);
			std::free(pluginS);
		}
		json_decref(pluginJ);
		std::fflush(f);

		for (plugin::Model* model : p->models) {
			if (!model)
				continue;
			models++;

			// The name of the thing about to be built goes down FIRST, so a module that takes
			// the process with it is named by the last line in the file.
			std::fprintf(f, "{\"plugin\":\"%s\",\"model\":\"%s\",\"name\":",
				p->slug.c_str(), model->slug.c_str());
			json_t* nameJ = json_string(model->name.c_str());
			char* nameS = json_dumps(nameJ, JSON_ENCODE_ANY | JSON_COMPACT);
			std::fprintf(f, "%s", nameS ? nameS : "\"\"");
			std::free(nameS);
			json_decref(nameJ);
			std::fflush(f);

			engine::Module* module = NULL;
			try {
				module = model->createModule();
			}
			catch (...) {
				module = NULL;
			}
			if (!module) {
				std::fprintf(f, ",\"built\":false}\n");
				std::fflush(f);
				failed++;
				continue;
			}
			built++;

			// Params, inputs, outputs and lights, each as an array indexed by the number a
			// patch file records. A hole in the array is a control the module did not name.
			json_t* rootJ = json_object();
			const char* keys[4] = {"params", "inputs", "outputs", "lights"};
			for (int kind = 0; kind < 4; kind++) {
				json_t* arrJ = json_array();
				const size_t n = (kind == 0) ? module->paramQuantities.size()
					: (kind == 1) ? module->inputInfos.size()
					: (kind == 2) ? module->outputInfos.size()
					: module->lightInfos.size();
				for (size_t i = 0; i < n; i++) {
					std::string name, unit;
					if (kind == 0) {
						engine::ParamQuantity* q = module->paramQuantities[i];
						if (q) {
							name = q->name;
							unit = q->unit;
						}
					}
					else if (kind == 1) {
						engine::PortInfo* info = module->inputInfos[i];
						if (info)
							name = info->name;
					}
					else if (kind == 2) {
						engine::PortInfo* info = module->outputInfos[i];
						if (info)
							name = info->name;
					}
					else {
						engine::LightInfo* info = module->lightInfos[i];
						if (info)
							name = info->name;
					}
					if (unit.empty())
						json_array_append_new(arrJ, json_string(name.c_str()));
					else
						json_array_append_new(arrJ, json_string((name + unit).c_str()));
				}
				json_object_set_new(rootJ, keys[kind], arrJ);
			}

			char* body = json_dumps(rootJ, JSON_COMPACT);
			if (body) {
				// The object is spliced into the line already begun, so the whole model is one
				// line: {"plugin":…,"model":…,"name":…,"params":[…],…}
				std::fprintf(f, ",%s\n", body + 1);
				std::free(body);
			}
			else {
				std::fprintf(f, ",\"built\":true}\n");
			}
			json_decref(rootJ);
			std::fflush(f);

			delete module;
		}
	}

	std::fclose(f);

	char msg[512];
	std::snprintf(msg, sizeof(msg),
		"%d models from %d plugins, %d built, %d would not build\n%s",
		models, (int) plugin::plugins.size(), built, failed, path.c_str());
	return msg;
}


static std::string portName(engine::Module* module, engine::Port::Type type, int id) {
	if (!module || id < 0)
		return "?";
	const std::vector<engine::PortInfo*>& infos =
		(type == engine::Port::OUTPUT) ? module->outputInfos : module->inputInfos;
	if (id >= (int) infos.size() || !infos[id])
		return "#" + std::to_string(id);
	const std::string name = infos[id]->name;
	return name.empty() ? ("#" + std::to_string(id)) : name;
}


static std::string moduleName(app::ModuleWidget* mw) {
	if (!mw || !mw->module || !mw->module->model)
		return "?";
	plugin::Model* m = mw->module->model;
	return m->plugin->slug + "/" + m->slug;
}


/** Built as text rather than written straight out, so it can be compared with what is already on
disk and only written when it differs. */
std::string patchReportText() {
	std::string out;
	char line[1024];

	std::vector<app::ModuleWidget*> mws = APP->scene->rack->getModules();

	// EVERY PATCHED INPUT, GATHERED ONCE. Asking the engine about every cable for every input of
	// every module is the same question a few hundred thousand times on a large patch, and this
	// runs every fifteen seconds.
	std::set<std::pair<int64_t, int> > patched;
	for (int64_t cid : APP->engine->getCableIds()) {
		engine::Cable* c = APP->engine->getCable(cid);
		if (c && c->inputModule)
			patched.insert(std::make_pair(c->inputModule->id, c->inputId));
	}

	std::snprintf(line, sizeof(line), "The rack, %d modules\n\n", (int) mws.size());
	out += line;

	for (app::ModuleWidget* mw : mws) {
		if (!mw || !mw->module)
			continue;
		engine::Module* module = mw->module;
		std::snprintf(line, sizeof(line), "%s  id %lld  at %d HP, row %d\n",
			moduleName(mw).c_str(), (long long) module->id,
			(int) std::round(mw->box.pos.x / RACK_GRID_WIDTH),
			(int) std::round(mw->box.pos.y / RACK_GRID_HEIGHT));
		out += line;

		// Settings as a person reads them: the name the module gave the control and the value
		// in its own units, not the raw number a patch file holds.
		for (size_t i = 0; i < module->paramQuantities.size(); i++) {
			engine::ParamQuantity* q = module->paramQuantities[i];
			if (!q)
				continue;
			std::snprintf(line, sizeof(line), "    param %2d  %-28s %s\n", (int) i,
				q->name.c_str(), q->getDisplayValueString().c_str());
			out += line;
		}

		// WHICH INPUTS HAVE NOTHING ON THEM. This is the line that finds a patch that makes no
		// sound: a module can be perfectly wired except for the one jack that tells it there is
		// anything to do.
		std::string bare;
		for (size_t i = 0; i < module->inputs.size(); i++) {
			if (patched.count(std::make_pair(module->id, (int) i)))
				continue;
			bare += (bare.empty() ? "" : ", ")
				+ portName(module, engine::Port::INPUT, (int) i);
		}
		if (!bare.empty()) {
			std::snprintf(line, sizeof(line), "    nothing patched into: %s\n", bare.c_str());
			out += line;
		}
		out += "\n";
	}

	out += "\nCables\n\n";
	for (int64_t cid : APP->engine->getCableIds()) {
		engine::Cable* c = APP->engine->getCable(cid);
		if (!c || !c->outputModule || !c->inputModule)
			continue;
		app::ModuleWidget* om = APP->scene->rack->getModule(c->outputModule->id);
		app::ModuleWidget* im = APP->scene->rack->getModule(c->inputModule->id);
		std::snprintf(line, sizeof(line), "  %s %s  ->  %s %s\n",
			moduleName(om).c_str(),
			portName(c->outputModule, engine::Port::OUTPUT, c->outputId).c_str(),
			moduleName(im).c_str(),
			portName(c->inputModule, engine::Port::INPUT, c->inputId).c_str());
		out += line;
	}

	return out;
}


static std::string reportPath() {
	return outDir() + "/patch-report.txt";
}


std::string writePatchReport() {
	const std::string path = reportPath();
	FILE* f = std::fopen(path.c_str(), "w");
	if (!f)
		return "could not write " + path;
	const std::string text = patchReportText();
	std::fwrite(text.data(), 1, text.size(), f);
	std::fclose(f);
	return path;
}


bool writePatchReportIfChanged() {
	static std::string last;
	const std::string text = patchReportText();
	if (text == last)
		return false;
	const std::string path = reportPath();
	FILE* f = std::fopen(path.c_str(), "w");
	if (!f)
		return false;
	std::fwrite(text.data(), 1, text.size(), f);
	std::fclose(f);
	last = text;
	writeHistory();
	return true;
}


// ---------------------------------------------------------------- the history

/** How many dated copies are kept. At one copy per change and a change no oftener than every
fifteen seconds, this is several hours of continuous work — and a patch file is a few kilobytes,
so the whole history is smaller than one screenshot. */
static const int HISTORY_KEEP = 400;


std::string historyDir() {
	const std::string dir = asset::user("DreamerDemo/history");
	if (!system::isDirectory(dir))
		system::createDirectories(dir);
	return dir;
}


/** Sorted by name, which is sorted by time, because the name is the time. */
static std::vector<std::string> historyFiles() {
	std::vector<std::string> out;
	const std::string dir = historyDir();
	if (!system::isDirectory(dir))
		return out;
	for (const std::string& path : system::getEntries(dir)) {
		if (path.size() > 4 && path.compare(path.size() - 4, 4, ".vcv") == 0)
			out.push_back(path);
	}
	std::sort(out.begin(), out.end());
	return out;
}


static std::string stamp() {
	const std::time_t t = std::time(NULL);
	std::tm tmv;
	localtime_r(&t, &tmv);
	char buf[32];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %H.%M.%S", &tmv);
	return buf;
}


void writeHistory() {
	const std::string base = historyDir() + "/" + stamp();
	try {
		APP->patch->save(base + ".vcv");
	}
	catch (Exception& e) {
		WARN("DreamerDemo: could not save history: %s", e.what());
		return;
	}
	// The report goes beside it, so what a saved patch contained can be read without loading it
	// — which matters when the question is "which of these is the one I want".
	FILE* f = std::fopen((base + ".txt").c_str(), "w");
	if (f) {
		const std::string text = patchReportText();
		std::fwrite(text.data(), 1, text.size(), f);
		std::fclose(f);
	}

	// OLDEST FIRST OUT. Pruning here rather than on a timer means the folder is tidied by the
	// same act that fills it, and never grows between sessions.
	std::vector<std::string> files = historyFiles();
	for (size_t i = 0; i + HISTORY_KEEP < files.size(); i++) {
		std::remove(files[i].c_str());
		std::string txt = files[i];
		txt.replace(txt.size() - 4, 4, ".txt");
		std::remove(txt.c_str());
	}
}


std::vector<HistoryEntry> historyList(int limit) {
	std::vector<HistoryEntry> out;
	std::vector<std::string> files = historyFiles();
	for (size_t k = files.size(); k > 0 && (int) out.size() < limit; k--) {
		const std::string& path = files[k - 1];
		HistoryEntry e;
		e.path = path;
		e.when = system::getFilename(path);
		if (e.when.size() > 4)
			e.when.resize(e.when.size() - 4);
		// The module count comes off the report beside it, so a list can say how big each patch
		// was without any of them being opened.
		std::string txt = path;
		txt.replace(txt.size() - 4, 4, ".txt");
		FILE* f = std::fopen(txt.c_str(), "r");
		if (f) {
			char line[256] = {0};
			if (std::fgets(line, sizeof(line), f))
				std::sscanf(line, "The rack, %d modules", &e.modules);
			std::fclose(f);
		}
		out.push_back(e);
	}
	return out;
}


} // namespace demo
