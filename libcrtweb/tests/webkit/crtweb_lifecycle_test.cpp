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
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <map>
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

#if defined(WTF_CRT_COFF_ASM)
// Windows (the persona defines WTF_CRT_COFF_ASM): no /proc. Processes and their modules come from the
// Toolhelp snapshot, memory and handle counts from kernel32; declared here because a CRT program does not
// see the Win32 headers.
extern "C" {
struct CrtProcessEntry {
    unsigned int size, usage, processId;
    uintptr_t defaultHeapId;
    unsigned int moduleId, threads, parentProcessId;
    int priorityClassBase;
    unsigned int flags;
    char exeFile[260];
};
struct CrtModuleEntry {
    unsigned int size, moduleId, processId, globalUsage, processUsage;
    unsigned char* baseAddress;
    unsigned int baseSize;
    void* module;
    char name[256];
    char exePath[260];
};
struct CrtMemoryCounters {
    unsigned int size, pageFaults;
    uintptr_t peakWorkingSet, workingSet, quotaPeakPaged, quotaPaged, quotaPeakNonPaged, quotaNonPaged, pagefile,
        peakPagefile;
};
void* __stdcall CreateToolhelp32Snapshot(unsigned int flags, unsigned int processId);
int __stdcall Process32First(void* snapshot, CrtProcessEntry* entry);
int __stdcall Process32Next(void* snapshot, CrtProcessEntry* entry);
int __stdcall Module32First(void* snapshot, CrtModuleEntry* entry);
int __stdcall Module32Next(void* snapshot, CrtModuleEntry* entry);
int __stdcall CloseHandle(void* handle);
void* __stdcall OpenProcess(unsigned int access, int inherit, unsigned int processId);
unsigned int __stdcall WaitForSingleObject(void* handle, unsigned int milliseconds);
void* __stdcall GetCurrentProcess(void);
int __stdcall K32GetProcessMemoryInfo(void* process, CrtMemoryCounters* counters, unsigned int size);
int __stdcall GetProcessHandleCount(void* process, unsigned int* count);
}

static bool processExited(unsigned int pid)
{
    void* process = OpenProcess(0x00100000 /* SYNCHRONIZE */, 0, pid);
    if (!process)
        return true;
    bool exited = WaitForSingleObject(process, 0) == 0 /* WAIT_OBJECT_0 */;
    CloseHandle(process);
    return exited;
}

static long residentKiB()
{
    CrtMemoryCounters counters = { };
    counters.size = sizeof counters;
    if (!K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof counters))
        return -1;
    return static_cast<long>(counters.workingSet / 1024);
}

static long ownThreads()
{
    long result = -1;
    void* snapshot = CreateToolhelp32Snapshot(0x2 /* TH32CS_SNAPPROCESS */, 0);
    if (snapshot == reinterpret_cast<void*>(-1))
        return result;
    CrtProcessEntry entry;
    entry.size = sizeof entry;
    for (int more = Process32First(snapshot, &entry); more; more = Process32Next(snapshot, &entry)) {
        if (static_cast<pid_t>(entry.processId) == getpid()) {
            result = entry.threads;
            break;
        }
    }
    CloseHandle(snapshot);
    return result;
}

