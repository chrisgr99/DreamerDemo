#include "Speech.hpp"

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
static pid_t gSpeaking = 0;
static double gSpeakingUntil = 0.0;


std::string speechDir() {
	const std::string dir = asset::user("DreamerDemo/speech");
	if (!system::isDirectory(dir))
		system::createDirectories(dir);
	return dir;
}


/** FNV-1a, which is enough here: the only thing riding on it is that two different sentences get
two different file names, and a wrong answer costs one re-render. */
std::string speechId(const std::string& text) {
	uint64_t h = 1469598103934665603ULL;
	for (size_t i = 0; i < text.size(); i++) {
		h ^= (uint64_t) (unsigned char) text[i];
		h *= 1099511628211ULL;
	}
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%016llx", (unsigned long long) h);
	return buf;
}


static std::string pathFor(const std::string& text) {
	return speechDir() + "/" + speechId(text) + ".aiff";
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
	// A VOICE THAT IS NOT INSTALLED RENDERS NOTHING, and would do it silently — every line
	// failing, every note falling back to its written hold, and no sign of why.
	if (!speechVoiceExists(voice)) {
		WARN("DreamerDemo: no voice called \"%s\" is installed; nothing will be spoken",
			voice.c_str());
		return -1;
	}
	int made = 0;
	for (size_t i = 0; i < lines.size(); i++) {
		if (lines[i].empty())
			continue;
		const std::string path = pathFor(lines[i]);
		if (system::isFile(path))
			continue;
		char rateBuf[16];
		std::snprintf(rateBuf, sizeof(rateBuf), "%d", rate);
		// NO DATA FORMAT NAMED. `say` refuses one that its container will not hold, and an AIFF
		// it chose for itself is about 45 kB a second — which is nothing beside the video, and
		// this is a cache rather than something that ships.
		const char* argv[] = {"say", "-v", voice.c_str(), "-r", rateBuf,
			"-o", path.c_str(), lines[i].c_str(), NULL};
		if (runAndWait(argv, NULL))
			made++;
	}
	return made;
}


float speechLength(const std::string& text) {
	if (text.empty())
		return 0.f;
	std::map<std::string, float>::const_iterator it = gLength.find(text);
	if (it != gLength.end())
		return it->second;
	const std::string path = pathFor(text);
	const float seconds = system::isFile(path) ? measure(path) : 0.f;
	gLength[text] = seconds;
	return seconds;
}


float speechPlay(const std::string& text) {
	speechSilence();
	const float seconds = speechLength(text);
	if (seconds <= 0.f)
		return 0.f;

	// NOTHING WAITS ON THE PROCESS. The length is already known, so the runner works to that and
	// the frame is never blocked by a program starting.
	const std::string path = pathFor(text);
	const char* argv[] = {"afplay", path.c_str(), NULL};
	pid_t pid = 0;
	if (::posix_spawnp(&pid, argv[0], NULL, NULL, (char* const*) argv, environ) != 0)
		return 0.f;
	gSpeaking = pid;
	gSpeakingUntil = system::getTime() + seconds;
	return seconds;
}


void speechSilence() {
	if (!gSpeaking)
		return;
	::kill(gSpeaking, SIGTERM);
	int status = 0;
	::waitpid(gSpeaking, &status, 0);
	gSpeaking = 0;
	gSpeakingUntil = 0.0;
}


bool speechSounding() {
	if (!gSpeaking)
		return false;
	if (system::getTime() < gSpeakingUntil)
		return true;
	// Finished on its own. Reaped here rather than left as a zombie for the life of Rack.
	int status = 0;
	if (::waitpid(gSpeaking, &status, WNOHANG) != 0) {
		gSpeaking = 0;
		gSpeakingUntil = 0.0;
	}
	return false;
}


} // namespace demo
