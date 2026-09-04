#pragma once
/** The narration: rendered ahead of the take, and played on cue.

RENDERED RATHER THAN SPOKEN LIVE, for two reasons that are both about pacing rather than speed.
A note holds for as long as its own sentence takes — nothing is ever time-stretched, so the
speech sets the floor and the rate multiplier squeezes only the silences around it — and that
length cannot be known until the line has been rendered. And `say` is not repeatable: the same
words spoken twice come out at slightly different lengths, so a second take of a script would be
paced differently from the first.

RENDERED BY THE PLUGIN, not by a tool somebody has to remember to run. Loading a script hashes
every line of its prose and renders the ones that have no audio yet, so a script is
self-contained: a markdown file, and the plugin. It also removes the failure a separate rendering
step invites, where a note is reworded, the render is forgotten, and the take speaks the old
sentence with nothing on screen to say so.

MAC ONLY, and deliberately. `say` and `afinfo` and `afplay` are all on the machine already; this
plugin is an authoring tool that is never submitted to the library, so shelling out to them costs
nobody anything.
*/
#include "plugin.hpp"

#include <string>
#include <vector>

namespace demo {


/** Where the rendered lines are kept: `DreamerDemo/speech` in Rack's user folder. A cache, and
safe to delete — anything missing is rendered again the next time a script is loaded. */
std::string speechDir();

/** The name a line of prose renders to. A hash of the text, so re-wording one note re-renders
one file and a script whose words have not changed loads at once. */
std::string speechId(const std::string& text);

/** Renders anything in these lines that has no audio yet, in the named voice.

Returns how many were rendered. Slow the first time a script is written and instant every time
after, so it is done when a script is loaded rather than when it is run. */
int speechRender(const std::vector<std::string>& lines, const std::string& voice, int rate);

/** Whether the machine has that voice. A missing one renders nothing and would do it in
silence — every note falling back to its written hold, with no sign of why. */
bool speechVoiceExists(const std::string& voice);

/** How long a line lasts, in seconds. Nought when it has not been rendered — a note with no
audio holds for the script's own `hold` instead. */
float speechLength(const std::string& text);

/** Speaks it, and returns how long it will take. Nothing waits on the process: the length is
already known, and the runner works to that. */
float speechPlay(const std::string& text);

/** Stops whatever is speaking. A demo that stops should stop talking. */
void speechSilence();

/** Whether anything is being said at this moment, which is what the ducking follows. */
bool speechSounding();


} // namespace demo
