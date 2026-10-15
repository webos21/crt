// Opt-in device diagnostic: CRT/PAL capture without WebKit or saved images.
#include <crtmedia/capture.h>
#include "../../libcrtweb/tests/common/crtweb_test_process.h"
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstring>

int main(int argc, char** argv)
{
    if (argc != 2 || strcmp(argv[1], "--allow-physical-camera")) {
        fprintf(stderr, "usage: %s --allow-physical-camera\n", argv[0]);
        return 2;
    }
    setvbuf(stdout, nullptr, _IOLBF, 0);
    printf("PAL_CAMERA pid=%ld\n", static_cast<long>(getpid()));
    crtmedia_capture_device_info devices[16];
    size_t count = 0;
    if (crtmedia_capture_enumerate(devices, 16, &count) != CRTMEDIA_OK || !count)
        return 1;
    crtweb_test::Resources baseline { };
    for (unsigned cycle = 1; cycle <= 24; ++cycle) {
        bool ok = true;
        unsigned frames = 0;
        auto began = std::chrono::steady_clock::now();
        std::thread worker([&] {
            crtmedia_capture* capture = nullptr;
            crtmedia_capture_config requested {640, 480, 30, CRTMEDIA_PIXEL_FORMAT_YUV420P}, actual { };
            if (crtmedia_capture_open(devices[0].id, &requested, &capture, &actual) != CRTMEDIA_OK) {
                ok = false;
                return;
            }
            ok = crtmedia_capture_start(capture) == CRTMEDIA_OK;
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            int64_t last = -1;
            while (ok && frames < 3 && std::chrono::steady_clock::now() < deadline) {
                crtmedia_frame frame { };
                auto result = crtmedia_capture_dequeue_frame(capture, 100, &frame);
                if (result == CRTMEDIA_WOULD_BLOCK || result == CRTMEDIA_ERROR_TIMEOUT)
                    continue;
                if (result != CRTMEDIA_OK) {
                    ok = false;
                    break;
                }
                ok = frame.width == actual.width && frame.height == actual.height
                    && frame.timestamp_us > last && frame.release;
                last = frame.timestamp_us;
                crtmedia_frame_release(&frame);
                ++frames;
            }
            ok &= frames == 3;
            ok &= crtmedia_capture_stop(capture) == CRTMEDIA_OK;
            crtmedia_capture_release(capture);
        });
        worker.join();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - began).count();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        auto r = crtweb_test::resources();
        if (r.memoryKiB < 0 || r.threads < 0 || r.descriptors < 0) {
            fprintf(stderr, "PAL_CAMERA resource observer unavailable\n");
            return 1;
        }
        if (cycle == 4) baseline = r;
        printf("PAL_CAMERA cycle=%u ok=%u frames=%u lifetime_ms=%lld rss=%ld threads=%ld descriptors=%ld rss_delta=%ld threads_delta=%ld descriptors_delta=%ld\n",
            cycle, ok, frames, static_cast<long long>(elapsed), r.memoryKiB, r.threads, r.descriptors,
            r.memoryKiB - baseline.memoryKiB, r.threads - baseline.threads, r.descriptors - baseline.descriptors);
        if (!ok) return 1;
    }
    std::this_thread::sleep_for(std::chrono::seconds(30));
    auto r = crtweb_test::resources();
    printf("PAL_CAMERA completed=20 final_rss_delta=%ld final_threads_delta=%ld final_descriptors_delta=%ld\n",
        r.memoryKiB - baseline.memoryKiB, r.threads - baseline.threads, r.descriptors - baseline.descriptors);
    return 0;
}
