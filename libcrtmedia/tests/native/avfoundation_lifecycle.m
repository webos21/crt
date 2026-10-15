/* Build-host diagnostic only: Apple SDK + libSystem, never a CRT target.
 * Compare AVFoundation teardown with the browser resource observer without
 * attributing the framework's asynchronous work to CRT track ownership. */
#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#include <libproc.h>
#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  long rss, threads, fds;
} Resources;

static Resources resources(void) {
  struct proc_taskinfo task = {0};
  Resources r = {-1, -1, -1};
  if (proc_pidinfo(getpid(), PROC_PIDTASKINFO, 0, &task, sizeof(task)) == sizeof(task)) {
    r.rss = (long)(task.pti_resident_size / 1024);
    r.threads = task.pti_threadnum;
  }
  int capacity = proc_pidinfo(getpid(), PROC_PIDLISTFDS, 0, NULL, 0);
  if (capacity > 0) {
    void* buffer = malloc((size_t)capacity * 2);
    if (buffer) {
      int size = proc_pidinfo(getpid(), PROC_PIDLISTFDS, 0, buffer, capacity * 2);
      if (size > 0) r.fds = size / (long)sizeof(struct proc_fdinfo);
      free(buffer);
    }
  }
  return r;
}

static void snapshot(unsigned cycle, unsigned idleMs, Resources baseline) {
  Resources r = resources();
  printf("NATIVE_CAMERA cycle=%u idle_ms=%u rss=%ld threads=%ld descriptors=%ld rss_delta=%ld threads_delta=%ld descriptors_delta=%ld\n",
         cycle, idleMs, r.rss, r.threads, r.fds, r.rss - baseline.rss,
         r.threads - baseline.threads, r.fds - baseline.fds);
  fflush(stdout);
}

@interface CameraObserver : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate> {
@public
  dispatch_semaphore_t ready;
  unsigned frames;
}
@end

@implementation CameraObserver
- (instancetype)init {
  if ((self = [super init])) ready = dispatch_semaphore_create(0);
  return self;
}
- (void)captureOutput:(AVCaptureOutput*)output didOutputSampleBuffer:(CMSampleBufferRef)sample
       fromConnection:(AVCaptureConnection*)connection {
  (void)output;
  (void)connection;
  CVPixelBufferRef pixel = CMSampleBufferGetImageBuffer(sample);
  if (!pixel || CVPixelBufferGetPixelFormatType(pixel) != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)
    return;
  if (CVPixelBufferLockBaseAddress(pixel, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess)
    return;
  // Consume/copy each plane using the native allocator; retain no image data.
  for (size_t p = 0; p < CVPixelBufferGetPlaneCount(pixel); ++p) {
    size_t size = CVPixelBufferGetBytesPerRowOfPlane(pixel, p) * CVPixelBufferGetHeightOfPlane(pixel, p);
    void* copy = malloc(size);
    if (copy) {
      memcpy(copy, CVPixelBufferGetBaseAddressOfPlane(pixel, p), size);
      __asm__ volatile("" : : "r"(copy) : "memory");
      free(copy);
    }
  }
  CVPixelBufferUnlockBaseAddress(pixel, kCVPixelBufferLock_ReadOnly);
  if (++frames == 3) dispatch_semaphore_signal(ready);
}
- (void)dealloc {
  dispatch_release(ready);
  [super dealloc];
}
@end

static BOOL lifetime(unsigned cycle) {
  BOOL success = NO;
  double began = [NSDate timeIntervalSinceReferenceDate];
  @autoreleasepool {
    NSError* error = nil;
    AVCaptureDevice* device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    if (!device) {
      fprintf(stderr, "NATIVE_CAMERA no video device\n");
      return NO;
    }
    AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
    AVCaptureSession* session = [[AVCaptureSession alloc] init];
    AVCaptureVideoDataOutput* output = [[AVCaptureVideoDataOutput alloc] init];
    CameraObserver* observer = [[CameraObserver alloc] init];
    dispatch_queue_t queue = dispatch_queue_create("crt.native-camera-control", DISPATCH_QUEUE_SERIAL);
    if (input && [session canAddInput:input] && [session canAddOutput:output]) {
      session.sessionPreset = AVCaptureSessionPreset640x480;
      output.videoSettings = @{
        (id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange),
        (id)kCVPixelBufferWidthKey: @640, (id)kCVPixelBufferHeightKey: @480
      };
      output.alwaysDiscardsLateVideoFrames = YES;
      [session addInput:input];
      [session addOutput:output];
      [output setSampleBufferDelegate:observer queue:queue];
      [session startRunning];
      success = dispatch_semaphore_wait(observer->ready, dispatch_time(DISPATCH_TIME_NOW, 10LL * NSEC_PER_SEC)) == 0;
      [session stopRunning];
      success &= !session.isRunning;
    } else {
      fprintf(stderr, "NATIVE_CAMERA setup_error=%s\n", error.localizedDescription.UTF8String ?: "unavailable device/input/output");
    }
    [output setSampleBufferDelegate:nil queue:NULL];
    dispatch_sync(queue, ^{});
    printf("NATIVE_CAMERA capture=%u frames=%u ok=%d running_after_stop=%d lifetime_ms=%.0f\n", cycle, observer->frames, success, session.isRunning,
           ([NSDate timeIntervalSinceReferenceDate] - began) * 1000);
    [session release];
    [output release];
    [observer release];
    dispatch_release(queue);
  }
  return success;
}

int main(int argc, char** argv) {
  if ((argc != 2 && argc != 3) || strcmp(argv[1], "--allow-physical-camera") ||
      (argc == 3 && strcmp(argv[2], "--idle-ms=300"))) {
    fprintf(stderr, "usage: %s --allow-physical-camera [--idle-ms=300] (4 warmups + 20 sessions; default idle 2000ms; no images saved)\n", argv[0]);
    return 2;
  }
  unsigned idleMs = argc == 3 ? 300 : 2000;
  @autoreleasepool {
    setvbuf(stdout, NULL, _IOLBF, 0);
    AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    printf("NATIVE_CAMERA pid=%d authorization=%ld runtime=host-libSystem warmups=4 cycles=20 idle_ms=%u\n", getpid(), (long)status, idleMs);
    if (status != AVAuthorizationStatusAuthorized) {
      fprintf(stderr, "NATIVE_CAMERA requires prior macOS camera authorization; no settings changed\n");
      return 3;
    }
    Resources baseline = resources();
    for (unsigned cycle = 1; cycle <= 24; ++cycle) {
      if (!lifetime(cycle)) return 1;
      usleep(300000);
      if (cycle == 4) {
        baseline = resources();
        printf("NATIVE_CAMERA baseline rss=%ld threads=%ld descriptors=%ld\n", baseline.rss, baseline.threads, baseline.fds);
      }
      snapshot(cycle, 300, baseline);
      if (idleMs == 2000) {
        usleep(1700000);
        snapshot(cycle, idleMs, baseline);
      }
    }
    for (unsigned second = 1; second <= 30; ++second) {
      usleep(1000000);
      if (second % 5 == 0) snapshot(24, idleMs + second * 1000, baseline);
    }
    puts("NATIVE_CAMERA completed=20 observation_only=1");
  }
  return 0;
}
