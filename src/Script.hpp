#pragma once
/** A demo is a markdown file.

One heading per step carrying the move; ordinary prose beneath it as what is said there. A step
with no prose says nothing but still takes its pause.

MARKDOWN RATHER THAN JSON because the choreography of a demo settles in an afternoon and its
wording is rewritten a dozen times. Rewriting a sentence inside JSON means minding quotes and
escapes to change a word, which is enough friction to stop an author fixing a sentence that is
merely not very good.

    # Reading a lead sheet

    **Patch** patches/chart-intro.vcv
    **Modules** chart = DreamerMPX/mpxChart, quant = Fundamental/Quantizer
    **Badges** off
    **Captions** off
    **Pacing** perform 1.2, hold 2.6
    **Master** mixer:Level

    ## point chart:Tempo
    The tempo knob sets the speed when there is no clock patched.

    ## set chart:Tempo 60%
    ## patch chart:Chord -> quant:Pitch
    ## wait 4

The step vocabulary, one per heading:

    say                            nothing happens; the prose is the step
    wait <seconds>                 let the patch play
    point <target>                 go there and say the note; touch nothing
    press <target>                 left click it
    click <target>                 the same word, for a control that is not a button
    right <target>                 right click it, which opens its menu
    set <target> <value>           a fraction, or a percentage
    scroll <target> [up|down]
    patch <target> -> <target>     either way round
    unpatch <target>               pull the cable off and drop it
    menu <target> <item>           right click, then choose that item
    move <name> <hp> <rows>        drag a module by its panel
    zoom <name> [factor]           frame a module; `zoom out` frames the whole rack
    open <patch file>              load a patch
    add <Plugin/Model> as <name>   add a module and bind a name to it
*/
#include "Runner.hpp"

#include <string>
#include <vector>

namespace demo {


struct Script {
	std::string path;
	std::string title;
	std::string patchPath;
	std::vector<std::pair<std::string, std::string> > bindings;
	Pacing pacing;
	bool badges = true;
	bool captions = true;
	/** The parameter pulled down while the narration speaks, and by how much. One recorded
	audio track cannot be rebalanced afterwards, so the balance is set while it plays. */
	std::string master;
	float duck = 0.35f;      /**< what the master falls to, as a fraction of where it was */

	/** The voice the narration is rendered in, and how fast it speaks. Declared beside the
	words it will speak, because it is the same decision. */
	std::string voice = "Jamie (Premium)";
	int rate = 175;          /**< words per minute */
	std::vector<Step> steps;

	/** Empty when the file parsed. Anything else is what is wrong with it, with a line
	number — a script with a mistake in it should say so before it is run, not while it is
	being recorded. */
	std::string error;
};


/** Read one. A file that does not exist, or that has a step nobody can carry out, comes back
with `error` set rather than as a half-script. */
Script scriptLoad(const std::string& path);

/** Where scripts live: `DreamerDemo/scripts` in Rack's own user folder, created if it is not
there. A file anywhere else can still be opened by hand. */
std::string scriptDir();

/** Every .md file in that folder, in the order the file system gives them. */
std::vector<std::string> scriptList();


} // namespace demo
