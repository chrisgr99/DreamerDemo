#include "Script.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <sys/stat.h>

namespace demo {


std::string scriptDir() {
	const std::string dir = asset::user("DreamerDemo/scripts");
	if (!system::isDirectory(dir))
		system::createDirectories(dir);
	return dir;
}


std::vector<std::string> scriptList() {
	std::vector<std::string> out;
	const std::string dir = scriptDir();
	if (!system::isDirectory(dir))
		return out;
	// MOST RECENTLY EDITED FIRST. The script you are working on is the one you just saved, and
	// it should be the first row rather than wherever its name falls in the alphabet.
	std::vector<std::pair<double, std::string> > dated;
	for (const std::string& path : system::getEntries(dir)) {
		if (path.size() > 3 && path.compare(path.size() - 3, 3, ".md") == 0) {
			struct stat st;
			const double when = (::stat(path.c_str(), &st) == 0) ? (double) st.st_mtime : 0.0;
			dated.push_back(std::make_pair(-when, path));
		}
	}
	std::sort(dated.begin(), dated.end());
	for (size_t i = 0; i < dated.size(); i++)
		out.push_back(dated[i].second);
	return out;
}


static std::string trim(const std::string& s) {
	size_t a = 0, b = s.size();
	while (a < b && std::isspace((unsigned char) s[a]))
		a++;
	while (b > a && std::isspace((unsigned char) s[b - 1]))
		b--;
	return s.substr(a, b - a);
}


static std::vector<std::string> split(const std::string& s, char sep) {
	std::vector<std::string> out;
	std::string cur;
	for (char c : s) {
		if (c == sep) {
			out.push_back(trim(cur));
			cur.clear();
		}
		else {
			cur += c;
		}
	}
	out.push_back(trim(cur));
	return out;
}


/** The words of a step heading, with a quoted run kept whole so a menu item can have a space in
it. */
static std::vector<std::string> words(const std::string& s) {
	std::vector<std::string> out;
	std::string cur;
	bool quoted = false;
	for (size_t i = 0; i < s.size(); i++) {
		const char c = s[i];
		if (c == '"') {
			quoted = !quoted;
			continue;
		}
		if (!quoted && std::isspace((unsigned char) c)) {
			if (!cur.empty()) {
				out.push_back(cur);
				cur.clear();
			}
			continue;
		}
		cur += c;
	}
	if (!cur.empty())
		out.push_back(cur);
	return out;
}


/** A fraction, a percentage, or a word. Positions are how you would say it — "fully open",
"centred" — because that is the grain an author dictates in. */
static bool readValue(const std::string& s, float* out) {
	const std::string v = trim(s);
	if (v.empty())
		return false;
	if (v == "off" || v == "closed" || v == "minimum") {
		*out = 0.f;
		return true;
	}
	if (v == "on" || v == "open" || v == "maximum" || v == "full") {
		*out = 1.f;
		return true;
	}
	if (v == "centre" || v == "center" || v == "middle") {
		*out = 0.5f;
		return true;
	}
	char* end = NULL;
	const float f = std::strtof(v.c_str(), &end);
	if (end == v.c_str())
		return false;
	*out = (end && *end == '%') ? f / 100.f : f;
	return true;
}


static std::string dirOf(const std::string& path) {
	const size_t slash = path.find_last_of('/');
	return (slash == std::string::npos) ? std::string(".") : path.substr(0, slash);
}


/** A path in a script is relative to the script, because that is where an author put the file
they are naming. An absolute one is left alone. */
static std::string resolvePath(const std::string& scriptPath, const std::string& given) {
	if (!given.empty() && given[0] == '/')
		return given;
	return dirOf(scriptPath) + "/" + given;
}


Script scriptLoad(const std::string& path) {
	Script sc;
	sc.path = path;

	std::ifstream file(path.c_str());
	if (!file) {
		sc.error = "cannot read " + path;
		return sc;
	}

	std::vector<std::string> lines;
	std::string line;
	while (std::getline(file, line)) {
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		lines.push_back(line);
	}

	// The prose under a heading is gathered and attached when the next heading arrives, so a
	// note can run to several lines and several paragraphs.
	std::vector<std::string> prose;
	bool haveStep = false;

	for (size_t n = 0; n < lines.size(); n++) {
		const std::string raw = lines[n];
		const std::string t = trim(raw);

		if (t.compare(0, 3, "## ") == 0 || t == "##") {
			// The prose collected so far belongs to the step before this one.
			if (haveStep && !prose.empty()) {
				std::string note;
				for (const std::string& p : prose)
					note += (note.empty() ? "" : " ") + p;
				sc.steps.back().note = note;
			}
			prose.clear();

			Step s;
			s.line = (int) n + 1;
			const std::vector<std::string> w = words(trim(t.substr(2)));
			if (w.empty()) {
				sc.error = "line " + std::to_string(s.line) + ": a step with no move";
				return sc;
			}
			const std::string verb = w[0];

			if (verb == "say") {
				s.kind = Step::SAY;
			}
			else if (verb == "wait") {
				s.kind = Step::WAIT;
				if (w.size() < 2 || !readValue(w[1], &s.wait)) {
					sc.error = "line " + std::to_string(s.line) + ": wait needs a number";
					return sc;
				}
			}
			else if (verb == "point" || verb == "press" || verb == "click"
				|| verb == "right" || verb == "unpatch") {
				if (w.size() < 2) {
					sc.error = "line " + std::to_string(s.line) + ": " + verb + " needs a target";
					return sc;
				}
				s.kind = (verb == "point") ? Step::POINT
					: (verb == "right") ? Step::RIGHT_CLICK
					: (verb == "unpatch") ? Step::UNPATCH : Step::CLICK;
				s.target = w[1];
			}
			else if (verb == "set") {
				if (w.size() < 3 || !readValue(w[2], &s.value)) {
					sc.error = "line " + std::to_string(s.line)
						+ ": set needs a target and a value";
					return sc;
				}
				s.kind = Step::SET;
				s.target = w[1];
			}
			else if (verb == "scroll") {
				if (w.size() < 2) {
					sc.error = "line " + std::to_string(s.line) + ": scroll needs a target";
					return sc;
				}
				s.kind = Step::SCROLL;
				s.target = w[1];
				s.value = (w.size() > 2 && w[2] == "down") ? -1.f : 1.f;
			}
			else if (verb == "patch") {
				// "patch a -> b", with the arrow as its own word or stuck to either side.
				std::string joined;
				for (size_t k = 1; k < w.size(); k++)
					joined += (joined.empty() ? "" : " ") + w[k];
				const size_t arrow = joined.find("->");
				if (arrow == std::string::npos) {
					sc.error = "line " + std::to_string(s.line) + ": patch needs \"a -> b\"";
					return sc;
				}
				s.kind = Step::PATCH;
				s.target = trim(joined.substr(0, arrow));
				s.target2 = trim(joined.substr(arrow + 2));
			}
			else if (verb == "menu") {
				if (w.size() < 3) {
					sc.error = "line " + std::to_string(s.line)
						+ ": menu needs a target and an item";
					return sc;
				}
				s.kind = Step::MENU;
				s.target = w[1];
				for (size_t k = 2; k < w.size(); k++)
					s.arg += (s.arg.empty() ? "" : " ") + w[k];
			}
			else if (verb == "move") {
				if (w.size() < 4) {
					sc.error = "line " + std::to_string(s.line)
						+ ": move needs a module, an HP and a row count";
					return sc;
				}
				s.kind = Step::MOVE_MODULE;
				s.target = w[1];
				s.value = (float) std::atof(w[2].c_str());
				s.value2 = (float) std::atof(w[3].c_str());
			}
			else if (verb == "zoom") {
				s.kind = Step::ZOOM;
				if (w.size() >= 2 && w[1] != "out") {
					s.target = w[1];
					s.value = (w.size() >= 3) ? (float) std::atof(w[2].c_str()) : 2.f;
				}
			}
			else if (verb == "open") {
				if (w.size() < 2) {
					sc.error = "line " + std::to_string(s.line) + ": open needs a patch file";
					return sc;
				}
				s.kind = Step::OPEN;
				s.arg = resolvePath(path, w[1]);
			}
			else if (verb == "add") {
				// "add Plugin/Model as name"
				if (w.size() < 4 || w[2] != "as") {
					sc.error = "line " + std::to_string(s.line)
						+ ": add needs \"Plugin/Model as name\"";
					return sc;
				}
				s.kind = Step::ADD;
				s.arg = w[1];
				s.target = w[3];
			}
			else {
				sc.error = "line " + std::to_string(s.line) + ": no step called \"" + verb + "\"";
				return sc;
			}

			sc.steps.push_back(s);
			haveStep = true;
			continue;
		}

		if (t.compare(0, 2, "# ") == 0) {
			sc.title = trim(t.substr(2));
			continue;
		}

		// A header line: **Name** value. Only before the first step, because a header in the
		// middle of a script would be a setting that changed halfway through without saying so.
		if (!haveStep && t.compare(0, 2, "**") == 0) {
			const size_t close = t.find("**", 2);
			if (close == std::string::npos)
				continue;
			std::string key = trim(t.substr(2, close - 2));
			const std::string value = trim(t.substr(close + 2));
			std::transform(key.begin(), key.end(), key.begin(),
				[](unsigned char c) { return (char) std::tolower(c); });

			if (key == "patch") {
				sc.patchPath = resolvePath(path, value);
			}
			else if (key == "modules") {
				for (const std::string& one : split(value, ',')) {
					const std::vector<std::string> parts = split(one, '=');
					if (parts.size() == 2 && !parts[0].empty() && !parts[1].empty())
						sc.bindings.push_back(std::make_pair(parts[0], parts[1]));
				}
			}
			else if (key == "badges") {
				sc.badges = (value != "off" && value != "no");
			}
			else if (key == "captions") {
				sc.captions = (value != "off" && value != "no");
			}
			else if (key == "master") {
				sc.master = value;
			}
			else if (key == "duck") {
				float f = 0.f;
				if (readValue(value, &f))
					sc.duck = math::clamp(f, 0.f, 1.f);
			}
			else if (key == "voice") {
				sc.voice = value;
			}
			else if (key == "rate") {
				const int r = std::atoi(value.c_str());
				if (r > 0)
					sc.rate = r;
			}
			else if (key == "pacing") {
				for (const std::string& one : split(value, ',')) {
					const std::vector<std::string> w = words(one);
					if (w.size() != 2)
						continue;
					const float f = (float) std::atof(w[1].c_str());
					if (w[0] == "perform") sc.pacing.perform = f;
					else if (w[0] == "arrive") sc.pacing.arrive = f;
					else if (w[0] == "beat") sc.pacing.beat = f;
					else if (w[0] == "settle") sc.pacing.settle = f;
					else if (w[0] == "hold") sc.pacing.hold = f;
				}
			}
			continue;
		}

		if (haveStep && !t.empty())
			prose.push_back(t);
	}

	if (haveStep && !prose.empty()) {
		std::string note;
		for (const std::string& p : prose)
			note += (note.empty() ? "" : " ") + p;
		sc.steps.back().note = note;
	}

	if (sc.steps.empty())
		sc.error = "no steps in " + path;
	return sc;
}


} // namespace demo
