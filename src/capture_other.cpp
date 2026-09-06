/** Recording a take, everywhere that is not macOS — see Capture.hpp.

There is nothing to fall back on. Screen recording is an operating system's own business, and
the one this plugin is written for offers ScreenCaptureKit; the others offer nothing a plugin
can use without shipping something else alongside it. So this says plainly that it cannot,
and the button that would arm it stays dark. */
#include "Capture.hpp"

#include <arch.hpp>

#if !defined ARCH_MAC

namespace demo {


bool captureAvailable() {
	return false;
}


bool captureStart(const std::string& path, std::string* why) {
	(void) path;
	if (why)
		*why = "recording is only built for macOS";
	return false;
}


void captureStop() {
}


bool captureRunning() {
	return false;
}


bool captureArmed() {
	return false;
}


void captureArm(bool on) {
	(void) on;
}


bool captureFinished(std::string* path, unsigned long long* bytes) {
	(void) path;
	(void) bytes;
	return false;
}


} // namespace demo

#endif
