// Web Tranche 4, milestone C1: the crtweb public API, exercised the way an application would.
//
// This program includes only <crtweb/crtweb.h>: no WebKit, WPE, GLib or JavaScriptCore header. It
// drives load/navigation/history, title/url/load callbacks, a navigation veto, script evaluation,
// frame copy and resize, pointer/key/text input, two views in one runtime, thread and argument
// checks, destruction semantics, and a second runtime after the first was destroyed.

#include "crtweb/crtweb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <functional>
#include <string>
#include <thread>
#include <vector>

static int failures;

#define CHECK(condition, message)                                                          \
    do {                                                                                   \
        if (!(condition)) {                                                                \
            fprintf(stderr, "crtweb_api_test: FAILED %s (line %d)\n", message, __LINE__);  \
            ++failures;                                                                    \
        }                                                                                  \
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

struct Recorder {
    std::vector<std::string> loads; // "<event>:<url>"
    std::string title;
    std::string url;
    std::vector<std::string> titles;
    uint64_t frame { 0 };
    unsigned frames { 0 };
    std::vector<std::string> vetoed;
    std::string denyContaining;
    unsigned terminated { 0 };

    static Recorder& of(void* data) { return *static_cast<Recorder*>(data); }

    crtweb_view_callbacks callbacks()
    {
        crtweb_view_callbacks cb = { };
        cb.struct_size = sizeof cb;
        cb.user_data = this;
        cb.on_load = [](crtweb_view*, crtweb_load_event event, const char* url, const char* message, void* data) {
            Recorder& r = of(data);
            r.loads.push_back(std::to_string(event) + ":" + url);
            if (getenv("CRTWEB_API_TEST_TRACE"))
                fprintf(stderr, "load event %d %s %s\n", event, url, message ? message : "");
            if (event == CRTWEB_LOAD_FAILED && !message)
                r.loads.push_back("failed-without-message");
        };
        cb.on_title_changed = [](crtweb_view*, const char* title, void* data) {
            Recorder& r = of(data);
            r.title = title;
            r.titles.push_back(title);
        };
        cb.on_url_changed = [](crtweb_view*, const char* url, void* data) { of(data).url = url; };
        cb.on_frame = [](crtweb_view*, uint64_t serial, void* data) {
            Recorder& r = of(data);
            r.frame = serial;
            ++r.frames;
        };
        cb.on_navigation_request = [](crtweb_view*, const char* url, void* data) -> int {
            Recorder& r = of(data);
            if (!r.denyContaining.empty() && strstr(url, r.denyContaining.c_str())) {
                r.vetoed.push_back(url);
                return 0;
            }
            return 1;
        };
        cb.on_process_terminated = [](crtweb_view*, void* data) { ++of(data).terminated; };
        return cb;
    }
};

static bool has(const Recorder& r, crtweb_load_event event, const char* urlPart)
{
    std::string prefix = std::to_string(event) + ":";
    for (const std::string& load : r.loads) {
        if (load.compare(0, prefix.size(), prefix) == 0 && load.find(urlPart) != std::string::npos)
            return true;
    }
    return false;
}

struct ScriptResult {
    bool done { false };
    int error { 0 };
    std::string text;
};

static std::string evaluate(crtweb_view* view, const char* script, int* error = nullptr)
{
    ScriptResult result;
    crtweb_result status = crtweb_view_evaluate_script(view, script, [](crtweb_view*, const char* text, int error, void* data) {
        auto* r = static_cast<ScriptResult*>(data);
        r->done = true;
        r->error = error;
        r->text = text ? text : "";
    }, &result);
    CHECK(status == CRTWEB_OK, "evaluate_script accepted");
    CHECK(pumpUntil([&] { return result.done; }), "script result delivered");
    if (getenv("CRTWEB_API_TEST_TRACE"))
        fprintf(stderr, "script '%s' -> '%s' error %d\n", script, result.text.c_str(), result.error);
    if (error)
        *error = result.error;
    return result.text;
}

static void writeFile(const std::string& path, const std::string& content)
{
    FILE* file = fopen(path.c_str(), "wb");
    if (!file) {
        fprintf(stderr, "crtweb_api_test: cannot write %s\n", path.c_str());
        exit(2);
    }
    fwrite(content.data(), 1, content.size(), file);
    fclose(file);
}

static uint32_t pixelAt(const std::vector<uint32_t>& pixels, uint32_t width, uint32_t x, uint32_t y)
{
    return pixels[static_cast<size_t>(y) * width + x];
}

int main()
{
    char cwd[4096];
    if (!getcwd(cwd, sizeof cwd))
        return 2;
    std::string directory = cwd;
    std::string pageA = directory + "/crtweb_api_a.html";
    std::string pageB = directory + "/crtweb_api_b.html";
    writeFile(pageA, "<!doctype html><title>page a</title><body style='margin:0;background:#336699'>"
        "<canvas id=c width=64 height=64 style='display:block'></canvas><script>"
        "var x=document.getElementById('c').getContext('2d');x.fillStyle='#ff0000';x.fillRect(0,0,64,64);"
        "</script><a id=l href='crtweb_api_b.html'>to b</a></body>");
    writeFile(pageB, "<!doctype html><title>page b</title><body style='background:#00aa00'>b</body>");

    // ---- version, strings, arguments ----
    CHECK(crtweb_api_version() == CRTWEB_API_VERSION, "api version");
    CHECK(strcmp(crtweb_result_string(CRTWEB_ERROR_STATE), "invalid state or wrong thread") == 0, "result string");
    CHECK(crtweb_runtime_create(nullptr, nullptr) == CRTWEB_ERROR_INVALID_ARGUMENT, "create without out pointer");

    crtweb_runtime_config config = { };
    config.struct_size = sizeof config;
    config.application_name = "CrtWebApiTest";
    CHECK(crtweb_runtime_create(&config, &gRuntime) == CRTWEB_OK && gRuntime, "runtime created");
    crtweb_runtime* second = nullptr;
    CHECK(crtweb_runtime_create(nullptr, &second) == CRTWEB_ERROR_STATE && !second, "one runtime per process");
    CHECK(crtweb_runtime_pump(nullptr, 0) == CRTWEB_ERROR_INVALID_ARGUMENT, "pump without runtime");

    crtweb_view_config viewConfig = { };
    viewConfig.struct_size = sizeof viewConfig;
    crtweb_view* view = nullptr;
    CHECK(crtweb_view_create(gRuntime, &viewConfig, &view) == CRTWEB_ERROR_INVALID_ARGUMENT, "view needs a size");
    viewConfig.width = 320;
    viewConfig.height = 240;
    viewConfig.scale_percent = 200;
    CHECK(crtweb_view_create(gRuntime, &viewConfig, &view) == CRTWEB_ERROR_UNSUPPORTED, "scale other than 100");
    viewConfig.scale_percent = 0;
    CHECK(crtweb_view_create(gRuntime, &viewConfig, &view) == CRTWEB_OK && view, "view created");

    Recorder recorder;
    crtweb_view_callbacks callbacks = recorder.callbacks();
    CHECK(crtweb_view_set_callbacks(view, &callbacks) == CRTWEB_OK, "callbacks set");

    CHECK(crtweb_view_load_url(view, nullptr) == CRTWEB_ERROR_INVALID_ARGUMENT, "null url");
    CHECK(crtweb_view_load_url(view, "javascript:alert(1)") == CRTWEB_ERROR_INVALID_ARGUMENT, "scheme not accepted");
    CHECK(crtweb_view_go_back(view) == CRTWEB_WOULD_BLOCK, "no history yet");

    // The API is owner-thread only.
    {
        crtweb_result fromThread = CRTWEB_OK, pumpFromThread = CRTWEB_OK;
        std::thread other([&] {
            fromThread = crtweb_view_reload(view);
            pumpFromThread = crtweb_runtime_pump(gRuntime, 0);
        });
        other.join();
        CHECK(fromThread == CRTWEB_ERROR_STATE && pumpFromThread == CRTWEB_ERROR_STATE, "other threads are refused");
    }

    // ---- navigation, callbacks, title/url ----
    CHECK(crtweb_view_load_url(view, ("file://" + pageA).c_str()) == CRTWEB_OK, "load page a");
    CHECK(pumpUntil([&] { return has(recorder, CRTWEB_LOAD_FINISHED, "crtweb_api_a.html"); }), "page a finished");
    CHECK(has(recorder, CRTWEB_LOAD_STARTED, "crtweb_api_a.html") && has(recorder, CRTWEB_LOAD_COMMITTED, "crtweb_api_a.html"),
        "started and committed before finished");
    CHECK(pumpUntil([&] { return recorder.title == "page a"; }, 5000), "title callback");
    CHECK(recorder.url.find("crtweb_api_a.html") != std::string::npos, "url callback");
    char buffer[64];
    size_t needed = 0;
    CHECK(crtweb_view_get_title(view, buffer, sizeof buffer, &needed) == CRTWEB_OK && !strcmp(buffer, "page a") && needed == 7, "get_title");
    char tiny[4];
    CHECK(crtweb_view_get_title(view, tiny, sizeof tiny, &needed) == CRTWEB_OK && !strcmp(tiny, "pag") && needed == 7, "get_title truncates");

    // ---- frame ----
    CHECK(pumpUntil([&] { return recorder.frames > 0; }), "frame callback");
    std::vector<uint32_t> pixels(320 * 240);
    crtweb_result copied = CRTWEB_WOULD_BLOCK;
    uint64_t serial = 0;
    CHECK(pumpUntil([&] {
        copied = crtweb_view_copy_frame(view, pixels.data(), 320, 240, 320 * 4, &serial);
        return copied == CRTWEB_OK && pixelAt(pixels, 320, 10, 10) == 0xffff0000u;
    }), "the canvas is painted");
    CHECK(pixelAt(pixels, 320, 200, 200) == 0xff336699u, "page background");
    crtweb_frame_info info = { };
    info.struct_size = sizeof info;
    CHECK(crtweb_view_get_frame_info(view, &info) == CRTWEB_OK && info.width == 320 && info.height == 240 && info.serial >= serial,
        "frame info");

    // ---- navigation veto, script ----
    int error = 0;
    CHECK(evaluate(view, "6*7", &error) == "42" && !error, "script result");
    evaluate(view, "throw new Error('boom')", &error);
    CHECK(error != 0, "script error reported");
    if (getenv("CRTWEB_API_TEST_TRACE"))
        fprintf(stderr, "user agent: %s\n", evaluate(view, "navigator.userAgent").c_str());
    CHECK(evaluate(view, "navigator.userAgent.indexOf('CrtWebApiTest/') >= 0 ? 'ua' : 'no-ua'") == "ua", "application name in the user agent");
    recorder.denyContaining = "denied.invalid";
    evaluate(view, "location.href = 'http://denied.invalid/'; 'set'");
    CHECK(pumpUntil([&] { return !recorder.vetoed.empty(); }, 5000), "navigation request seen");
    crtweb_runtime_pump(gRuntime, 100);
    CHECK(!has(recorder, CRTWEB_LOAD_STARTED, "denied.invalid"), "vetoed navigation did not start");
    recorder.denyContaining.clear();

    // ---- history ----
    recorder.loads.clear();
    CHECK(crtweb_view_load_url(view, ("file://" + pageB).c_str()) == CRTWEB_OK, "load page b");
    CHECK(pumpUntil([&] { return has(recorder, CRTWEB_LOAD_FINISHED, "crtweb_api_b.html"); }), "page b finished");
    CHECK(pumpUntil([&] { return recorder.title == "page b"; }, 5000), "page b title");
    CHECK(crtweb_view_can_go_back(view) == 1 && crtweb_view_can_go_forward(view) == 0, "history state after b");
    recorder.loads.clear();
    CHECK(crtweb_view_go_back(view) == CRTWEB_OK, "go back");
    CHECK(pumpUntil([&] { return has(recorder, CRTWEB_LOAD_FINISHED, "crtweb_api_a.html"); }), "back finished");
    CHECK(pumpUntil([&] { return recorder.title == "page a"; }, 5000), "title after back");
    CHECK(crtweb_view_can_go_forward(view) == 1, "can go forward");
    recorder.loads.clear();
    CHECK(crtweb_view_go_forward(view) == CRTWEB_OK, "go forward");
    CHECK(pumpUntil([&] { return has(recorder, CRTWEB_LOAD_FINISHED, "crtweb_api_b.html"); }), "forward finished");
    recorder.loads.clear();
    CHECK(crtweb_view_reload(view) == CRTWEB_OK, "reload");
    CHECK(pumpUntil([&] { return has(recorder, CRTWEB_LOAD_FINISHED, "crtweb_api_b.html"); }), "reload finished");
    recorder.loads.clear();
    CHECK(crtweb_view_load_url(view, "file:///crtweb_api_does_not_exist.html") == CRTWEB_OK, "load a missing file");
    CHECK(pumpUntil([&] { return has(recorder, CRTWEB_LOAD_FAILED, "does_not_exist"); }), "failure reported");

    // ---- load_html, resize, input ----
    recorder.loads.clear();
    recorder.title.clear();
    const char* inputPage =
        "<!doctype html><title>input</title><body style='margin:0;background:#101010'>"
        "<div id=box style='position:absolute;left:0;top:0;width:100px;height:100px;background:#ffffff' "
        "onclick=\"document.title='clicked'\"></div>"
        "<input id=t style='position:absolute;left:0;top:120px;width:150px' autofocus></body>";
    CHECK(crtweb_view_load_html(view, inputPage, nullptr) == CRTWEB_OK, "load_html");
    CHECK(pumpUntil([&] { return has(recorder, CRTWEB_LOAD_FINISHED, "about:blank"); }), "html finished");
    CHECK(crtweb_view_set_size(view, 200, 160, 100) == CRTWEB_OK, "resize");
    CHECK(crtweb_view_set_size(view, 200, 160, 150) == CRTWEB_ERROR_UNSUPPORTED, "scale refused");
    std::vector<uint32_t> small(200 * 160);
    CHECK(pumpUntil([&] {
        return crtweb_view_copy_frame(view, small.data(), 200, 160, 200 * 4, nullptr) == CRTWEB_OK
            && pixelAt(small, 200, 50, 50) == 0xffffffffu && pixelAt(small, 200, 190, 150) == 0xff101010u;
    }), "resized picture arrives");
    CHECK(crtweb_view_copy_frame(view, small.data(), 320, 240, 320 * 4, nullptr) != CRTWEB_OK, "stale size is not served");

    CHECK(crtweb_view_set_focus(view, 1) == CRTWEB_OK, "focus");
    CHECK(crtweb_view_pointer(view, CRTWEB_VIEW_POINTER_MOVE, 0, 50, 50, 0) == CRTWEB_OK, "pointer move");
    CHECK(crtweb_view_pointer(view, CRTWEB_VIEW_POINTER_DOWN, 1, 50, 50, 0) == CRTWEB_OK, "pointer down");
    CHECK(crtweb_view_pointer(view, CRTWEB_VIEW_POINTER_UP, 1, 50, 50, 0) == CRTWEB_OK, "pointer up");
    CHECK(pumpUntil([&] { return recorder.title == "clicked"; }, 8000), "the click reached the page");
    CHECK(crtweb_view_key(view, 1, 0, 0x61, 0, 0) == CRTWEB_ERROR_INVALID_ARGUMENT, "key without keycode");
    // Focus the field by clicking it, then type.
    crtweb_view_pointer(view, CRTWEB_VIEW_POINTER_DOWN, 1, 20, 130, 0);
    crtweb_view_pointer(view, CRTWEB_VIEW_POINTER_UP, 1, 20, 130, 0);
    crtweb_runtime_pump(gRuntime, 100);
    CHECK(crtweb_view_text(view, "Z") == CRTWEB_OK, "text");
    CHECK(pumpUntil([&] { return evaluate(view, "document.getElementById('t').value") == "Z"; }, 8000), "typed text arrived");

    // ---- a second view in the same runtime, independent state ----
    crtweb_view* other = nullptr;
    crtweb_view_config otherConfig = { sizeof(crtweb_view_config), 100, 100, 100 };
    CHECK(crtweb_view_create(gRuntime, &otherConfig, &other) == CRTWEB_OK, "second view");
    Recorder otherRecorder;
    crtweb_view_callbacks otherCallbacks = otherRecorder.callbacks();
    crtweb_view_set_callbacks(other, &otherCallbacks);
    CHECK(crtweb_view_load_html(other, "<title>second</title><body>2", nullptr) == CRTWEB_OK, "second load");
    CHECK(pumpUntil([&] { return otherRecorder.title == "second"; }), "second title");
    CHECK(recorder.title == "clicked", "the first view's state is its own");
    crtweb_view_destroy(other);
    unsigned before = otherRecorder.loads.size();
    crtweb_runtime_pump(gRuntime, 50);
    CHECK(otherRecorder.loads.size() == before, "no callback after destroy");

    // ---- shutdown, and a runtime again ----
    crtweb_view_destroy(view);
    crtweb_runtime_destroy(gRuntime);
    gRuntime = nullptr;
    CHECK(crtweb_runtime_create(nullptr, &gRuntime) == CRTWEB_OK && gRuntime, "a runtime can be created again");
    crtweb_view* again = nullptr;
    CHECK(crtweb_view_create(gRuntime, &otherConfig, &again) == CRTWEB_OK, "view in the new runtime");
    Recorder againRecorder;
    crtweb_view_callbacks againCallbacks = againRecorder.callbacks();
    crtweb_view_set_callbacks(again, &againCallbacks);
    crtweb_view_load_html(again, "<title>again</title>", nullptr);
    CHECK(pumpUntil([&] { return againRecorder.title == "again"; }), "the new runtime works");
    crtweb_runtime_destroy(gRuntime);

    unlink(pageA.c_str());
    unlink(pageB.c_str());
    if (failures)
        return 1;
    printf("crtweb_api_test: ok\n");
    return 0;
}
