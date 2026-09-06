#pragma once
/** Recording the take.

A demo is made to be watched afterwards, which means somebody has to press record at the right
moment and stop at the right moment, and then trim the result. That is exactly the work this
plugin exists to avoid: the run knows when it starts and when it ends, so it can do both itself
and hand over a file that needs no editing.

MACOS ONLY, and honestly so. This uses ScreenCaptureKit, which is Apple's — there is no
equivalent to fall back on, and a plugin that drives the host's own interface was never going
to be portable anyway. Everywhere else these are stubs that say they cannot.

THE PERMISSION IS RACK'S. Screen recording is granted to the application, once, in System
Settings; Rack has to be restarted after granting it. Building this into the plugin rather than
shipping a separate recorder means there is one permission rather than two, and nothing to sign
or find on disk.
*/
#include <string>

namespace demo {


/** Whether this build and this machine can record at all. */
bool captureAvailable();

/** Starts recording Rack's own window to an MP4 at `path`, with the system's audio in the same
file. Returns false and fills `why` if it cannot — the usual reason being that the permission
has not been granted, which looks like Rack's window simply not being offered. */
bool captureStart(const std::string& path, std::string* why);

/** Closes the file. Safe to call when nothing is recording. */
void captureStop();

bool captureRunning();

/** ARMED OR NOT, kept here rather than on the runner.

The runner belongs to the transport window and dies with it, and Escape closes that window — so
a Record button that armed the runner was silently disarmed by the very key used to end a take.
Arming is a standing preference, not part of a run. */
bool captureArmed();
void captureArm(bool on);

/** THE FILE THAT WAS JUST WRITTEN, once, to whoever asks first.

Saying so is not something the recorder can do itself. A take usually ends with Escape, which
closes the transport, hides the card and takes the demo off the screen — so a message put up at
the moment the file closes has nowhere to live. The fact is left here instead, and the module's
panel, which is still there, picks it up on its next frame and says it out loud.

Returns false when there is nothing new to report. */
bool captureFinished(std::string* path, unsigned long long* bytes);


} // namespace demo
