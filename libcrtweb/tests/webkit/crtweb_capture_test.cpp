// Web Tranche 8, gate 8C: camera permission lifecycle and video capture through the CRT capture source.
//
// Includes only <crtweb/crtweb.h>. A *synthetic* camera (CRTWEB_TEST_SYNTHETIC_CAMERA, labelled "CRT test
// camera (synthetic)") stands in for hardware so the default gate never activates a device; it records
// its lifecycle in the file named by CRTWEB_TEST_CAMERA_STATE. Physical-camera evidence is a separate,
// explicitly opt-in run (see docs/acceptance/crtweb_acceptance.md, 8C).

#include "crtweb/crtweb.h"
#include "../common/crtweb_test_http_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <chrono>
#include <functional>
#include <string>
#include <vector>

static int failures;

#define CHECK(condition, message)                                                            \
    do {                                                                                     \
        if (!(condition)) {                                                                  \
            fprintf(stderr, "crtweb_capture_test: FAILED %s (line %d)\n", message, __LINE__); \
            ++failures;                                                                      \
        } else if (getenv("CRTWEB_CAPTURE_TEST_TRACE"))                                      \
            fprintf(stderr, "crtweb_capture_test: ok %s\n", message);                        \
    } while (0)

static crtweb_runtime* gRuntime;

static bool pumpUntil(const std::function<bool()>& done, unsigned timeoutMs = 15000)
{
    unsigned waited = 0;
    while (!done()) {
        if (waited >= timeoutMs)
            return false;
        crtweb_runtime_pump(gRuntime, 20);
        waited += 20;
    }
    return true;
}

static std::string js(crtweb_view* view, const std::string& script)
{
    struct Result { bool done = false; int error = 0; std::string text; } result;
    crtweb_view_evaluate_script(view, script.c_str(), [](crtweb_view*, const char* text, int error, void* data) {
        auto* r = static_cast<Result*>(data);
        r->done = true;
        r->error = error;
        r->text = text ? text : "";
    }, &result);
    pumpUntil([&] { return result.done; }, 10000);
    return result.error ? "<error>" : result.text;
}

static std::vector<std::string> stateLines(const std::string& path)
{
    std::vector<std::string> lines;
    if (FILE* file = fopen(path.c_str(), "r")) {
        char buffer[128];
        while (fgets(buffer, sizeof buffer, file)) {
            std::string line = buffer;
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();
            lines.push_back(line);
        }
        fclose(file);
    }
    return lines;
}

static bool stateHas(const std::string& path, const char* event)
{
    for (const std::string& line : stateLines(path)) {
        if (line == event)
            return true;
    }
    return false;
}

struct PermissionRecord {
    unsigned requests { 0 };
    std::string requesting, topLevel;
    uint32_t kinds { 0 };
    uint32_t cameras { 0 };
    bool decide { false };       // answer immediately
    bool grant { false };
    crtweb_media_permission* last { nullptr };
};

static crtweb_view_callbacks callbacksFor(PermissionRecord& record)
{
    crtweb_view_callbacks callbacks = { };
    callbacks.struct_size = sizeof callbacks;
    callbacks.user_data = &record;
    callbacks.on_media_permission = [](crtweb_view*, crtweb_media_permission* request, const crtweb_media_permission_info* info, void* data) {
        auto* r = static_cast<PermissionRecord*>(data);
        ++r->requests;
        r->requesting = info->requesting_origin_utf8;
        r->topLevel = info->top_level_origin_utf8;
        r->kinds = info->kinds;
        r->cameras = info->camera_count;
        r->last = request;
        if (r->decide)
            crtweb_media_permission_complete(request, r->grant ? 1 : 0, 0);
    };
    return callbacks;
}

static std::string startCamera(crtweb_view* view, const char* constraints = "")
{
    js(view, std::string("start(") + constraints + ")");
    pumpUntil([&] { return js(view, "String(window.result)") != "null"; }, 15000);
    return js(view, "String(window.result)");
}

// The camera element fills the 320x240 view; the synthetic source draws red/green over blue/yellow.
static bool viewPixel(crtweb_view* view, int x, int y, uint32_t* out)
{
    std::vector<uint32_t> pixels(320 * 240);
    if (crtweb_view_copy_frame(view, pixels.data(), 320, 240, 320 * 4, nullptr) != CRTWEB_OK)
        return false;
    *out = pixels[static_cast<size_t>(y) * 320 + x];
    return true;
}

static bool near(uint32_t pixel, int r, int g, int b)
{
    auto close = [](int v, int e) { return v >= e - 30 && v <= e + 30; };
    return close((pixel >> 16) & 0xff, r) && close((pixel >> 8) & 0xff, g) && close(pixel & 0xff, b);
}

