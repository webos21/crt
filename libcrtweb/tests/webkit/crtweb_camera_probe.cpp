// Web Tranche 8, gate 8C step 4: the physical-camera probe. NEVER part of an automated gate.
//
// Opens the host's real camera through the browser path (getUserMedia -> application grant ->
// CRT capture source -> <video> -> view pixels) and prints the evidence the acceptance document asks
// for: the device label WebKit exposes after the grant, the negotiated size and rate, the number of
// frames and how much the picture varies, then releases the device and checks it was released.
// Without --allow-physical-camera it refuses to start, and it refuses to run when the synthetic test
// camera is selected, so synthetic output can never be mistaken for physical evidence.
//
// Usage: crtweb_camera_probe --allow-physical-camera [seconds]

#include "crtweb/crtweb.h"
#include "../common/crtweb_test_http_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <chrono>
#include <functional>
#include <set>
#include <string>
#include <vector>

static crtweb_runtime* gRuntime;

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
    crtweb_view_evaluate_script(view, script.c_str(), [](crtweb_view*, const char* text, int error, void* data) {
        auto* r = static_cast<Result*>(data);
        r->done = true;
        r->text = error ? "<error>" : (text ? text : "");
    }, &result);
    pumpFor(10000, [&] { return result.done; });
    return result.text;
}

int main(int argc, char** argv)
{
    bool allowed = false;
    unsigned seconds = 3;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--allow-physical-camera"))
            allowed = true;
        else
            seconds = static_cast<unsigned>(atoi(argv[i]));
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
    cleanup();
    bool ok = record.requests == 1 && samples > 5 && hashes.size() > 1 && lit > 0 && ended == "ended";
    printf("crtweb_camera_probe: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
