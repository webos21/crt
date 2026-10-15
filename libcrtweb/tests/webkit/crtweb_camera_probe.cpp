// Web Tranche 8, gate 8C step 4: the physical-camera probe. NEVER part of an automated gate.
//
// Opens the host's real camera through the browser path (getUserMedia -> application grant ->
// CRT capture source -> <video> -> view pixels) and prints the evidence the acceptance document asks
// for: the device label WebKit exposes after the grant, the negotiated size and rate, the number of
// frames and how much the picture varies, then releases the device and checks it was released.
// Without --allow-physical-camera it refuses to start, and it refuses to run when the synthetic test
// camera is selected, so synthetic output can never be mistaken for physical evidence.
//
// Usage: crtweb_camera_probe --allow-physical-camera [seconds] [--stress-cycles=20]

#include "crtweb/crtweb.h"
#include "../common/crtweb_test_http_server.h"
#include "../common/crtweb_test_process.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <chrono>
#include <functional>
#include <set>
#include <string>
#include <vector>

static crtweb_runtime* gRuntime;
// Initialize every release path, including clone, before measuring repeated lifetimes.
static constexpr unsigned cameraStressWarmups = 4;
static constexpr unsigned cameraResourceSettleMs = 2000;

static bool pumpFor(unsigned ms, const std::function<bool()>& done = [] { return false; })
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (done())
            return true;
        crtweb_runtime_pump(gRuntime, 20);
    }
    return done();
}

static std::string js(crtweb_view* view, const std::string& script)
{
    struct Result { bool done = false; std::string text; } result;
    crtweb_result status = crtweb_view_evaluate_script(view, script.c_str(), [](crtweb_view*, const char* text, int error, void* data) {
        auto* r = static_cast<Result*>(data);
        r->done = true;
        r->text = error ? "<error>" : (text ? text : "");
    }, &result);
    if (status != CRTWEB_OK || !pumpFor(10000, [&] { return result.done; })) {
        fprintf(stderr, "crtweb_camera_probe: script failed/timed out\n");
        // Do not return with a callback still referencing a dead stack frame.
        crtweb_runtime_destroy(gRuntime);
        exit(1);
    }
    return result.text;
}

static pid_t webProcess()
{
    pid_t result = -1;
    for (const auto& process : crtweb_test::children()) {
        if (process.name != "WebProcess") continue;
        if (result > 0) return -1; // a replacement/cached process must not hide a leak
        result = process.pid;
    }
    return result;
}

static bool valid(const crtweb_test::Resources& r)
{
    return r.memoryKiB > 0 && r.threads > 0 && r.descriptors > 0;
}

static bool bounded(const crtweb_test::Resources& before, const crtweb_test::Resources& after)
{
    // Same predeclared bounds as the public API lifecycle test, not thresholds fitted to a run.
    return valid(before) && valid(after) && after.memoryKiB - before.memoryKiB <= 24 * 1024
        && after.threads - before.threads <= 2 && after.descriptors - before.descriptors <= 2;
}

static long cameraDescriptors(pid_t pid)
{
#if !defined(WTF_CRT_COFF_ASM) && !defined(_WIN32) && !defined(WTF_CRT_MACHO_ASM) && !defined(__APPLE__)
    std::string root = "/proc/" + std::to_string(pid) + "/fd/";
    DIR* directory = opendir(root.c_str());
    if (!directory) return -1;
    long count = 0;
    while (dirent* entry = readdir(directory)) {
        if (entry->d_name[0] == '.') continue;
        char target[4096] { };
        ssize_t size = readlink((root + entry->d_name).c_str(), target, sizeof target - 1);
        if (size < 0 && errno != ENOENT) { count = -1; break; }
        if (size > 0 && !strncmp(target, "/dev/video", 10)) ++count;
    }
    closedir(directory);
    return count;
#else
    (void)pid;
    return -1; // no independent native-camera handle identity observer on this host
#endif
}