// Diagnostic (CRTWEB_LIFECYCLE_HANDLES=1): histogram of the kernel object types of this process's handles.
extern "C" void* __stdcall GetModuleHandleA(const char* name);
extern "C" void* __stdcall GetProcAddress(void* module, const char* name);
extern "C" unsigned int __stdcall GetFileType(void*);
extern "C" unsigned int __stdcall GetFinalPathNameByHandleA(void*, char*, unsigned int, unsigned int);
extern "C" int __stdcall GetNamedPipeInfo(void*, unsigned int*, unsigned int*, unsigned int*, unsigned int*);
typedef long(__stdcall* NtQueryObjectFunction)(void*, int, void*, unsigned int, unsigned int*);
static void dumpHandleTypes(int iteration)
{
    if (!getenv("CRTWEB_LIFECYCLE_HANDLES"))
        return;
    auto NtQueryObject = reinterpret_cast<NtQueryObjectFunction>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryObject"));
    if (!NtQueryObject)
        return;
    std::map<std::string, int> histogram;
    for (uintptr_t h = 4; h < 0x8000; h += 4) {
        alignas(8) unsigned char buffer[512];
        unsigned int returned = 0;
        if (NtQueryObject(reinterpret_cast<void*>(h), 2 /* ObjectTypeInformation */, buffer, sizeof buffer, &returned) < 0)
            continue;
        unsigned short length = *reinterpret_cast<unsigned short*>(buffer);
        const unsigned short* text = *reinterpret_cast<unsigned short**>(buffer + 8); // UTF-16: wchar_t is 4 bytes here
        std::string name;
        for (unsigned short i = 0; i < length / 2; ++i)
            name.push_back(static_cast<char>(text[i]));
        ++histogram[name];
        if (name == "File") {
            char path[300] = "";
            unsigned int type = GetFileType(reinterpret_cast<void*>(h));
            if (type == 1)
                GetFinalPathNameByHandleA(reinterpret_cast<void*>(h), path, sizeof path, 0);
            char detail[200] = "";
            if (type == 3) {
                unsigned int flags = 0, outSize = 0, inSize = 0, instances = 0;
                if (GetNamedPipeInfo(reinterpret_cast<void*>(h), &flags, &outSize, &inSize, &instances))
                    snprintf(detail, sizeof detail, "pipe flags=%u out=%u in=%u max=%u", flags, outSize, inSize, instances);
                else {
                    auto getsockname = reinterpret_cast<int(__stdcall*)(uintptr_t, void*, int*)>(GetProcAddress(GetModuleHandleA("ws2_32.dll"), "getsockname"));
                    auto getpeername = reinterpret_cast<int(__stdcall*)(uintptr_t, void*, int*)>(GetProcAddress(GetModuleHandleA("ws2_32.dll"), "getpeername"));
                    char name[128] = { }, peer[128] = { };
                    int nameLength = sizeof name, peerLength = sizeof peer;
                    int a = getsockname ? getsockname(h, name, &nameLength) : -9;
                    int b = getpeername ? getpeername(h, peer, &peerLength) : -9;
                    snprintf(detail, sizeof detail, "socket? name=%d(%s) peer=%d(%s)", a, name + 2, b, peer + 2);
                }
            }
            printf("  FILE h=%llx type=%u %s %s\n", (unsigned long long)h, type, path, detail);
        }
    }
    printf("HANDLES iteration %d:", iteration);
    for (auto& entry : histogram)
        printf(" %s=%d", entry.first.c_str(), entry.second);
    printf("\n");
}

static unsigned openHandles()
{
    unsigned count = 0;
    GetProcessHandleCount(GetCurrentProcess(), &count);
    return count;
}

// Live children of this process, named without ".exe".
static std::vector<std::pair<pid_t, std::string>> children()
{
    std::vector<std::pair<pid_t, std::string>> result;
    void* snapshot = CreateToolhelp32Snapshot(0x2 /* TH32CS_SNAPPROCESS */, 0);
    if (snapshot == reinterpret_cast<void*>(-1))
        return result;
    CrtProcessEntry entry;
    entry.size = sizeof entry;
    for (int more = Process32First(snapshot, &entry); more; more = Process32Next(snapshot, &entry)) {
        if (static_cast<pid_t>(entry.parentProcessId) != getpid() || processExited(entry.processId))
            continue;
        std::string name = entry.exeFile;
        if (name.size() > 4 && !strcasecmp(name.c_str() + name.size() - 4, ".exe"))
            name.resize(name.size() - 4);
        result.emplace_back(static_cast<pid_t>(entry.processId), name);
    }
    CloseHandle(snapshot);
    return result;
}

static void printMappedFiles(pid_t pid, const std::string& name)
{
    std::set<std::string> files;
    void* snapshot = CreateToolhelp32Snapshot(0x8 | 0x10 /* TH32CS_SNAPMODULE | SNAPMODULE32 */, pid);
    if (snapshot != reinterpret_cast<void*>(-1)) {
        CrtModuleEntry entry;
        entry.size = sizeof entry;
        for (int more = Module32First(snapshot, &entry); more; more = Module32Next(snapshot, &entry)) {
            std::string path = entry.exePath;
            for (auto& c : path)
                if (c == '\\')
                    c = '/';
            files.insert(path);
        }
        CloseHandle(snapshot);
    }
    for (auto& file : files)
        printf("MAPPED %s %s\n", name.c_str(), file.c_str());
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
#endif

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

#if defined(WTF_CRT_COFF_ASM)
        samples.push_back({ residentKiB(), ownThreads(), openHandles() });
        dumpHandleTypes(i + 1);
#else
        samples.push_back({ statusValue("VmRSS"), statusValue("Threads"), openFileDescriptors() });
#endif
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
