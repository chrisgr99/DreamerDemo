#pragma once
/** Two files that turn numbers back into names.

A PATCH FILE RECORDS PORTS BY NUMBER. `"outputModuleId": 3774…, "outputId": 1` says which cable
goes where but not what either end is called, and the names live only inside the running program,
where each module declares them with configParam, configInput and configOutput — the same strings
Rack puts in its tooltips. So reading somebody's patch from the outside means guessing which jack
is which, and guessing wrong is worse than not knowing.

These write the names down.

**The port index** builds one of every module of every installed plugin, asks it what its
controls are called, and writes a line per model. It is slow — a hundred plugins — and it only
needs doing again when a plugin is added or updated.

**The patch report** describes the rack as it stands: modules, their settings with names and
units, every cable written in names at both ends, and which inputs have nothing patched into
them. That last line is the one that finds a patch that makes no sound.
*/
#include "plugin.hpp"

#include <string>
#include <vector>

namespace demo {


/** Writes the port index. Returns what to tell the user: the path, or what went wrong. */
std::string writePortIndex();

/** Writes the patch report for the rack as it stands. Returns the path. */
std::string writePatchReport();

/** The same report as text, without writing anything. */
std::string patchReportText();

/** Writes it only if it says something different from what is already on disk.

KEPT UP TO DATE BY ITSELF, once every fifteen seconds, which is the interval Rack autosaves at —
so the report and the patch file it explains are never far apart and nobody has to remember to
press anything. Writing only on a change means the file's own timestamp says when the patch last
altered, and an untouched rack does not rewrite a file every fifteen seconds all day. */
bool writePatchReportIfChanged();


/** THE PATCH HISTORY.

Rack keeps one autosave and overwrites it every fifteen seconds, so a crash, a mistaken deletion
or a module that replaces the rack costs whatever was there — there is nothing older to go back
to. This keeps a dated copy of the patch itself every time it changes, alongside the report that
explains it, and prunes the old ones rather than growing without limit.

It is a safety net rather than version control: no names, no messages, just the rack as it stood
at a moment, and a way to get back to it. */
std::string historyDir();

/** Saves a dated copy of the patch and its report. Called when the report changes. */
void writeHistory();

/** The saved patches, newest first: the file path and a line describing it. */
struct HistoryEntry {
	std::string path;
	std::string when;     /**< as a person reads it */
	int modules = 0;
};
std::vector<HistoryEntry> historyList(int limit);


} // namespace demo
