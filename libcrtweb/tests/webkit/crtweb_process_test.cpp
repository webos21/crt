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
// Children are found by reading /proc on Linux, by asking ps(1) on macOS (no /proc) and from a Toolhelp
// snapshot on Windows.

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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
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

#if defined(WTF_CRT_COFF_ASM)
// Windows (the persona defines WTF_CRT_COFF_ASM): no /proc and no ps(1); the process list comes from
// the Toolhelp snapshot, declared here because a CRT program does not see the Win32 headers.
extern "C" {
struct CrtProcessEntry {
    unsigned int size, usage, processId;
    uintptr_t defaultHeapId;
    unsigned int moduleId, threads, parentProcessId;
    int priorityClassBase;
    unsigned int flags;
    char exeFile[260];
};
void* __stdcall CreateToolhelp32Snapshot(unsigned int flags, unsigned int processId);
int __stdcall Process32First(void* snapshot, CrtProcessEntry* entry);
int __stdcall Process32Next(void* snapshot, CrtProcessEntry* entry);
int __stdcall CloseHandle(void* handle);
void* __stdcall OpenProcess(unsigned int access, int inherit, unsigned int processId);
unsigned int __stdcall WaitForSingleObject(void* handle, unsigned int milliseconds);
}

// A terminated process stays in the snapshot while anyone still holds a handle to it (the CRT's reaper
// does until waitpid); it is no longer a live child.
static bool processExited(unsigned int pid)
{
    void* process = OpenProcess(0x00100000 /* SYNCHRONIZE */, 0, pid);
    if (!process)
        return true;
    bool exited = WaitForSingleObject(process, 0) == 0 /* WAIT_OBJECT_0 */;
    CloseHandle(process);
    return exited;
}

// Children of this process whose executable is named `name` (with or without ".exe").
static std::vector<pid_t> childrenNamed(const char* name)
{
    std::vector<pid_t> result;
    void* snapshot = CreateToolhelp32Snapshot(0x2 /* TH32CS_SNAPPROCESS */, 0);
    if (snapshot == reinterpret_cast<void*>(-1))
        return result;
    std::string wanted = name;
    CrtProcessEntry entry;
    entry.size = sizeof entry;
    for (int more = Process32First(snapshot, &entry); more; more = Process32Next(snapshot, &entry)) {
        if (static_cast<pid_t>(entry.parentProcessId) != getpid())
            continue;
        if (processExited(entry.processId))
            continue;
        std::string executable = entry.exeFile;
        for (auto& c : executable)
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        std::string lowered = wanted;
        for (auto& c : lowered)
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        if (executable == lowered || executable == lowered + ".exe")
            result.push_back(static_cast<pid_t>(entry.processId));
    }
    CloseHandle(snapshot);
    return result;
}
#elif defined(CRT_TARGET_OS_MACOS)
// Children of this process whose executable is named `name`, from `ps -axo pid=,ppid=,comm=`.
static std::vector<pid_t> childrenNamed(const char* name)
{
    std::vector<pid_t> result;
    FILE* pipe = popen("ps -axo pid=,ppid=,comm=", "r");
    if (!pipe)
        return result;
    char line[4096];
    while (fgets(line, sizeof line, pipe)) {
        int pid = 0, ppid = 0, offset = 0;
        if (sscanf(line, " %d %d %n", &pid, &ppid, &offset) < 2 || ppid != getpid())
            continue;
        std::string executable = line + offset;
        while (!executable.empty() && (executable.back() == '\n' || executable.back() == ' '))
            executable.pop_back();
        size_t slash = executable.rfind('/');
        if (executable.substr(slash == std::string::npos ? 0 : slash + 1) == name)
            result.push_back(static_cast<pid_t>(pid));
    }
    pclose(pipe);
    return result;
}
#else
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

#endif

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
    setvbuf(stdout, nullptr, _IOLBF, 0); // progress lines survive a hang that the harness kills
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
    // The reaper retires the process asynchronously to the crash notification; give it a moment.
    bool gone = false;
    for (int attempt = 0; attempt < 50 && !gone; ++attempt) {
        auto remaining = childrenNamed("WebProcess");
        gone = true;
        for (pid_t pid : remaining)
            gone = gone && pid != webPid;
        if (!gone)
            usleep(100000);
        else
            printf("crtweb_process_test: web process gone after %d ms\n", attempt * 100);
    }
    check(gone, "the killed web process is gone");

    if (failures)
        return 1;
    printf("crtweb_process_test: ok\n");
    return 0;
}
