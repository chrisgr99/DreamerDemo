/** Recording a take, macOS — see Capture.hpp. */
#include "Capture.hpp"

// ARCH_MAC comes from here, NOT from the compiler: it is a make variable and is never passed as
// -DARCH_MAC. A file that tests it without including this header silently compiles its non-Mac
// branch, which is how a do-nothing stub has shipped before.
#include <arch.hpp>

#if defined ARCH_MAC

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <AVFoundation/AVFoundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <logger.hpp>

#include <unistd.h>

namespace demo { void captureNoteFinished(const char* path, unsigned long long bytes); }


/** Everything the recording owns, in one object with the lifetime of a take.

NO AUTOMATIC REFERENCE COUNTING — the SDK compiles Objective-C++ with the same flags as C++, so
every object here is retained and released by hand. There are six of them and they all die
together, which is the one case where doing it by hand is not a liability. */
API_AVAILABLE(macos(13.0))
@interface DemoRecorder : NSObject <SCStreamOutput, SCStreamDelegate> {
	SCStream* stream;
	AVAssetWriter* writer;
	AVAssetWriterInput* videoIn;
	AVAssetWriterInput* audioIn;
	dispatch_queue_t queue;
	/** A QUEUE OF ITS OWN FOR THE SOUND. Sharing one with the picture meant a frame being
	encoded held the audio buffers behind it; the writer was then not ready for them and they
	were thrown away, which is what a jittering voice is. Sound is small and must never wait
	for anything. */
	dispatch_queue_t audioQueue;
	/** The writer cannot be given a sample before it has been told where the recording begins.
	THE FIRST SAMPLE OF EITHER KIND decides that — not the first picture. Waiting for a picture
	threw away every audio buffer that arrived before it, which is the first word or two of the
	narration whenever the sound stream got going first. The two streams share a clock, so
	either one can set the origin. */
	BOOL sessionStarted;
	/** Signalled when sound is actually flowing, so a take does not begin before the recorder
	is demonstrably running. */
	dispatch_semaphore_t soundLive;
	BOOL soundSeen;
	BOOL finishing;
	/** What actually happened, since a recording that fails does so out of sight on a queue of
	its own: the counts say whether anything ever arrived, and the writer's own status says
	whether it took it. Without these a file that is simply never written looks identical to
	one that was never asked for. */
	int frames;
	int audioBits;
	int dropped;
	int audioDropped;
	NSString* outPath;
}
- (BOOL)startAtPath:(NSString*)path why:(NSString**)why;
- (void)stop;
@end


@implementation DemoRecorder

/** Rack's own window, offered by the system. It is not offered at all when screen recording has
not been granted, which is the failure worth naming: everything else about the request succeeds
and the file would simply be empty. */
- (SCWindow*)findOwnWindow:(SCShareableContent*)content {
	SCWindow* best = nil;
	CGFloat bestArea = 0.0;
	const pid_t mine = getpid();
	for (SCWindow* w in content.windows) {
		if (w.owningApplication.processID != mine)
			continue;
		const CGFloat area = w.frame.size.width * w.frame.size.height;
		if (area > bestArea) {
			bestArea = area;
			best = w;
		}
	}
	return best;
}

