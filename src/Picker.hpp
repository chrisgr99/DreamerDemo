#pragma once
/** The script picker: a list, not a menu.

A MENU IS THE WRONG THING for choosing between scripts. It is a list of commands with a few files
mixed into it, it does not scroll when there are more files than fit, and it says nothing about
which one you have got. This is a field showing what is loaded, and a list that drops out of it
carrying every script there is, the current one marked, scrolling when there are more than fit.

Because the list carries every script, there is nothing left for a menu to do: opening a file by
hand was only ever a way round a list that did not show them all.
*/
#include "plugin.hpp"

#include <functional>
#include <string>
#include <vector>

namespace demo {


/** Drops a list below `field` (in scene coordinates) and calls `pick` with the chosen path.
Closes on a click anywhere else, or on Escape. */
void pickerOpen(math::Rect field, const std::vector<std::string>& paths,
	const std::string& current, std::function<void(std::string)> pick);


/** WHICH SCRIPT IS CURRENT when the transport opens.

Whichever was touched last: the script that was played most recently, or the one whose file was
edited most recently, whichever of those two happened later. Editing a script in a text editor
makes it the one you are working on; so does playing it. */
std::string pickerRemembered();

/** Records that this script was played, so it is the one waiting next time. */
void pickerRemember(const std::string& path);


} // namespace demo
