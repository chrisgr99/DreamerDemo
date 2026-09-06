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
	/** A SECOND STREAM, FOR SOUND ALONE.
	
	Audio taken alongside a window is that window's application, and half of what a demo makes
	is spoken by another process altogether — the voice is rendered to a file and played by the
	system's own player. So the picture comes from Rack's window and the sound comes from a
	stream filtered on the whole display, which is everything audible: the patch, the narration,
	and anything else making a noise. Its own video is never collected. */
	SCStream* audioStream;
	AVAssetWriter* writer;
	AVAssetWriterInput* videoIn;
	AVAssetWriterInput* audioIn;
	dispatch_queue_t queue;
	/** The writer cannot be given a sample before it has been told where the recording begins,
	and the first video frame is what decides that. Audio that arrives before it is dropped. */
	BOOL sessionStarted;
	BOOL finishing;
	/** What actually happened, since a recording that fails does so out of sight on a queue of
	its own: the counts say whether anything ever arrived, and the writer's own status says
	whether it took it. Without these a file that is simply never written looks identical to
	one that was never asked for. */
	int frames;
	int audioBits;
	int dropped;
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

	// AT THE SCREEN'S OWN RESOLUTION, so a retina display is recorded at the size it is drawn
	// rather than at half of it. Even numbers: H.264 will not take an odd dimension.
	NSScreen* screen = [NSScreen mainScreen];
	const CGFloat scale = screen ? screen.backingScaleFactor : 1.0;
	const size_t w = ((size_t) (win.frame.size.width * scale)) & ~(size_t) 1;
	const size_t h = ((size_t) (win.frame.size.height * scale)) & ~(size_t) 1;
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
	cfg.minimumFrameInterval = CMTimeMake(1, 60);
	cfg.pixelFormat = kCVPixelFormatType_32BGRA;
	// THE POINTER IN THE FILE IS THE DRAWN ONE. A take hides the real cursor and draws its own,
	// and asking the recorder for the system's would put a second pointer in the picture.
	cfg.showsCursor = NO;
	// Not here: this stream's audio would be Rack's alone. See audioStream.
	cfg.capturesAudio = NO;
	cfg.queueDepth = 6;

	// The display the window is on, for the sound.
	SCDisplay* display = nil;
	for (SCDisplay* d in content.displays) {
		if (CGRectIntersectsRect(d.frame, win.frame)) {
			display = d;
			break;
		}
	}
	if (!display && content.displays.count > 0)
		display = content.displays[0];
	SCContentFilter* soundFilter = display
		? [[SCContentFilter alloc] initWithDisplay:display excludingWindows:@[]] : nil;
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
		AVVideoCompressionPropertiesKey: @{AVVideoAverageBitRateKey: @(16000000)},
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
	stream = [[SCStream alloc] initWithFilter:filter configuration:cfg delegate:self];
	[filter release];
	[cfg release];

	if (![stream addStreamOutput:self type:SCStreamOutputTypeScreen
			sampleHandlerQueue:queue error:&err]
		|| ![stream addStreamOutput:self type:SCStreamOutputTypeAudio
			sampleHandlerQueue:queue error:&err]) {
		if (why)
			*why = err ? err.localizedDescription : @"the stream would not take an output";
		return NO;
	}

	if (soundFilter) {
		SCStreamConfiguration* acfg = [[SCStreamConfiguration alloc] init];
		acfg.capturesAudio = YES;
		acfg.sampleRate = 48000;
		acfg.channelCount = 2;
		// A stream must carry a picture whether or not anybody collects it, so this one carries
		// the smallest and slowest picture it will accept. No screen output is added, so those
		// frames are made and dropped inside the system and never reach us.
		acfg.width = 160;
		acfg.height = 120;
		acfg.minimumFrameInterval = CMTimeMake(1, 1);
		acfg.queueDepth = 5;
		audioStream = [[SCStream alloc] initWithFilter:soundFilter configuration:acfg
			delegate:self];
		[soundFilter release];
		[acfg release];
		if (![audioStream addStreamOutput:self type:SCStreamOutputTypeAudio
				sampleHandlerQueue:queue error:&err]) {
			WARN("DreamerDemo capture: no sound: %s",
				err ? err.localizedDescription.UTF8String : "the stream refused an audio output");
			[audioStream release];
			audioStream = nil;
		}
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

	if (ok && audioStream) {
		dispatch_semaphore_t sound = dispatch_semaphore_create(0);
		[audioStream startCaptureWithCompletionHandler:^(NSError* e) {
			if (e)
				WARN("DreamerDemo capture: no sound: %s", e.localizedDescription.UTF8String);
			dispatch_semaphore_signal(sound);
		}];
		dispatch_semaphore_wait(sound,
			dispatch_time(DISPATCH_TIME_NOW, (int64_t) (4 * NSEC_PER_SEC)));
	}
	if (!ok && why)
		*why = startWhy ? [startWhy autorelease] : @"the recording would not start";
	return ok;
}

- (void)stream:(SCStream*)s didOutputSampleBuffer:(CMSampleBufferRef)sb
	ofType:(SCStreamOutputType)type {
	(void) s;
	if (finishing || !sb || !CMSampleBufferIsValid(sb) || !CMSampleBufferDataIsReady(sb))
		return;
	if (writer.status != AVAssetWriterStatusWriting)
		return;

	if (type == SCStreamOutputTypeScreen) {
		// A FRAME THE SYSTEM CALLS COMPLETE. It also sends frames marked idle — nothing on
		// screen changed — which carry no image and would be written as a black one.
		NSArray* attachments = (NSArray*) CMSampleBufferGetSampleAttachmentsArray(sb, NO);
		if (attachments.count > 0) {
			NSDictionary* first = attachments[0];
			NSNumber* status = first[SCStreamFrameInfoStatus];
			if (status && status.intValue != SCFrameStatusComplete)
				return;
		}
		const CMTime pts = CMSampleBufferGetPresentationTimeStamp(sb);
		if (!sessionStarted) {
			[writer startSessionAtSourceTime:pts];
			sessionStarted = YES;
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

	// Audio before the first frame has nowhere to go: the recording does not begin until the
	// session does, and the session begins at the first picture.
	if (sessionStarted && audioIn.isReadyForMoreMediaData && [audioIn appendSampleBuffer:sb])
		audioBits++;
}

- (void)stream:(SCStream*)s didStopWithError:(NSError*)error {
	(void) s;
	(void) error;
}

- (void)stop {
	finishing = YES;

	SCStream* both[2] = {stream, audioStream};
	for (int i = 0; i < 2; i++) {
		if (!both[i])
			continue;
		dispatch_semaphore_t done = dispatch_semaphore_create(0);
		[both[i] stopCaptureWithCompletionHandler:^(NSError* e) {
			(void) e;
			dispatch_semaphore_signal(done);
		}];
		dispatch_semaphore_wait(done,
			dispatch_time(DISPATCH_TIME_NOW, (int64_t) (4 * NSEC_PER_SEC)));
	}
	// Anything already handed to the queue finishes before the inputs are closed.
	if (queue)
		dispatch_sync(queue, ^{});

	INFO("DreamerDemo capture: %d frames, %d audio buffers, %d dropped, writer status %ld",
		frames, audioBits, dropped, writer ? (long) writer.status : -1L);
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
	[audioStream release];
	audioStream = nil;
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
	sessionStarted = NO;
	finishing = NO;
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
