// Web Tranche 5: public API recovery, navigation/resize and process/resource lifecycle.
#include "crtweb/crtweb.h"
#include "../common/crtweb_test_process.h"
#include <chrono>
#include <functional>
#include <thread>
#include <vector>

static int failures;
static void check(bool condition, const char* message)
{
    if (condition) return;
    fprintf(stderr, "crtweb_api_lifecycle_test: FAILED %s\n", message);
    ++failures;
}
static bool until(crtweb_runtime* runtime, const std::function<bool()>& ready, unsigned ms = 15000)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (!ready()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        if (runtime) crtweb_runtime_pump(runtime, 10);
        else std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}
struct Seen {
    unsigned finished = 0, terminated = 0, frames = 0, scripts = 0;
    crtweb_view_callbacks callbacks()
    {
        crtweb_view_callbacks cb { };
        cb.struct_size = sizeof cb;
        cb.user_data = this;
        cb.on_load = [](crtweb_view*, crtweb_load_event event, const char*, const char*, void* data) {
            if (event == CRTWEB_LOAD_FINISHED) ++static_cast<Seen*>(data)->finished;
        };
        cb.on_frame = [](crtweb_view*, uint64_t, void* data) { ++static_cast<Seen*>(data)->frames; };
        cb.on_process_terminated = [](crtweb_view*, void* data) { ++static_cast<Seen*>(data)->terminated; };
        return cb;
    }
};
static void destroy(crtweb_runtime* runtime)
{
    auto started = std::chrono::steady_clock::now();
    crtweb_runtime_destroy(runtime);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    check(milliseconds < 5000, "runtime destruction bounded below five seconds");
}
static const char html[] = "<!doctype html><title>lifecycle</title><body style='margin:0;background:#336699'>"
    "<canvas id=c width=64 height=64></canvas><script>let x=c.getContext('2d');"
    "x.fillStyle='#ff0000';x.fillRect(0,0,64,64)</script>";
static bool picture(crtweb_view* view, unsigned width, unsigned height)
{
    std::vector<uint32_t> pixels(width * height);
    return crtweb_view_copy_frame(view, pixels.data(), width, height, width * 4, nullptr) == CRTWEB_OK
        && pixels[10 * width + 10] == 0xffff0000u && pixels[100 * width + 100] == 0xff336699u;
}
static pid_t webProcess()
{
    pid_t found = 0;
    for (const auto& process : crtweb_test::children())
        if (process.name == "WebProcess") { check(!found, "only one web process in the recovery fixture"); found = process.pid; }
    return found;
}

