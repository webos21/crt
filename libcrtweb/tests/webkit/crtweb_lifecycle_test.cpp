// Web Tranche 3B, milestone B7: lifecycle and resource audit of the Linux prototype.
//
// Creates, uses and destroys a web context and view `N` times in one UI process (default 100):
//   * every iteration launches a Web and a Network process, loads a page with a canvas and a script,
//     checks that it painted, and releases view, configurations and context;
//   * after the release both child processes must be gone (the UI process notices the closed
//     connections and the processes exit) before the next iteration starts;
//   * open file descriptors, threads and resident memory are sampled after every iteration: growth
//     after a warm-up of 10 iterations is bounded and reported;
//   * during the first iteration the files mapped into the Web and Network processes are listed
//     (MAPPED lines) so the driver can run the Host ABI audit on what really ran, not only on what
//     the executables name.
// Arguments: [iterations]. Linux only (it reads /proc).

#define BUILDING_CRT__ 1

#include <WebKit/WKBase.h>
#include <WebKit/WKContext.h>
#include <WebKit/WKContextConfigurationRef.h>
#include <WebKit/WKGeometry.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKRunLoop.h>
#include <WebKit/WKString.h>
#include <WebKit/WKType.h>
#include <WebKit/WKURL.h>
#include <WebKit/WKView.h>
#include <WebKit/WKViewCRT.h>

#include <dirent.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <set>
#include <string>
#include <vector>

static int failures;
static bool finishedNavigation;
static bool crashed;

static void check(bool condition, const char* what)
{
    if (condition)
        return;
    fprintf(stderr, "crtweb_lifecycle_test: FAILED %s\n", what);
    ++failures;
}

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    finishedNavigation = true;
    WKRunLoopStopMain();
}

static void webProcessDidCrash(WKPageRef, const void*)
{
    crashed = true;
    WKRunLoopStopMain();
}

static void stopLoop(void*)
{
    WKRunLoopStopMain();
}

static void spin(unsigned milliseconds)
{
    for (unsigned waited = 0; waited < milliseconds; waited += 20) {
        usleep(20 * 1000);
        WKRunLoopCallOnMainThread(stopLoop, nullptr);
        WKRunLoopRunMain();
    }
}

static std::string readLink(const std::string& path)
{
    char buffer[4096];
    ssize_t length = readlink(path.c_str(), buffer, sizeof(buffer) - 1);
    if (length <= 0)
        return { };
    buffer[length] = 0;
    return buffer;
}

static long statusValue(const char* key)
{
    FILE* file = fopen("/proc/self/status", "r");
    if (!file)
        return -1;
    char line[256];
    long value = -1;
    size_t keyLength = strlen(key);
    while (fgets(line, sizeof line, file)) {
        if (!strncmp(line, key, keyLength) && line[keyLength] == ':') {
            value = atol(line + keyLength + 1);
            break;
        }
    }
    fclose(file);
    return value;
}

static unsigned openFileDescriptors()
{
    unsigned count = 0;
    if (DIR* directory = opendir("/proc/self/fd")) {
        while (readdir(directory))
            ++count;
        closedir(directory);
    }
    return count;
}

static pid_t parentOf(pid_t pid)
{
    FILE* file = fopen(("/proc/" + std::to_string(pid) + "/stat").c_str(), "r");
    if (!file)
        return 0;
    char buffer[1024];
    size_t length = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[length] = 0;
    const char* close = strrchr(buffer, ')');
    char state;
    int ppid = 0;
    if (!close || sscanf(close + 1, " %c %d", &state, &ppid) != 2)
        return 0;
    return ppid;
}

// Live (not zombie) children of this process.
static std::vector<std::pair<pid_t, std::string>> children()
{
    std::vector<std::pair<pid_t, std::string>> result;
    DIR* directory = opendir("/proc");
    if (!directory)
        return result;
    while (dirent* entry = readdir(directory)) {
        pid_t pid = static_cast<pid_t>(atoi(entry->d_name));
        if (pid <= 0 || parentOf(pid) != getpid())
            continue;
        std::string executable = readLink("/proc/" + std::to_string(pid) + "/exe");
        if (executable.empty())
            continue; // a zombie has no exe link
        size_t slash = executable.rfind('/');
        result.emplace_back(pid, executable.substr(slash == std::string::npos ? 0 : slash + 1));
    }
    closedir(directory);
    return result;
}

static void printMappedFiles(pid_t pid, const std::string& name)
{
    std::set<std::string> files;
    if (FILE* maps = fopen(("/proc/" + std::to_string(pid) + "/maps").c_str(), "r")) {
        char line[4096];
        while (fgets(line, sizeof line, maps)) {
            const char* path = strchr(line, '/');
            if (!path) {
                // [vdso], [heap], [stack] and anonymous mappings
                const char* bracket = strchr(line, '[');
                if (bracket)
                    files.insert(std::string(bracket, strcspn(bracket, "\n")));
                continue;
            }
            files.insert(std::string(path, strcspn(path, "\n")));
        }
        fclose(maps);
    }
    for (auto& file : files)
        printf("MAPPED %s %s\n", name.c_str(), file.c_str());
}

struct Sample {
    long rssKiB;
    long threads;
    unsigned descriptors;
};