static bool stressCamera(crtweb_view* view, unsigned cycles, const std::string& page)
{
    const unsigned warmups = cameraStressWarmups;
    crtweb_test::Resources uiBefore { }, webBefore { };
    pid_t pid = -1;
    for (unsigned iteration = 0; iteration < warmups + cycles; ++iteration) {
        auto fail = [&](const char* phase) {
            fprintf(stderr, "CAMERA_STRESS failed iteration=%u phase=%s\n", iteration + 1, phase);
            return false;
        };
        if (crtweb_view_revoke_media(view) != CRTWEB_OK) return fail("revoke");
        js(view, "document.getElementById('v').srcObject=null;window.stream=null;window.track=null;'reset'");
        js(view, "start(),'go'");
        if (!pumpFor(15000, [&] { return js(view, "String(window.result)") != "null"; })
            || js(view, "String(window.result)") != "ok") return fail("start");
        // Count fresh frame serials from a nonzero video, not the previous page's retained picture.
        if (!pumpFor(10000, [&] { return js(view, "String(document.getElementById('v').videoWidth>0&&document.getElementById('v').currentTime>0)") == "true"; })) {
            fprintf(stderr, "CAMERA_STRESS video_state=%s\n", js(view,
                "(function(){var v=document.getElementById('v');return JSON.stringify({width:v.videoWidth,time:v.currentTime,paused:v.paused,ready:v.readyState,ended:v.ended,track:window.track&&window.track.readyState,result:window.result});})()").c_str());
            return fail("video-clock");
        }
        std::set<uint64_t> serials;
        std::set<uint64_t> hashes;
        for (unsigned sample = 0; sample < 30 && (serials.size() < 3 || hashes.size() < 2); ++sample) {
            pumpFor(100);
            std::vector<uint32_t> pixels(320 * 240);
            uint64_t serial = 0;
            if (crtweb_view_copy_frame(view, pixels.data(), 320, 240, 320 * 4, &serial) != CRTWEB_OK) continue;
            uint64_t hash = 1469598103934665603ull;
            for (uint32_t pixel : pixels) hash = (hash ^ pixel) * 1099511628211ull;
            serials.insert(serial);
            hashes.insert(hash);
        }
        if (serials.size() < 3 || hashes.size() < 2) return fail("fresh-pictures");
        unsigned mode = iteration % 4;
        if (mode == 0) js(view, "window.track.stop(),'stopped'");
        else if (mode == 1) {
            if (crtweb_view_revoke_media(view) != CRTWEB_OK) return fail("stop-revoke");
        } else if (mode == 2) {
            // Replace an actively capturing document, rather than revoking it before navigation.
            if (crtweb_view_load_url(view, page.c_str()) != CRTWEB_OK) return fail("navigate");
            if (!pumpFor(15000, [&] { return js(view, "String(typeof window.start==='function'&&typeof window.track==='undefined')") == "true"; })) return fail("new-document");
        } else {
            js(view, "window.clone=window.track.clone();window.track.stop();document.getElementById('v').srcObject=new MediaStream([window.clone]);'cloned'");
            if (js(view, "window.clone.readyState") != "live") return fail("clone-live");
            pumpFor(300);
            if (js(view, "window.clone.readyState") != "live") {
                fprintf(stderr, "CAMERA_STRESS clone_state=%s\n", js(view,
                    "JSON.stringify({clone:window.clone.readyState,original:window.track.readyState,result:window.result})").c_str());
                return fail("clone-after-stop");
            }
            js(view, "window.clone.stop();window.clone=null;'last-owner-stopped'");
        }
        if (mode != 2 && !pumpFor(5000, [&] { return js(view, "window.track.readyState") == "ended"; })) return fail("track-ended");
        js(view, "document.getElementById('v').srcObject=null;window.stream=null;window.track=null;'released'");
        pumpFor(300); // allow queued main-loop frame references to retire after the worker joined
        pid_t observed = webProcess();
        if (observed <= 0 || (pid > 0 && observed != pid)) return fail("same-process");
        pid = observed;
        long cameraFds = cameraDescriptors(pid);
#if !defined(WTF_CRT_COFF_ASM) && !defined(_WIN32) && !defined(WTF_CRT_MACHO_ASM) && !defined(__APPLE__)
        if (cameraFds != 0 || cameraDescriptors(getpid()) != 0) return fail("device-fd-release");
#endif
        if (iteration == warmups - 1) {
            uiBefore = crtweb_test::resources();
            webBefore = crtweb_test::resources(pid);
            if (!valid(uiBefore) || !valid(webBefore)) return fail("baseline");
            printf("CAMERA_STRESS baseline pid=%ld ui_rss=%ld ui_threads=%ld ui_descriptors=%ld web_rss=%ld web_threads=%ld web_descriptors=%ld\n",
                static_cast<long>(pid), uiBefore.memoryKiB, uiBefore.threads, uiBefore.descriptors,
                webBefore.memoryKiB, webBefore.threads, webBefore.descriptors);
        }
        if (iteration >= warmups) {
            auto uiAfter = crtweb_test::resources();
            auto webAfter = crtweb_test::resources(pid);
            // AVFoundation's model initialization can finish asynchronously after stopRunning.
            // This fixed acceptance budget is not an AVFoundation cache/thread-release guarantee.
            auto settleStart = std::chrono::steady_clock::now();
            auto deadline = settleStart + std::chrono::milliseconds(cameraResourceSettleMs);
            while ((!bounded(uiBefore, uiAfter) || !bounded(webBefore, webAfter))
                && std::chrono::steady_clock::now() < deadline) {
                pumpFor(20);
                uiAfter = crtweb_test::resources();
                webAfter = crtweb_test::resources(pid);
            }
            auto settledMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - settleStart).count();
            printf("CAMERA_STRESS cycle=%u mode=%u pid=%ld samples=%zu distinct=%zu camera_fds=%ld settled_ms=%lld ui_rss_delta=%ld ui_threads_delta=%ld ui_descriptors_delta=%ld web_rss_delta=%ld web_threads_delta=%ld web_descriptors_delta=%ld\n",
                iteration - warmups + 1, mode, static_cast<long>(pid), serials.size(), hashes.size(), cameraFds, static_cast<long long>(settledMs),
                uiAfter.memoryKiB - uiBefore.memoryKiB, uiAfter.threads - uiBefore.threads, uiAfter.descriptors - uiBefore.descriptors,
                webAfter.memoryKiB - webBefore.memoryKiB, webAfter.threads - webBefore.threads, webAfter.descriptors - webBefore.descriptors);
            fflush(stdout);
            if (!bounded(uiBefore, uiAfter) || !bounded(webBefore, webAfter) || settledMs > cameraResourceSettleMs)
                return fail("resource-bound");
        }
    }
    printf("CAMERA_STRESS completed=%u warmups=%u same_process=1 rss_limit_kib=24576 thread_limit=2 descriptor_limit=2 settle_timeout_ms=%u\n", cycles, warmups, cameraResourceSettleMs);
    return true;
}

