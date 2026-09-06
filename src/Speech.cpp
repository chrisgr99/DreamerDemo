#include "Speech.hpp"
#include "Capture.hpp"

#include <cstdio>
#include <cstdlib>
#include <map>

#include <signal.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

namespace demo {


/** The lengths already measured, so a line that has been asked about once is not measured again
by running a program. Cleared by nothing: a rendered line's length does not change. */
static std::map<std::string, float> gLength;

/** The process that is speaking, and when it will finish. Kept so it can be silenced and so the
ducking knows whether anything is being said. */
static double gSpeakingUntil = 0.0;


std::string speechDir() {
	const std::string dir = asset::user("DreamerDemo/speech");
	if (!system::isDirectory(dir))
		system::createDirectories(dir);
	return dir;
}


/** FNV-1a, which is enough here: the only thing riding on it is that two different sentences get
two different file names, and a wrong answer costs one re-render. */
std::string speechId(const std::string& text, const std::string& voice, int rate) {
	char rateBuf[16];
	std::snprintf(rateBuf, sizeof(rateBuf), "%d", rate);
	const std::string all = text + "\x1f" + voice + "\x1f" + rateBuf;
	uint64_t h = 1469598103934665603ULL;
	for (size_t i = 0; i < all.size(); i++) {
		h ^= (uint64_t) (unsigned char) all[i];
		h *= 1099511628211ULL;
	}
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%016llx", (unsigned long long) h);
	return buf;
}


static std::string pathFor(const std::string& text, const std::string& voice, int rate) {
	return speechDir() + "/" + speechId(text, voice, rate) + ".aiff";
}


/** Runs a program and waits for it. Used only at authoring time — rendering and measuring — and
never on the path that plays a line. */
static bool runAndWait(const char* const argv[], std::string* out) {
	int fds[2] = {-1, -1};
	if (out && ::pipe(fds) != 0)
		return false;

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	if (out) {
		posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
		posix_spawn_file_actions_addclose(&actions, fds[0]);
	}

	pid_t pid = 0;
	const int rc = ::posix_spawnp(&pid, argv[0], &actions, NULL,
		(char* const*) argv, environ);
	posix_spawn_file_actions_destroy(&actions);
	if (rc != 0) {
		if (out) {
			::close(fds[0]);
			::close(fds[1]);
		}
		return false;
	}

	if (out) {
		::close(fds[1]);
		char buf[512];
		ssize_t n = 0;
		while ((n = ::read(fds[0], buf, sizeof(buf))) > 0)
			out->append(buf, (size_t) n);
		::close(fds[0]);
	}

	int status = 0;
	::waitpid(pid, &status, 0);
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}


/** `afinfo` prints a line reading "estimated duration: 3.146 seconds". */
static float measure(const std::string& path) {
	std::string out;
	const char* argv[] = {"afinfo", path.c_str(), NULL};
	if (!runAndWait(argv, &out))
		return 0.f;
	const size_t at = out.find("estimated duration:");
	if (at == std::string::npos)
		return 0.f;
	return (float) std::atof(out.c_str() + at + 19);
}


bool speechVoiceExists(const std::string& voice) {
	std::string out;
	const char* argv[] = {"say", "-v", "?", NULL};
	if (!runAndWait(argv, &out))
		return true;   // cannot tell; assume it is there rather than refusing to render
	return out.find(voice) != std::string::npos;
}


int speechRender(const std::vector<std::string>& lines, const std::string& voice, int rate) {
	// A VOICE THAT IS NOT INSTALLED IS NOT A REASON TO SAY NOTHING.
	//
	// A script names the voice it was written for — this one asks for a premium voice that has
	// to be downloaded — and on any other machine `say` simply fails, line after line, leaving
	// a demo that runs in silence with all its pauses intact and no sign of why. The system's
	// own voice is a poorer reading and an entirely usable one, so it is used instead and the
	// substitution is written down. The file is still stored under the name the script asked
	// for, so nothing else has to know.
	const bool named = speechVoiceExists(voice);
	if (!named)
		WARN("DreamerDemo: no voice called \"%s\" is installed; using the system voice",
			voice.c_str());

	int made = 0;
	for (size_t i = 0; i < lines.size(); i++) {
		if (lines[i].empty())
			continue;
		const std::string path = pathFor(lines[i], voice, rate);
		if (system::isFile(path))
			continue;
		char rateBuf[16];
		std::snprintf(rateBuf, sizeof(rateBuf), "%d", rate);
		// NO DATA FORMAT NAMED. `say` refuses one that its container will not hold, and an AIFF
		// it chose for itself is about 45 kB a second — which is nothing beside the video, and
		// this is a cache rather than something that ships.
		const char* named_argv[] = {"say", "-v", voice.c_str(), "-r", rateBuf,
			"-o", path.c_str(), lines[i].c_str(), NULL};
		const char* plain_argv[] = {"say", "-r", rateBuf,
			"-o", path.c_str(), lines[i].c_str(), NULL};
		if (runAndWait(named ? named_argv : plain_argv, NULL))
			made++;
	}
	return made;
}


float speechLength(const std::string& text, const std::string& voice, int rate) {
	if (text.empty())
		return 0.f;
	const std::string key = speechId(text, voice, rate);
	std::map<std::string, float>::const_iterator it = gLength.find(key);
	if (it != gLength.end())
		return it->second;
	const std::string path = pathFor(text, voice, rate);
	const float seconds = system::isFile(path) ? measure(path) : 0.f;
	gLength[key] = seconds;
	return seconds;
}


float speechPlay(const std::string& text, const std::string& voice, int rate) {
	speechSilence();
	const float seconds = speechLength(text, voice, rate);
	if (seconds <= 0.f)
		return 0.f;

	// PLAYED IN THIS PROCESS, not by afplay.
	//
	// A separate program is the obvious way to play a file and it cost every recording its first
	// sentence: the system's audio capture does not pick up a process the moment it starts
	// making a noise, so the narration was missing from the front of a take while Rack's own
	// sound was there throughout. Played here it is Rack's own sound, and nothing has to be
	// noticed by anything.
	//
	// Nothing waits on it either. The length is already known, so the runner works to that and
	// no frame is blocked.
	const std::string path = pathFor(text, voice, rate);
	if (!soundPlay(path))
		return 0.f;
	gSpeakingUntil = system::getTime() + seconds;
	return seconds;
}


void speechSilence() {
	soundStop();
	gSpeakingUntil = 0.0;
}


bool speechSounding() {
	if (gSpeakingUntil <= 0.0)
		return false;
	if (system::getTime() < gSpeakingUntil && soundBusy())
		return true;
	gSpeakingUntil = 0.0;
	return false;
}


} // namespace demo