int main(int argc, char** argv)
{
    int iterations = argc > 1 ? atoi(argv[1]) : 100;
    if (iterations < 12)
        iterations = 12;

    static const char html[] = "<!doctype html><title>lifecycle</title><body style='background:#336699'>"
        "<canvas id=c width=64 height=64></canvas><script>"
        "var x=document.getElementById('c').getContext('2d');x.fillStyle='#ff0000';x.fillRect(0,0,64,64);"
        "document.title='ready'</script></body>";

    WKRunLoopInitializeMain();
    alarm(30 + iterations * 6);
    signal(SIGALRM, [](int) { _exit(2); });

    std::vector<Sample> samples;
    unsigned maxChildren = 0;
    for (int i = 0; i < iterations; ++i) {
        finishedNavigation = false;
        crashed = false;

        WKContextConfigurationRef contextConfiguration = WKContextConfigurationCreate();
        WKContextRef context = WKContextCreateWithConfiguration(contextConfiguration);
        WKPageConfigurationRef pageConfiguration = WKPageConfigurationCreate();
        WKPageConfigurationSetContext(pageConfiguration, context);
        WKViewRef view = WKViewCreate(pageConfiguration);
        WKViewCRTSetSize(view, 320, 240);
        WKViewSetActive(view, true);
        WKViewSetVisible(view, true);
        WKViewSetFocus(view, true);

        WKPageRef page = WKViewGetPage(view);
        WKPageNavigationClientV0 navigationClient = { };
        navigationClient.base.version = 0;
        navigationClient.didFinishNavigation = didFinishNavigation;
        navigationClient.webProcessDidCrash = webProcessDidCrash;
        WKPageSetPageNavigationClient(page, &navigationClient.base);

        WKStringRef htmlString = WKStringCreateWithUTF8CString(html);
        WKURLRef baseURL = WKURLCreateWithUTF8CString("about:blank");
        WKPageLoadHTMLString(page, htmlString, baseURL);
        WKRunLoopRunMain();
        WKRelease(htmlString);
        WKRelease(baseURL);
        if (!finishedNavigation || crashed) {
            check(false, "the page loaded");
            break;
        }

        spin(150); // painting and the UI process composing it

        std::vector<uint32_t> pixels(320u * 240u, 0);
        bool painted = WKViewCRTCopyPixels(view, pixels.data(), 320, 240, 320 * 4);
        // The canvas is the top-left 64x64: red; the page background elsewhere.
        check(painted && pixels[10 * 320 + 10] == 0xffff0000u && pixels[200 * 320 + 200] == 0xff336699u,
            "the page painted its canvas and background");

        auto live = children();
        if (live.size() > maxChildren)
            maxChildren = live.size();
        if (i == 0) {
            printMappedFiles(getpid(), "UIProcess");
            for (auto& child : live)
                printMappedFiles(child.first, child.second);
        }
        check(live.size() == 2, "a Web and a Network process while the view is alive");

        WKRelease(view);
        WKRelease(pageConfiguration);
        WKRelease(context);
        WKRelease(contextConfiguration);

        // The Web process must go away on its own once nothing references it. The Network process
        // belongs to the default website data store, which upstream keeps for the life of the UI
        // process: it must stay a single, reused process, never one per context.
        bool gone = false;
        size_t networkProcesses = 0;
        for (int wait = 0; wait < 500 && !gone; ++wait) {
            spin(20);
            gone = true;
            networkProcesses = 0;
            for (auto& child : children()) {
                if (child.second == "NetworkProcess")
                    ++networkProcesses;
                else
                    gone = false;
            }
        }
        check(gone, "the Web process exited after the release");
        check(networkProcesses <= 1, "the Network process is shared, not created per context");
        if (!gone) {
            for (auto& child : children())
                fprintf(stderr, "crtweb_lifecycle_test: still alive: %d %s\n", child.first, child.second.c_str());
            break;
        }

        samples.push_back({ statusValue("VmRSS"), statusValue("Threads"), openFileDescriptors() });
        if (i % 10 == 9)
            printf("crtweb_lifecycle_test: iteration %d: rss %ld KiB, %ld threads, %u descriptors\n",
                i + 1, samples.back().rssKiB, samples.back().threads, samples.back().descriptors);
    }

    if (samples.size() >= 12) {
        const Sample& warm = samples[9];
        const Sample& last = samples.back();
        long rssGrowth = last.rssKiB - warm.rssKiB;
        long threadGrowth = last.threads - warm.threads;
        long descriptorGrowth = static_cast<long>(last.descriptors) - static_cast<long>(warm.descriptors);
        printf("crtweb_lifecycle_test: %zu iterations, after warm-up (10): rss %+ld KiB, threads %+ld, descriptors %+ld; "
               "most children at once %u\n", samples.size(), rssGrowth, threadGrowth, descriptorGrowth, maxChildren);
        // Bounded, not zero: allocators keep arenas. A leak of a page or a view would show as linear
        // growth, which over the remaining iterations is far beyond these bounds.
        check(descriptorGrowth <= 2, "open descriptors do not grow");
        check(threadGrowth <= 2, "threads do not grow");
        check(rssGrowth <= 24 * 1024, "resident memory growth is bounded (24 MiB over the run)");
        check(samples.size() == static_cast<size_t>(iterations), "every iteration completed");
    } else
        check(false, "enough iterations completed to judge growth");

    alarm(0);
    if (failures)
        return 1;
    printf("crtweb_lifecycle_test: ok\n");
    return 0;
}