int main()
{
    char cwd[4096];
    if (!getcwd(cwd, sizeof cwd))
        return 2;
    std::string statePath = std::string(cwd) + "/crtweb_capture_state.txt";
    unlink(statePath.c_str());
    // Test-only: a labelled synthetic camera; no physical device is ever opened by this program.
    setenv("CRTWEB_TEST_SYNTHETIC_CAMERA", "1", 1);
    setenv("CRTWEB_TEST_CAMERA_STATE", statePath.c_str(), 1);

    HttpServer server;
    CHECK(crtweb_runtime_create(nullptr, &gRuntime) == CRTWEB_OK, "runtime");
    crtweb_view_config config = { sizeof(crtweb_view_config), 320, 240, 100 };
    crtweb_view* view = nullptr;
    CHECK(crtweb_view_create(gRuntime, &config, &view) == CRTWEB_OK, "view");
    crtweb_view_set_focus(view, 1);
    std::string page = server.url("/camera");
    auto loadCameraPage = [&](crtweb_view* target) {
        if (crtweb_view_load_url(target, page.c_str()) != CRTWEB_OK)
            return false;
        return pumpUntil([&] { return js(target, "typeof window.start") == "function"; });
    };
    CHECK(loadCameraPage(view), "camera page");
    CHECK(js(view, "String(window.isSecureContext)") == "true", "loopback is a secure context");

    // 1. No callback: denied, and the device is never touched.
    CHECK(startCamera(view) == "error:NotAllowedError", "no callback denies");
    CHECK(stateLines(statePath).empty(), "no device was opened without a grant");

    // 2. An explicit denial.
    PermissionRecord record;
    record.decide = true;
    record.grant = false;
    crtweb_view_callbacks callbacks = callbacksFor(record);
    CHECK(crtweb_view_set_callbacks(view, &callbacks) == CRTWEB_OK, "callbacks");
    CHECK(startCamera(view) == "error:NotAllowedError", "explicit denial");
    CHECK(record.requests == 1 && record.kinds == CRTWEB_MEDIA_KIND_CAMERA && record.cameras == 1, "request info: camera, one device");
    CHECK(record.requesting == "http://127.0.0.1:" + page.substr(page.find(':', 7) + 1, page.find('/', 8) - page.find(':', 7) - 1)
        || record.requesting.find("127.0.0.1") != std::string::npos, "requesting origin");
    CHECK(stateLines(statePath).empty(), "no device after a denial");

    // 3. Audio and display are never grantable.
    {
        // With no audio device the request is unsatisfiable: WebKit reports NotFoundError,
        // OverconstrainedError or (hiding device presence before a grant) NotAllowedError; each is an
        // honest failure and none may reach the application as a grantable request.
        unsigned before = record.requests;
        std::string audio = startCamera(view, "{audio:true}");
        if (getenv("CRTWEB_CAPTURE_TEST_TRACE"))
            fprintf(stderr, "trace: audio-only request -> %s\n", audio.c_str());
        CHECK(audio == "error:NotFoundError" || audio == "error:NotAllowedError" || audio == "error:OverconstrainedError", "microphone request fails honestly (no audio devices)");
        std::string both = startCamera(view, "{video:true,audio:true}");
        CHECK(both == "error:NotFoundError" || both == "error:NotAllowedError" || both == "error:OverconstrainedError", "camera+microphone request fails honestly");
        CHECK(record.requests == before, "an unsupported request never reaches the application as a grantable one");
    }
    CHECK(stateLines(statePath).empty(), "no device for unsupported requests");

    // 4. A grant: a real video track whose frames reach the <video> element and the view.
    record.grant = true;
    CHECK(startCamera(view) == "ok", "grant yields a stream");
    CHECK(js(view, "window.log[window.log.length-1]") == "granted:1:0", "one video track, no audio track");
    CHECK(js(view, "window.track.label") == "CRT test camera (synthetic)", "labelled synthetic device");
    CHECK(pumpUntil([&] { return js(view, "String(document.getElementById('v').videoWidth)") != "0"; }, 10000), "video has dimensions");
    uint32_t pixel = 0, first = 0, second = 0;
    CHECK(pumpUntil([&] { return viewPixel(view, 10, 10, &pixel) && near(pixel, 255, 0, 0); }, 10000), "upper-left quadrant is red (camera pixels)");
    CHECK(viewPixel(view, 310, 10, &pixel) && near(pixel, 0, 255, 0), "upper-right is green");
    CHECK(viewPixel(view, 10, 230, &pixel) && near(pixel, 0, 0, 255), "lower-left is blue");
    CHECK(viewPixel(view, 310, 230, &pixel) && near(pixel, 255, 255, 0), "lower-right is yellow");
    {
        // The sweeping bar makes consecutive pictures differ: sample a column over time.
        std::vector<uint32_t> a(320 * 240), b(320 * 240);
        bool changed = false;
        crtweb_view_copy_frame(view, a.data(), 320, 240, 320 * 4, nullptr);
        for (int i = 0; i < 40 && !changed; ++i) {
            crtweb_runtime_pump(gRuntime, 40);
            if (crtweb_view_copy_frame(view, b.data(), 320, 240, 320 * 4, nullptr) == CRTWEB_OK)
                changed = a != b;
        }
        CHECK(changed, "frames change over time (live source)");
        (void)first;
        (void)second;
    }
    CHECK(stateHas(statePath, "open") && stateHas(statePath, "start"), "the source was opened and started after the grant");

    // 5. track.stop() releases the source.
    js(view, "window.track.stop(),'stopped'");
    CHECK(pumpUntil([&] { return stateHas(statePath, "stop") && stateHas(statePath, "close"); }, 10000), "track.stop() stops and closes the source");

    // 6. A duplicate / late completion is rejected.
    CHECK(record.last && crtweb_media_permission_complete(record.last, 1, 0) == CRTWEB_ERROR_INVALID_ARGUMENT, "a resolved request cannot be completed again");

    // 7. Revocation ends the track and releases the source; the next request asks again. (Within one
    // document WebKit keeps a granted frame's access after track.stop(), as getUserMedia's spec allows;
    // revocation is what resets it.)
    unlink(statePath.c_str());
    CHECK(startCamera(view) == "ok", "second stream in the same document");
    CHECK(pumpUntil([&] { return stateHas(statePath, "start"); }, 8000), "second start");
    unsigned asked = record.requests;
    CHECK(crtweb_view_revoke_media(view) == CRTWEB_OK, "revoke");
    CHECK(pumpUntil([&] { return js(view, "window.log.indexOf('ended') >= 0") == "true"; }, 10000), "revocation ends the track");
    CHECK(pumpUntil([&] { return stateHas(statePath, "stop") && stateHas(statePath, "close"); }, 10000), "revocation releases the source");
    unlink(statePath.c_str());
    CHECK(startCamera(view) == "ok", "a new request after revocation");
    CHECK(record.requests == asked + 1, "the application is asked again after a revocation");
    js(view, "window.track.stop(),'s'");
    CHECK(pumpUntil([&] { return stateHas(statePath, "close"); }, 10000), "released");

    // 8. Navigation replaces the document: tracks end, the source is released, a new page asks again.
    unlink(statePath.c_str());
    CHECK(startCamera(view) == "ok", "third grant");
    CHECK(pumpUntil([&] { return stateHas(statePath, "start"); }, 8000), "third start");
    CHECK(loadCameraPage(view), "navigate to a fresh page");
    CHECK(pumpUntil([&] { return stateHas(statePath, "stop") && stateHas(statePath, "close"); }, 10000), "navigation releases the source");
    asked = record.requests;
    CHECK(startCamera(view) == "ok" && record.requests == asked + 1, "the new document asks again");
    js(view, "window.track.stop(),'x'");
    CHECK(pumpUntil([&] { return stateHas(statePath, "close"); }, 10000), "released again");

    // 9. A second view: its request is its own; a pending request dies with the view.
    crtweb_view* other = nullptr;
    CHECK(crtweb_view_create(gRuntime, &config, &other) == CRTWEB_OK, "second view");
    PermissionRecord otherRecord;
    crtweb_view_callbacks otherCallbacks = callbacksFor(otherRecord);
    crtweb_view_set_callbacks(other, &otherCallbacks);
    CHECK(loadCameraPage(other), "second view page");
    js(other, "start(),'go'");
    CHECK(pumpUntil([&] { return otherRecord.requests == 1; }, 10000), "the second view asked on its own");
    CHECK(record.requests == asked + 1, "the first view's grant did not leak to the second");
    crtweb_media_permission* pending = otherRecord.last;
    crtweb_view_destroy(other);
    CHECK(crtweb_media_permission_complete(pending, 1, 0) == CRTWEB_ERROR_INVALID_ARGUMENT, "a request pending on a destroyed view is already denied");
    crtweb_runtime_pump(gRuntime, 100);

    // 10. Nothing leaked: the source is closed and the runtime ends cleanly.
    crtweb_view_destroy(view);
    crtweb_runtime_destroy(gRuntime);
    unlink(statePath.c_str());
    if (failures)
        return 1;
    printf("crtweb_capture_test: ok\n");
    return 0;
}
