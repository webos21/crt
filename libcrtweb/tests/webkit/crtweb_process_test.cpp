// Web Tranche 3B, milestone B4: the WebKit multi-process model on CRT.
//
// This program is a UI process. It creates a web context and a view through WebKit's C API (the
// PlayStation-shaped one PlatformCRT exposes), and checks, in order:
//   1. creating the view launches a Web process and a Network process (separate pids, children of
//      this process, running the CRT process executables);
//   2. a navigation round-trips: loadHTML goes UI -> Web process, and didFinishNavigation comes
//      back over the IPC connection (a SOCK_SEQPACKET pair with SCM_RIGHTS);
//   3. killing the Web process is detected through the closed connection: the navigation client's
//      webProcessDidCrash fires, and the dead pid is no longer a child.
// It never paints: milestone B5 does.
//
// Linux-only as written (it finds the Network process by reading /proc).

#define BUILDING_CRT__ 1

#include <WebKit/WKBase.h>
#include <WebKit/WKContext.h>
#include <WebKit/WKContextConfigurationRef.h>
#include <WebKit/WKGeometry.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKRunLoop.h>
#include <WebKit/WKString.h>
#include <WebKit/WKURL.h>
#include <WebKit/WKView.h>

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <vector>

static int failures;
static bool finishedNavigation;
static bool webProcessCrashed;
static bool timedOut;

static void check(bool condition, const char* what)
{
    if (condition)
        return;
    fprintf(stderr, "crtweb_process_test: FAILED %s\n", what);
    ++failures;
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
    if (!close)
        return 0;
    char state;
    int ppid = 0;
    if (sscanf(close + 1, " %c %d", &state, &ppid) != 2)
        return 0;
    return ppid;
}

// Children of this process whose executable is named `name`.
static std::vector<pid_t> childrenNamed(const char* name)
{
    std::vector<pid_t> result;
    DIR* directory = opendir("/proc");
    if (!directory)
        return result;
    while (dirent* entry = readdir(directory)) {
        pid_t pid = static_cast<pid_t>(atoi(entry->d_name));
        if (pid <= 0 || parentOf(pid) != getpid())
            continue;
        std::string executable = readLink("/proc/" + std::to_string(pid) + "/exe");
        size_t slash = executable.rfind('/');
        if (executable.substr(slash == std::string::npos ? 0 : slash + 1) == name)
            result.push_back(pid);
    }
    closedir(directory);
    return result;
}

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    finishedNavigation = true;
    WKRunLoopStopMain();
}

static void webProcessDidCrash(WKPageRef, const void*)
{
    webProcessCrashed = true;
    WKRunLoopStopMain();
}

static void timeoutFired(void*)
{
    timedOut = true;
    WKRunLoopStopMain();
}

int main()
{
    WKRunLoopInitializeMain();

    WKContextConfigurationRef contextConfiguration = WKContextConfigurationCreate();
    WKContextRef context = WKContextCreateWithConfiguration(contextConfiguration);
    WKPageConfigurationRef pageConfiguration = WKPageConfigurationCreate();
    WKPageConfigurationSetContext(pageConfiguration, context);

    WKViewRef view = WKViewCreate(pageConfiguration);
    WKViewSetSize(view, WKSizeMake(640, 480));
    WKPageRef page = WKViewGetPage(view);

    WKPageNavigationClientV0 navigationClient = { };
    navigationClient.base.version = 0;
    navigationClient.didFinishNavigation = didFinishNavigation;
    navigationClient.webProcessDidCrash = webProcessDidCrash;
    WKPageSetPageNavigationClient(page, &navigationClient.base);

    // A watchdog so a hang is a failure, not a stuck test.
    auto armWatchdog = [] {
        WKRunLoopCallOnMainThread(timeoutFired, nullptr);
    };
    (void)armWatchdog;

    WKStringRef html = WKStringCreateWithUTF8CString("<!doctype html><title>crt</title><p>B4</p>");
    WKURLRef base = WKURLCreateWithUTF8CString("about:blank");
    WKPageLoadHTMLString(page, html, base);

    // The run loop is stopped by the navigation client or by the alarm below.
    alarm(60);
    signal(SIGALRM, [](int) { _exit(2); });
    WKRunLoopRunMain();
    alarm(0);

    check(finishedNavigation, "navigation finished (message round trip)");

    pid_t webPid = static_cast<pid_t>(WKPageGetProcessIdentifier(page));
    check(webPid > 0 && webPid != getpid(), "web process has its own pid");
    auto webChildren = childrenNamed("WebProcess");
    auto networkChildren = childrenNamed("NetworkProcess");
    printf("crtweb_process_test: web pid %d, WebProcess children %zu, NetworkProcess children %zu\n",
        static_cast<int>(webPid), webChildren.size(), networkChildren.size());
    check(webChildren.size() == 1 && webChildren[0] == webPid, "one WebProcess child, the page's");
    check(networkChildren.size() == 1, "one NetworkProcess child");

    // Kill the Web process and wait for the UI process to notice.
    finishedNavigation = false;
    kill(webPid, SIGKILL);
    alarm(60);
    WKRunLoopRunMain();
    alarm(0);
    check(webProcessCrashed, "killing the web process is detected (webProcessDidCrash)");
    check(childrenNamed("WebProcess").empty() || childrenNamed("WebProcess")[0] != webPid, "the killed web process is gone");

    if (failures)
        return 1;
    printf("crtweb_process_test: ok\n");
    return 0;
}