int main(int argc, char** argv)
{
    bool allowed = false;
    unsigned seconds = 3;
    unsigned stressCycles = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--allow-physical-camera"))
            allowed = true;
        else {
            bool stress = !strncmp(argv[i], "--stress-cycles=", 16);
            const char* number = stress ? argv[i] + 16 : argv[i];
            if (!*number || strspn(number, "0123456789") != strlen(number) || strlen(number) > 3) return 2;
            unsigned value = static_cast<unsigned>(strtoul(number, nullptr, 10));
            if (stress) {
                if (value < 10 || value > 100) return 2;
                stressCycles = value;
            } else {
                if (value > 60) return 2;
                seconds = value;
            }
        }
    }
    if (!allowed) {
        fprintf(stderr, "crtweb_camera_probe: refusing to open a camera without --allow-physical-camera\n");
        return 2;
    }
    if (getenv("CRTWEB_TEST_SYNTHETIC_CAMERA")) {
        fprintf(stderr, "crtweb_camera_probe: CRTWEB_TEST_SYNTHETIC_CAMERA is set; unset it for a physical run\n");
        return 2;
    }

    HttpServer server;
    if (crtweb_runtime_create(nullptr, &gRuntime) != CRTWEB_OK)
        return 1;
    crtweb_view_config config = { sizeof(crtweb_view_config), 320, 240, 100 };
    crtweb_view* view = nullptr;
    if (crtweb_view_create(gRuntime, &config, &view) != CRTWEB_OK) {
        crtweb_runtime_destroy(gRuntime);
        return 1;
    }
    auto cleanup = [&] {
        crtweb_view_destroy(view);
        crtweb_runtime_destroy(gRuntime);
    };
    crtweb_view_set_focus(view, 1);

    struct Record { unsigned requests = 0; } record;
    crtweb_view_callbacks callbacks = { };
    callbacks.struct_size = sizeof callbacks;
    callbacks.user_data = &record;
    callbacks.on_media_permission = [](crtweb_view*, crtweb_media_permission* request, const crtweb_media_permission_info* info, void* data) {
        ++static_cast<Record*>(data)->requests;
        fprintf(stderr, "crtweb_camera_probe: permission asked by %s (%u camera device(s)); granting for this run\n",
            info->requesting_origin_utf8, info->camera_count);
        crtweb_media_permission_complete(request, 1, 0);
    };
    crtweb_view_set_callbacks(view, &callbacks);

    crtweb_view_load_url(view, server.url("/camera").c_str());
    if (!pumpFor(15000, [&] { return js(view, "typeof window.start") == "function"; })) {
        fprintf(stderr, "crtweb_camera_probe: the page did not load\n");
        cleanup();
        return 1;
    }
    js(view, "start(),'go'");
    if (!pumpFor(20000, [&] { return js(view, "String(window.result)") != "null"; }) || js(view, "String(window.result)") != "ok") {
        fprintf(stderr, "crtweb_camera_probe: no stream: %s\n", js(view, "String(window.result)").c_str());
        cleanup();
        return 1;
    }
    pumpFor(seconds * 1000);
    std::string label = js(view, "window.track.label");
    std::string settings = js(view, "JSON.stringify(window.track.getSettings())");
    // Pictures sampled over the run: distinct hashes show a live, changing source.
    std::set<uint64_t> hashes;
    unsigned lit = 0, samples = 0;
    for (unsigned i = 0; i < 20; ++i) {
        pumpFor(100);
        std::vector<uint32_t> pixels(320 * 240);
        if (crtweb_view_copy_frame(view, pixels.data(), 320, 240, 320 * 4, nullptr) != CRTWEB_OK)
            continue;
        uint64_t hash = 1469598103934665603ull;
        unsigned count = 0;
        for (uint32_t pixel : pixels) {
            hash = (hash ^ pixel) * 1099511628211ull;
            if ((pixel & 0xffffff) != 0)
                ++count;
        }
        hashes.insert(hash);
        lit = count;
        ++samples;
    }
    printf("CAMERA_PROBE label=%s\n", label.c_str());
    printf("CAMERA_PROBE settings=%s\n", settings.c_str());
    printf("CAMERA_PROBE samples=%u distinct_pictures=%zu lit_pixels_last=%u of %u\n", samples, hashes.size(), lit, 320u * 240u);
    printf("CAMERA_PROBE permission_requests=%u\n", record.requests);
    js(view, "window.track.stop(),'s'");
    pumpFor(1000);
    std::string ended = js(view, "window.track.readyState");
    printf("CAMERA_PROBE readyState_after_stop=%s\n", ended.c_str());
    bool ok = record.requests == 1 && samples > 5 && hashes.size() > 1 && lit > 0 && ended == "ended";
    if (ok && stressCycles) {
        ok = stressCamera(view, stressCycles, server.url("/camera"));
        printf("CAMERA_STRESS permission_requests=%u\n", record.requests);
        ok &= record.requests == 1 + cameraStressWarmups + stressCycles;
    }
    cleanup();
    ok &= crtweb_test::children().empty();
    printf("crtweb_camera_probe: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