- (BOOL)startAtPath:(NSString*)path why:(NSString**)why {
	__block SCShareableContent* content = nil;
	dispatch_semaphore_t got = dispatch_semaphore_create(0);
	[SCShareableContent getShareableContentExcludingDesktopWindows:YES
		onScreenWindowsOnly:YES
		completionHandler:^(SCShareableContent* c, NSError* e) {
			(void) e;
			content = [c retain];
			dispatch_semaphore_signal(got);
		}];
	// A wait on the interface thread, but a short one, and only at the start of a take.
	if (dispatch_semaphore_wait(got,
			dispatch_time(DISPATCH_TIME_NOW, (int64_t) (4 * NSEC_PER_SEC))) != 0) {
		if (why)
			*why = @"the system did not answer; screen recording may not be granted";
		return NO;
	}

	SCWindow* win = [self findOwnWindow:content];
	if (!win) {
		[content release];
		if (why)
			*why = @"Rack's window was not offered. Grant Screen Recording to Rack in System "
				@"Settings, Privacy and Security, then restart Rack";
		return NO;
	}

	// SHARP, BUT NOT AT ANY PRICE. A retina window is recorded at the size it is drawn, up to
	// a limit: encoding three thousand pixels across at sixty frames a second is more work than
	// a machine also running a synthesiser should be asked for, and what it costs is dropped
	// audio. Beyond the limit the picture is scaled down, which is invisible on a video nobody
	// will watch at more than about two thousand pixels wide.
	//
	// Even numbers throughout: H.264 will not take an odd dimension.
	NSScreen* screen = [NSScreen mainScreen];
	const CGFloat scale = screen ? screen.backingScaleFactor : 1.0;
	CGFloat pxW = win.frame.size.width * scale;
	CGFloat pxH = win.frame.size.height * scale;
	const CGFloat cap = 2048.0;
	if (pxW > cap) {
		pxH *= cap / pxW;
		pxW = cap;
	}
	const size_t w = ((size_t) pxW) & ~(size_t) 1;
	const size_t h = ((size_t) pxH) & ~(size_t) 1;
	if (w < 16 || h < 16) {
		[content release];
		if (why)
			*why = @"Rack's window is too small to record";
		return NO;
	}

	SCContentFilter* filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:win];
	SCStreamConfiguration* cfg = [[SCStreamConfiguration alloc] init];
	cfg.width = w;
	cfg.height = h;
	cfg.minimumFrameInterval = CMTimeMake(1, 30);
	cfg.pixelFormat = kCVPixelFormatType_32BGRA;
	// THE POINTER IN THE FILE IS THE DRAWN ONE. A take hides the real cursor and draws its own,
	// and asking the recorder for the system's would put a second pointer in the picture.
	cfg.showsCursor = NO;
	// RACK'S OWN SOUND, WHICH IS NOW ALL OF IT.
	//
	// This used to be off, with a second stream filtered on the whole display doing the sound,
	// because the narration was spoken by another process. It is played here now, so everything
	// audible in a take — the patch and the voice — belongs to Rack, and one process tap can
	// carry it. Taking it from the system-wide tap instead meant our own audio being mixed and
	// resampled by something else on its way into the file, and the voice arrived jittering
	// although it sounded perfect as it played.
	cfg.capturesAudio = YES;
	cfg.sampleRate = 48000;
	cfg.channelCount = 2;
	cfg.queueDepth = 6;

	[content release];

	outPath = [path copy];
	NSURL* url = [NSURL fileURLWithPath:path];
	[[NSFileManager defaultManager] removeItemAtURL:url error:nil];
	NSError* err = nil;
	writer = [[AVAssetWriter alloc] initWithURL:url fileType:AVFileTypeMPEG4 error:&err];
	if (!writer) {
		[filter release];
		[cfg release];
		if (why)
			*why = err ? err.localizedDescription : @"the file could not be opened";
		return NO;
	}

	NSDictionary* videoSettings = @{
		AVVideoCodecKey: AVVideoCodecTypeH264,
		AVVideoWidthKey: @(w),
		AVVideoHeightKey: @(h),
		AVVideoCompressionPropertiesKey: @{AVVideoAverageBitRateKey: @(12000000)},
	};
	videoIn = [[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeVideo
		outputSettings:videoSettings];
	videoIn.expectsMediaDataInRealTime = YES;

	NSDictionary* audioSettings = @{
		AVFormatIDKey: @(kAudioFormatMPEG4AAC),
		AVSampleRateKey: @48000,
		AVNumberOfChannelsKey: @2,
		AVEncoderBitRateKey: @(192000),
	};
	audioIn = [[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeAudio
		outputSettings:audioSettings];
	audioIn.expectsMediaDataInRealTime = YES;

	if ([writer canAddInput:videoIn])
		[writer addInput:videoIn];
	if ([writer canAddInput:audioIn])
		[writer addInput:audioIn];
	if (![writer startWriting]) {
		if (why)
			*why = writer.error ? writer.error.localizedDescription
				: @"the writer would not start";
		[filter release];
		[cfg release];
		return NO;
	}

	queue = dispatch_queue_create("com.dreamerdemo.capture", DISPATCH_QUEUE_SERIAL);
	audioQueue = dispatch_queue_create("com.dreamerdemo.capture.audio", DISPATCH_QUEUE_SERIAL);
	stream = [[SCStream alloc] initWithFilter:filter configuration:cfg delegate:self];
	[filter release];
	[cfg release];

	// THE PICTURE STREAM CARRIES NO SOUND. It was given an audio output as well, which is how
	// the narration came to be recorded twice: once from this stream, which hears Rack — and
	// Rack is what plays the narration — and once from the stream below, which hears the whole
	// machine. Two copies of the same speech a few milliseconds apart is not an echo; it is the
	// jitter that made every take unusable.
	if (![stream addStreamOutput:self type:SCStreamOutputTypeScreen
			sampleHandlerQueue:queue error:&err]
		|| ![stream addStreamOutput:self type:SCStreamOutputTypeAudio
			sampleHandlerQueue:audioQueue error:&err]) {
		if (why)
			*why = err ? err.localizedDescription : @"the stream would not take an output";
		return NO;
	}

	__block BOOL ok = YES;
	__block NSString* startWhy = nil;
	dispatch_semaphore_t started = dispatch_semaphore_create(0);
	[stream startCaptureWithCompletionHandler:^(NSError* e) {
		if (e) {
			ok = NO;
			startWhy = [e.localizedDescription copy];
		}
		dispatch_semaphore_signal(started);
	}];
	dispatch_semaphore_wait(started,
		dispatch_time(DISPATCH_TIME_NOW, (int64_t) (4 * NSEC_PER_SEC)));

	if (ok) {
		// SOUND PROVEN TO BE FLOWING BEFORE THE DEMO BEGINS. The streams report themselves
		// started before they deliver anything, and the first sentence follows within a second
		// — so a recording that was merely "started" could still miss the opening words. This
		// waits for the first audio buffer, which is the only proof that matters.
		soundLive = dispatch_semaphore_create(0);
		if (dispatch_semaphore_wait(soundLive,
				dispatch_time(DISPATCH_TIME_NOW, (int64_t) (3 * NSEC_PER_SEC))) != 0)
			WARN("DreamerDemo capture: no sound arrived in three seconds; recording anyway");
	}
	if (!ok && why)
		*why = startWhy ? [startWhy autorelease] : @"the recording would not start";
	return ok;
}

- (void)stream:(SCStream*)s didOutputSampleBuffer:(CMSampleBufferRef)sb
	ofType:(SCStreamOutputType)type {
	if (finishing || !sb || !CMSampleBufferIsValid(sb) || !CMSampleBufferDataIsReady(sb))
		return;
	if (writer.status != AVAssetWriterStatusWriting)
		return;

	if (type == SCStreamOutputTypeScreen) {
		@synchronized(self) {
			if (!sessionStarted) {
				[writer startSessionAtSourceTime:CMSampleBufferGetPresentationTimeStamp(sb)];
				sessionStarted = YES;
			}
		}
		// A FRAME THE SYSTEM CALLS COMPLETE. It also sends frames marked idle — nothing on
		// screen changed — which carry no image and would be written as a black one. This is
		// asked AFTER the session has begun: an idle frame still carries a good timestamp, and
		// refusing to start on one was throwing away the sound that came before the first
		// picture worth keeping.
		NSArray* attachments = (NSArray*) CMSampleBufferGetSampleAttachmentsArray(sb, NO);
		if (attachments.count > 0) {
			NSDictionary* first = attachments[0];
			NSNumber* status = first[SCStreamFrameInfoStatus];
			if (status && status.intValue != SCFrameStatusComplete)
				return;
		}
		if (!videoIn.isReadyForMoreMediaData) {
			dropped++;
			return;
		}
		if (![videoIn appendSampleBuffer:sb]) {
			dropped++;
			if (frames == 0 && writer.error)
				WARN("DreamerDemo capture: the writer refused the first frame: %s",
					writer.error.localizedDescription.UTF8String);
			return;
		}
		frames++;
		return;
	}

	// Sound can be the thing that starts the recording, and often is. Both kinds arrive on
	// queues of their own now, so the one decision they share is made under a lock.
	@synchronized(self) {
		if (!sessionStarted) {
			[writer startSessionAtSourceTime:CMSampleBufferGetPresentationTimeStamp(sb)];
			sessionStarted = YES;
		}
	}
	if (!audioIn.isReadyForMoreMediaData || ![audioIn appendSampleBuffer:sb])
		audioDropped++;
	else
		audioBits++;
	if (!soundSeen) {
		soundSeen = YES;
		if (soundLive)
			dispatch_semaphore_signal(soundLive);
	}
}

- (void)stream:(SCStream*)s didStopWithError:(NSError*)error {
	(void) s;
	(void) error;
}

- (void)stop {
	finishing = YES;

	if (stream) {
		dispatch_semaphore_t done = dispatch_semaphore_create(0);
		[stream stopCaptureWithCompletionHandler:^(NSError* e) {
			(void) e;
			dispatch_semaphore_signal(done);
		}];
		dispatch_semaphore_wait(done,
			dispatch_time(DISPATCH_TIME_NOW, (int64_t) (4 * NSEC_PER_SEC)));
	}
	// Anything already handed to either queue finishes before the inputs are closed.
	if (queue)
		dispatch_sync(queue, ^{});
	if (audioQueue)
		dispatch_sync(audioQueue, ^{});

	INFO("DreamerDemo capture: %d frames (%d dropped), %d audio buffers (%d dropped), "
		"writer status %ld", frames, dropped, audioBits, audioDropped,
		writer ? (long) writer.status : -1L);
	if (writer && writer.status != AVAssetWriterStatusWriting && writer.error)
		WARN("DreamerDemo capture: the writer had already failed: %s",
			writer.error.localizedDescription.UTF8String);

	if (writer && writer.status == AVAssetWriterStatusWriting) {
		[videoIn markAsFinished];
		[audioIn markAsFinished];
		dispatch_semaphore_t written = dispatch_semaphore_create(0);
		[writer finishWritingWithCompletionHandler:^{
			dispatch_semaphore_signal(written);
		}];
		dispatch_semaphore_wait(written,
			dispatch_time(DISPATCH_TIME_NOW, (int64_t) (20 * NSEC_PER_SEC)));
	}

	if (writer && writer.status == AVAssetWriterStatusFailed)
		WARN("DreamerDemo capture: writing failed: %s",
			writer.error.localizedDescription.UTF8String);
	if (outPath) {
		NSDictionary* attrs = [[NSFileManager defaultManager]
			attributesOfItemAtPath:outPath error:nil];
		if (attrs) {
			INFO("DreamerDemo capture: wrote %s (%llu bytes)", outPath.UTF8String,
				(unsigned long long) [attrs fileSize]);
			demo::captureNoteFinished(outPath.UTF8String,
				(unsigned long long) [attrs fileSize]);
		}
		else {
			WARN("DreamerDemo capture: nothing was written to %s", outPath.UTF8String);
		}
		[outPath release];
		outPath = nil;
	}

	[stream release];
	stream = nil;
	[videoIn release];
	videoIn = nil;
	[audioIn release];
	audioIn = nil;
	[writer release];
	writer = nil;
	if (queue) {
		dispatch_release(queue);
		queue = nil;
	}
	if (audioQueue) {
		dispatch_release(audioQueue);
		audioQueue = nil;
	}
	sessionStarted = NO;
	finishing = NO;
	soundSeen = NO;
	if (soundLive) {
		dispatch_release(soundLive);
		soundLive = NULL;
	}
}

@end


namespace demo {


static id gRecorder = nil;
static bool gArmed = false;
static std::string gFinishedPath;
static unsigned long long gFinishedBytes = 0;


/** Told by the recorder as the file closes; read once by whoever says so. */
void captureNoteFinished(const char* path, unsigned long long bytes) {
	gFinishedPath = path ? path : "";
	gFinishedBytes = bytes;
}


/** THE NARRATION'S AUDIO PATH, BUILT ONCE AND LEFT UP.

Two ways of playing a rendered line have already failed. A separate program — afplay, started
once a sentence — lost the opening words of every recording, because the system's audio capture
does not pick a new process up the instant it makes a noise. Playing it here with AVAudioPlayer
put the words back and made the voice jitter, because that builds and tears down an audio path
inside the very process being captured, once per sentence, and what the capture hears while that
happens is not what was played.

So the path is built once: an engine and a player node, started when the first line is spoken and
left running for the life of Rack. Each line is scheduled onto the node that is already there.
Nothing starts, nothing stops, and the capture hears one continuous stream.

The format is taken from the first file. Every line in a script is rendered by the same voice at
the same rate, so they all match; a file that does not is reconnected for, which is the one case
that stops the engine briefly. */
static AVAudioEngine* gEngine = nil;
static AVAudioPlayerNode* gNode = nil;
static AVAudioFormat* gFormat = nil;


static bool soundEnsure(AVAudioFormat* want) {
	if (gEngine && gFormat && [gFormat isEqual:want])
		return true;

	if (!gEngine) {
		gEngine = [[AVAudioEngine alloc] init];
		gNode = [[AVAudioPlayerNode alloc] init];
		[gEngine attachNode:gNode];
	}
	else {
		[gNode stop];
		[gEngine stop];
		[gEngine disconnectNodeOutput:gNode];
	}

	[gEngine connect:gNode to:gEngine.mainMixerNode format:want];
	[gFormat release];
	gFormat = [want retain];

	NSError* err = nil;
	if (![gEngine startAndReturnError:&err]) {
		WARN("DreamerDemo: cannot start the audio engine: %s",
			err ? err.localizedDescription.UTF8String : "unknown");
		return false;
	}
	return true;
}


bool soundPlay(const std::string& path) {
	NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
	NSError* err = nil;
	AVAudioFile* file = [[AVAudioFile alloc] initForReading:url error:&err];
	if (!file) {
		WARN("DreamerDemo: cannot read %s: %s", path.c_str(),
			err ? err.localizedDescription.UTF8String : "unknown");
		return false;
	}
	if (!soundEnsure(file.processingFormat)) {
		[file release];
		return false;
	}

	// Stopping the node clears what it was playing without touching the engine, so the output
	// unit underneath keeps running between one sentence and the next.
	[gNode stop];
	[gNode scheduleFile:file atTime:nil completionHandler:nil];
	[gNode play];
	[file release];
	return true;
}


void soundStop() {
	if (gNode)
		[gNode stop];
}


bool soundBusy() {
	return gNode && gNode.isPlaying;
}


bool captureArmed() {
	return gArmed;
}


void captureArm(bool on) {
	gArmed = on;
}


bool captureFinished(std::string* path, unsigned long long* bytes) {
	if (gFinishedPath.empty())
		return false;
	if (path)
		*path = gFinishedPath;
	if (bytes)
		*bytes = gFinishedBytes;
	gFinishedPath.clear();
	gFinishedBytes = 0;
	return true;
}


bool captureAvailable() {
	if (@available(macOS 13.0, *))
		return true;
	return false;
}


bool captureStart(const std::string& path, std::string* why) {
	if (@available(macOS 13.0, *)) {
		if (gRecorder)
			captureStop();
		DemoRecorder* rec = [[DemoRecorder alloc] init];
		NSString* whyNS = nil;
		NSString* nsPath = [NSString stringWithUTF8String:path.c_str()];
		if (![rec startAtPath:nsPath why:&whyNS]) {
			if (why)
				*why = whyNS ? std::string(whyNS.UTF8String) : std::string("it would not start");
			[rec stop];
			[rec release];
			return false;
		}
		gRecorder = rec;
		return true;
	}
	if (why)
		*why = "recording needs macOS 13 or newer";
	return false;
}


void captureStop() {
	if (@available(macOS 13.0, *)) {
		if (!gRecorder)
			return;
		DemoRecorder* rec = (DemoRecorder*) gRecorder;
		gRecorder = nil;
		[rec stop];
		[rec release];
	}
}


bool captureRunning() {
	return gRecorder != nil;
}


} // namespace demo

#endif