extern "C" int main(int argc, char** argv)
{
    unsigned cycles = argc > 1 ? atoi(argv[1]) : 100;
    if (cycles < 12) return 2;
    setvbuf(stdout, nullptr, _IOLBF, 0);
    crtweb_runtime* runtime = nullptr;
    check(crtweb_runtime_create(nullptr, &runtime) == CRTWEB_OK, "create recovery runtime");
    if (!runtime) return 1;
    crtweb_view_config config { sizeof config, 320, 240, 100 };
    crtweb_view* view = nullptr;
    check(crtweb_view_create(runtime, &config, &view) == CRTWEB_OK, "create recovery view");
    Seen seen;
    auto cb = seen.callbacks();
    crtweb_view_set_callbacks(view, &cb);
    // A file URL can actually be reloaded after death; load_html's about:blank has no persistent document.
    std::string file = "crtweb_lifecycle_recovery.html";
    FILE* output = fopen(file.c_str(), "wb");
    if (!output) return 2;
    fwrite(html, 1, sizeof html - 1, output);
    fclose(output);
    char cwd[4096];
    if (!getcwd(cwd, sizeof cwd)) return 2;
    std::string url = "file://" + std::string(cwd) + "/" + file;
    crtweb_view_load_url(view, url.c_str());
    check(until(runtime, [&] { return seen.finished && picture(view, 320, 240); }), "initial real frame");
    for (unsigned crash = 0; crash < 3 && !failures; ++crash) {
        pid_t old = webProcess();
        check(old > 0 && kill(old, SIGKILL) == 0, "force WebProcess termination");
        check(until(runtime, [&] { return seen.terminated == crash + 1; }), "one process-termination callback");
        std::vector<uint32_t> deadPixels(360 * 260, 0x11223344u);
        unsigned oldWidth = crash ? 360 : 320, oldHeight = crash ? 260 : 240;
        check(crtweb_view_copy_frame(view, deadPixels.data(), oldWidth, oldHeight, oldWidth * 4, nullptr)
            == CRTWEB_WOULD_BLOCK && deadPixels[0] == 0x11223344u, "terminated view has no live frame");
        unsigned loaded = seen.finished;
        crtweb_view_set_size(view, 360, 260, 100);
        check(crtweb_view_reload(view) == CRTWEB_OK, "reload terminated view");
        check(until(runtime, [&] { return seen.finished > loaded && picture(view, 360, 260); }), "reload repaints at new size");
        pid_t replacement = webProcess();
        check(replacement > 0 && replacement != old, "new WebProcess PID");
        check(seen.terminated == crash + 1, "termination notified exactly once");
        printf("crtweb_api_lifecycle_test: recovery %u: old=%d new=%d\n", crash + 1, old, replacement);
    }
    destroy(runtime);
    check(crtweb_test::children().empty(), "runtime destroy returns with every child reaped");
    unlink(file.c_str());
    if (failures) return 1;

    // A wedged script cannot cooperate with page close. Shutdown still has to reap its process.
    check(crtweb_runtime_create(nullptr, &runtime) == CRTWEB_OK, "create busy runtime");
    check(crtweb_view_create(runtime, &config, &view) == CRTWEB_OK, "create busy view");
    Seen busy;
    cb = busy.callbacks();
    crtweb_view_set_callbacks(view, &cb);
    crtweb_view_load_html(view, html, nullptr);
    check(until(runtime, [&] { return busy.finished && picture(view, 320, 240); }), "busy fixture loaded");
    crtweb_view_evaluate_script(view, "while (true) {}", [](crtweb_view*, const char*, int, void* data) {
        ++static_cast<Seen*>(data)->scripts;
    }, &busy);
    auto busyDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
    while (std::chrono::steady_clock::now() < busyDeadline) crtweb_runtime_pump(runtime, 10);
    destroy(runtime);
    check(crtweb_test::children().empty(), "unresponsive WebProcess reaped on runtime destruction");
    check(!busy.scripts, "pending busy script callback retired");
    if (failures) return 1;

    crtweb_runtime *left = nullptr, *right = nullptr;
    crtweb_view *leftView = nullptr, *rightView = nullptr;
    crtweb_runtime_create(nullptr, &left);
    crtweb_runtime_create(nullptr, &right);
    crtweb_view_create(left, &config, &leftView);
    crtweb_view_create(right, &config, &rightView);
    Seen leftSeen, rightSeen;
    cb = leftSeen.callbacks(); crtweb_view_set_callbacks(leftView, &cb);
    cb = rightSeen.callbacks(); crtweb_view_set_callbacks(rightView, &cb);
    crtweb_view_load_html(leftView, html, nullptr);
    crtweb_view_load_html(rightView, html, nullptr);
    check(until(right, [&] { return leftSeen.finished && rightSeen.finished
        && picture(leftView, 320, 240) && picture(rightView, 320, 240); }), "two runtimes paint");
    crtweb_view_evaluate_script(leftView, "while (true) {}", nullptr, nullptr);
    busyDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
    while (std::chrono::steady_clock::now() < busyDeadline) crtweb_runtime_pump(right, 10);
    destroy(left);
    check(!rightSeen.terminated, "destroying busy runtime leaves sibling alive");
    unsigned finished = rightSeen.finished;
    crtweb_view_load_html(rightView, html, nullptr);
    check(until(right, [&] { return rightSeen.finished > finished && picture(rightView, 320, 240); }), "sibling navigates after shutdown");
    destroy(right);
    check(crtweb_test::children().empty(), "both runtimes reaped");
    if (failures) return 1;

    std::vector<crtweb_test::Resources> samples;
    // No pump has delivered the asynchronous launch callback yet.
    for (unsigned i = 0; i < 20; ++i) {
        crtweb_runtime_create(nullptr, &runtime);
        crtweb_view_create(runtime, &config, &view);
        crtweb_view_load_html(view, html, nullptr);
        destroy(runtime);
        const auto remaining = crtweb_test::children();
        for (const auto& child : remaining)
            fprintf(stderr, "crtweb_api_lifecycle_test: launch-in-flight remaining child %d %s\n", child.pid, child.name.c_str());
        check(remaining.empty(), "launch-in-flight shutdown reaps children");
        if (failures) return 1;
    }
    for (unsigned i = 0; i < cycles; ++i) {
        runtime = nullptr;
        check(crtweb_runtime_create(nullptr, &runtime) == CRTWEB_OK, "create cycle runtime");
        if (!runtime) break;
        view = nullptr;
        check(crtweb_view_create(runtime, &config, &view) == CRTWEB_OK, "create cycle view");
        if (!view) { destroy(runtime); break; }
        Seen cycle;
        cb = cycle.callbacks();
        crtweb_view_set_callbacks(view, &cb);
        // Navigate while several resizes are still in flight; the final extent is authoritative.
        crtweb_view_load_html(view, "<!doctype html><title>superseded</title>", nullptr);
        crtweb_view_set_size(view, 340, 250, 100);
        crtweb_view_load_html(view, html, nullptr);
        crtweb_view_set_size(view, 380, 270, 100);
        crtweb_view_set_size(view, 360, 260, 100);
        check(until(runtime, [&] { return cycle.finished && picture(view, 360, 260); }), "navigation while resizing paints final extent");
        check(crtweb_test::children().size() == 2, "exactly Web and Network children");
        // Teardown with asynchronous work pending: no callbacks may reach the retired view.
        crtweb_view_evaluate_script(view, "42", [](crtweb_view*, const char*, int, void* data) {
            ++static_cast<Seen*>(data)->scripts;
        }, &cycle);
        unsigned callbacks = cycle.finished + cycle.frames + cycle.terminated + cycle.scripts;
        if (i % 2) crtweb_view_destroy(view); // both explicit-view and runtime-owned teardown
        destroy(runtime);
        check(callbacks == cycle.finished + cycle.frames + cycle.terminated + cycle.scripts, "no callbacks after view destruction");
        check(crtweb_test::children().empty(), "all cycle children reaped before destroy returns");
        samples.push_back(crtweb_test::resources());
        if (i % 10 == 9) {
            const auto& sample = samples.back();
            printf("crtweb_api_lifecycle_test: cycle %u: rss=%ld KiB threads=%ld descriptors=%ld\n",
                i + 1, sample.memoryKiB, sample.threads, sample.descriptors);
        }
        if (failures) break;
    }
    if (samples.size() >= 12) {
        const auto& warm = samples[9];
        const auto& last = samples.back();
        check(warm.memoryKiB >= 0 && warm.threads >= 0 && warm.descriptors >= 0, "resource observations available");
        check(last.memoryKiB >= 0 && last.threads >= 0 && last.descriptors >= 0, "final resource observations available");
        printf("crtweb_api_lifecycle_test: %zu cycles after warmup: rss=%+ld KiB threads=%+ld descriptors=%+ld\n",
            samples.size(), last.memoryKiB - warm.memoryKiB, last.threads - warm.threads, last.descriptors - warm.descriptors);
        check(last.memoryKiB - warm.memoryKiB <= 24 * 1024, "RSS growth bounded at 24 MiB");
        check(last.threads - warm.threads <= 2, "thread growth bounded at 2");
        check(last.descriptors - warm.descriptors <= 2, "descriptor/handle growth bounded at 2");
    } else check(false, "at least 12 completed cycles");
    check(samples.size() == cycles, "every lifecycle cycle completed");
    if (failures) return 1;
    puts("crtweb_api_lifecycle_test: ok");
    return 0;
}
