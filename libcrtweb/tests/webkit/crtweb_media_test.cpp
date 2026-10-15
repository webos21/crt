// Web Tranche 8, gate 8B: HTML media playback through the CRT playback engine (crtmedia).
//
// Includes only <crtweb/crtweb.h>. Drives <video> and <audio> elements with script: canPlayType,
// metadata, play/pause/seek, ended, volume/mute, pixels (a canvas readback of the video frame), audio-only
// media, local files and the browser's own HTTP loader, unsupported/missing/garbage sources, and
// repeated lifetimes with a thread/descriptor bound on the Web process.
//
// Arguments: assets-directory (libcrtmedia/assets)

#include "crtweb/crtweb.h"
#include "../common/crtweb_test_http_server.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <chrono>
#include <functional>
#include <string>
#include <vector>

static int failures;

static bool gTrace;

static long long elapsedMs()
{
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
}

#define CHECK(condition, message)                                                          \
    do {                                                                                   \
        if (!(condition)) {                                                                \
            fprintf(stderr, "crtweb_media_test: FAILED %s (line %d)\n", message, __LINE__); \
            ++failures;                                                                    \
        } else if (gTrace)                                                                 \
            fprintf(stderr, "crtweb_media_test: ok %s (%lld ms)\n", message, elapsedMs());     \
    } while (0)

static crtweb_runtime* gRuntime;
static crtweb_view* gView;

static bool pumpUntil(const std::function<bool()>& done, unsigned timeoutMs = 20000)
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

static std::string js(const std::string& script)
{
    struct Result { bool done = false; int error = 0; std::string text; } result;
    crtweb_view_evaluate_script(gView, script.c_str(), [](crtweb_view*, const char* text, int error, void* data) {
        auto* r = static_cast<Result*>(data);
        r->done = true;
        r->error = error;
        r->text = text ? text : "";
    }, &result);
    pumpUntil([&] { return result.done; }, 15000);
    return result.error ? "<error>" : result.text;
}

static double num(const std::string& script)
{
    std::string text = js(script);
    return strtod(text.c_str(), nullptr);
}

static void writeFile(const std::string& path, const std::string& content)
{
    FILE* file = fopen(path.c_str(), "wb");
    if (!file) {
        fprintf(stderr, "crtweb_media_test: cannot write %s\n", path.c_str());
        exit(2);
    }
    fwrite(content.data(), 1, content.size(), file);
    fclose(file);
}

static std::string stereoPCMFixture()
{
    // A quiet 300-ms triangular tone in the common native ALSA format. Decoder-native mono/float
    // can be accepted by a Pulse null sink even when no hardware receives PCM; exercise an actual
    // hardware-facing format too. Generated test data, not a patched upstream asset.
    const uint32_t rate = 48000, frames = rate * 3 / 10, bytes = frames * 4;
    std::string wav = "RIFF";
    auto append = [&](uint32_t value, unsigned count) {
        for (unsigned i = 0; i < count; ++i) wav += static_cast<char>((value >> (i * 8)) & 255);
    };
    append(bytes + 36, 4); wav += "WAVEfmt "; append(16, 4);
    append(1, 2); append(2, 2); append(rate, 4); append(rate * 4, 4);
    append(4, 2); append(16, 2); wav += "data"; append(bytes, 4);
    for (uint32_t i = 0; i < frames; ++i) {
        int phase = static_cast<int>(i % 120); // 400 Hz, peak 2048/32768
        int16_t sample = static_cast<int16_t>((phase < 60 ? phase - 30 : 90 - phase) * 2048 / 30);
        append(static_cast<uint16_t>(sample), 2); append(static_cast<uint16_t>(sample), 2);
    }
    return wav;
}

static std::string fileURLFor(const std::string& path)
{
    std::string normalized = path;
    for (char& c : normalized)
        if (c == '\\')
            c = '/';
#if defined(CRT_TARGET_OS_WINDOWS) || defined(WTF_CRT_COFF_ASM)
    // WebKit uses its POSIX persona; the CRT exposes a Windows drive as /c/..., as getcwd does.
    if (normalized.size() > 2 && normalized[1] == ':') {
        char drive = normalized[0];
        if (drive >= 'A' && drive <= 'Z')
            drive += 'a' - 'A';
        normalized = std::string("/") + drive + normalized.substr(2);
    }
#endif
    return "file://" + normalized;
}

static bool loadPage(const std::string& url)
{
    // A unique query makes every navigation a new document, so the readiness checks below cannot be
    // satisfied by the page that is being replaced.
    static unsigned counter;
    std::string tagged = url + "?n=" + std::to_string(++counter);
    if (crtweb_view_load_url(gView, tagged.c_str()) != CRTWEB_OK)
        return false;
    std::string expected = "?n=" + std::to_string(counter);
    return pumpUntil([&] {
        return js("location.search") == expected && js("document.readyState") == "complete" && js("typeof window.log") == "object";
    });
}

// The page's media elements record their events in window.log.
static std::string pageFor(const std::string& element, const std::string& source)
{
    return "<!doctype html><title>media</title><body style='margin:0;background:#000'>"
           + element + "<script>"
           "window.log=[];var m=document.getElementById('m');"
           "['loadedmetadata','loadeddata','canplay','playing','pause','seeked','ended','error','emptied'].forEach(function(n){m.addEventListener(n,function(){window.log.push(n);});});"
           "</script>";
    (void)source;
}

// The video is the whole 320x240 view; its picture is read from the view (a canvas readback of a
// file: video is cross-origin tainted, and the view is the real compositor contract anyway).
static std::string viewSignature(unsigned* nonBlack = nullptr)
{
    crtweb_runtime_pump(gRuntime, 150);
    std::vector<uint32_t> pixels(320 * 240);
    if (crtweb_view_copy_frame(gView, pixels.data(), 320, 240, 320 * 4, nullptr) != CRTWEB_OK)
        return "<no frame>";
    uint64_t hash = 1469598103934665603ull;
    unsigned lit = 0;
    for (uint32_t pixel : pixels) {
        hash = (hash ^ pixel) * 1099511628211ull;
        if ((pixel & 0xffffff) != 0)
            ++lit;
    }
    if (nonBlack)
        *nonBlack = lit;
    return std::to_string(hash);
}

static bool logHas(const char* event)
{
    return js("window.log.indexOf('" + std::string(event) + "') >= 0") == "true";
}

#if defined(CRT_TARGET_OS_WINDOWS) || defined(WTF_CRT_COFF_ASM)
// Acceptance-only process observations. Win32 types stay out of the public CRT surface.
extern "C" {
struct ProcessEntry {
    unsigned size, usage, pid;
    uintptr_t heap;
    unsigned module, threads, parent;
    int priority;
    unsigned flags;
    char executable[260];
};
struct ModuleEntry {
    unsigned size, module, pid, globalUsage, processUsage;
    unsigned char* base;
    unsigned baseSize;
    void* handle;
    char name[256];
    char path[260];
};
void* __stdcall CreateToolhelp32Snapshot(unsigned, unsigned);
int __stdcall Process32First(void*, ProcessEntry*);
int __stdcall Process32Next(void*, ProcessEntry*);
int __stdcall Module32First(void*, ModuleEntry*);
int __stdcall Module32Next(void*, ModuleEntry*);
void* __stdcall OpenProcess(unsigned, int, unsigned);
unsigned __stdcall WaitForSingleObject(void*, unsigned);
int __stdcall CloseHandle(void*);
}

static std::vector<pid_t> childrenNamed(const char* name)
{
    std::vector<pid_t> result;
    void* snapshot = CreateToolhelp32Snapshot(0x2 /* TH32CS_SNAPPROCESS */, 0);
    if (snapshot == reinterpret_cast<void*>(-1))
        return result;
    ProcessEntry entry { };
    entry.size = sizeof entry;
    for (int more = Process32First(snapshot, &entry); more; more = Process32Next(snapshot, &entry)) {
        if (entry.parent != static_cast<unsigned>(getpid()) || !strstr(entry.executable, name))
            continue;
        void* process = OpenProcess(0x00100000 /* SYNCHRONIZE */, 0, entry.pid);
        bool alive = process && WaitForSingleObject(process, 0) != 0;
        if (process)
            CloseHandle(process);
        if (alive)
            result.push_back(static_cast<pid_t>(entry.pid));
    }
    CloseHandle(snapshot);
    return result;
}

static void printMappedFiles(pid_t pid)
{
    void* snapshot = CreateToolhelp32Snapshot(0x8 | 0x10 /* TH32CS_SNAPMODULE | SNAPMODULE32 */, pid);
    if (snapshot == reinterpret_cast<void*>(-1))
        return;
    std::vector<std::string> files;
    ModuleEntry entry { };
    entry.size = sizeof entry;
    for (int more = Module32First(snapshot, &entry); more; more = Module32Next(snapshot, &entry)) {
        std::string path = entry.path;
        for (char& c : path)
            if (c == '\\')
                c = '/';
        bool known = false;
        for (const std::string& file : files)
            known = known || file == path;
        if (!known)
            files.push_back(path);
    }
    CloseHandle(snapshot);
    for (const std::string& file : files)
        printf("MAPPED WebProcess %s\n", file.c_str());
}

static long threadsOf(pid_t pid)
{
    long result = -1;
    void* snapshot = CreateToolhelp32Snapshot(0x2 /* TH32CS_SNAPPROCESS */, 0);
    if (snapshot == reinterpret_cast<void*>(-1))
        return result;
    ProcessEntry entry { };
    entry.size = sizeof entry;
    for (int more = Process32First(snapshot, &entry); more; more = Process32Next(snapshot, &entry))
        if (entry.pid == static_cast<unsigned>(pid)) {
            result = entry.threads;
            break;
        }
    CloseHandle(snapshot);
    return result;
}
#elif defined(CRT_TARGET_OS_MACOS) || defined(WTF_CRT_MACHO_ASM)
// macOS has no /proc: libSystem's libproc lists children and reads the task info, and the files the Web
// process has mapped come from vmmap(1) (declared here; none of this is part of the target ABI).
extern "C" int proc_listchildpids(pid_t, void*, int);
extern "C" int proc_name(int, void*, uint32_t);
extern "C" int proc_pidinfo(int, int, uint64_t, void*, int);

struct TaskInfo {
    uint64_t virtualSize, residentSize, totalUser, totalSystem, threadsUser, threadsSystem;
    int32_t policy, faults, pageIns, copyOnWriteFaults, messagesSent, messagesReceived,
        machCalls, unixCalls, switches, threads, runningThreads, priority;
};

static std::vector<pid_t> childrenNamed(const char* name)
{
    std::vector<pid_t> result;
    pid_t pids[256] { };
    int count = proc_listchildpids(getpid(), pids, sizeof pids); // a number of pids, not of bytes
    for (int i = 0; i < count && i < 256; ++i) {
        char comm[256] { };
        if (pids[i] > 0 && proc_name(pids[i], comm, sizeof comm) > 0 && strstr(comm, name))
            result.push_back(pids[i]);
    }
    return result;
}

static void printMappedFiles(pid_t pid)
{
    std::vector<std::string> files;
    std::string command = "/usr/bin/vmmap " + std::to_string(pid) + " 2>/dev/null";
    if (FILE* maps = popen(command.c_str(), "r")) {
        char line[4096];
        while (fgets(line, sizeof line, maps)) {
            // The mapped file is the absolute path at the end of a region line, after a space.
            const char* name = strstr(line, " /");
            if (!name)
                continue;
            std::string file(name + 1, strcspn(name + 1, "\n"));
            if (file.find("/*/") != std::string::npos)
                continue;
            bool known = false;
            for (const std::string& each : files)
                known = known || each == file;
            if (!known)
                files.push_back(file);
        }
        pclose(maps);
    }
    for (const std::string& file : files)
        printf("MAPPED WebProcess %s\n", file.c_str());
}

static long threadsOf(pid_t pid)
{
    TaskInfo info { };
    if (proc_pidinfo(pid, 4 /* PROC_PIDTASKINFO */, 0, &info, sizeof info) != static_cast<int>(sizeof info))
        return -1;
    return info.threads;
}
#else
static std::vector<pid_t> childrenNamed(const char* name)
{
    std::vector<pid_t> result;
    if (DIR* directory = opendir("/proc")) {
        while (dirent* entry = readdir(directory)) {
            pid_t pid = atoi(entry->d_name);
            if (pid <= 0)
                continue;
            char path[64], buffer[512];
            snprintf(path, sizeof path, "/proc/%d/stat", pid);
            FILE* file = fopen(path, "r");
            if (!file)
                continue;
            size_t length = fread(buffer, 1, sizeof buffer - 1, file);
            fclose(file);
            buffer[length] = '\0';
            char* open = strchr(buffer, '(');
            char* close = strrchr(buffer, ')');
            if (!open || !close)
                continue;
            std::string comm(open + 1, close - open - 1);
            int ppid = 0;
            sscanf(close + 2, "%*c %d", &ppid);
            if (ppid == getpid() && comm.find(name) != std::string::npos)
                result.push_back(pid);
        }
        closedir(directory);
    }
    return result;
}

// The files the Web process has mapped (the driver audits them against the Host ABI roots).
static void printMappedFiles(pid_t pid)
{
    std::vector<std::string> files;
    char path[64], line[4096];
    snprintf(path, sizeof path, "/proc/%d/maps", pid);
    if (FILE* maps = fopen(path, "r")) {
        while (fgets(line, sizeof line, maps)) {
            const char* name = strchr(line, '/');
            if (!name)
                continue;
            std::string file(name, strcspn(name, "\n"));
            bool known = false;
            for (const std::string& each : files)
                known = known || each == file;
            if (!known)
                files.push_back(file);
        }
        fclose(maps);
    }
    for (const std::string& file : files)
        printf("MAPPED WebProcess %s\n", file.c_str());
}

static long threadsOf(pid_t pid)
{
    char path[64], line[256];
    snprintf(path, sizeof path, "/proc/%d/status", pid);
    long threads = -1;
    if (FILE* file = fopen(path, "r")) {
        while (fgets(line, sizeof line, file)) {
            if (!strncmp(line, "Threads:", 8))
                threads = atol(line + 8);
        }
        fclose(file);
    }
    return threads;
}

#endif

int main(int argc, char** argv)
{
    // CRT startup supplies envp before main, but after Mach-O static initializers.
    gTrace = getenv("CRTWEB_MEDIA_TRACE") != nullptr;
    if (argc != 2) {
        fprintf(stderr, "usage: %s assets-directory\n", argv[0]);
        return 2;
    }
    std::string assets = argv[1];
    char cwd[4096];
    if (!getcwd(cwd, sizeof cwd))
        return 2;
    std::string directory = cwd;
    std::string assetURL = fileURLFor(assets);

    crtweb_runtime_config config = { };
    config.struct_size = sizeof config;
    config.application_name = "CrtWebMediaTest";
    CHECK(crtweb_runtime_create(&config, &gRuntime) == CRTWEB_OK, "runtime");
    crtweb_view_config viewConfig = { sizeof(crtweb_view_config), 320, 240, 100 };
    CHECK(crtweb_view_create(gRuntime, &viewConfig, &gView) == CRTWEB_OK, "view");

    // ---- video: metadata, play, pause, seek, ended ----
    writeFile(directory + "/crtweb_media_video.html",
        pageFor("<video id=m muted style='width:320px;height:240px' src='" + assetURL + "/test_video.mp4'></video>", ""));
    CHECK(loadPage(fileURLFor(directory + "/crtweb_media_video.html")), "video page");
    CHECK(js("document.createElement('video').canPlayType('video/mp4; codecs=\"avc1.42E01E, mp4a.40.2\"')") != "", "canPlayType h264+aac");
    CHECK(js("document.createElement('video').canPlayType('video/x-unknown')") == "", "canPlayType unknown");
    CHECK(js("document.createElement('video').canPlayType('video/mp4; codecs=\"vp09\"')") == "", "canPlayType unsupported codec");
    CHECK(pumpUntil([&] { return js("m.readyState") >= std::string("3"); }, 20000), "enough data");
    CHECK(logHas("loadedmetadata") && logHas("loadeddata"), "metadata and data events");
    double duration = num("m.duration");
    CHECK(duration > 0.8 && duration < 1.3, "duration is about one second");
    CHECK(num("m.videoWidth") == 64 && num("m.videoHeight") == 64, "video size");
    CHECK(js("String(m.paused)") == "true" && num("m.currentTime") == 0, "starts paused at zero");

    unsigned lit = 0;
    std::string firstSignature = viewSignature(&lit);
    if (gTrace) {
        fprintf(stderr, "trace: first signature %s, readyState %s, networkState %s, size %s, view frame:\n", firstSignature.c_str(),
            js("String(m.readyState)").c_str(), js("String(m.networkState)").c_str(), js("m.videoWidth + 'x' + m.videoHeight").c_str());
        std::vector<uint32_t> pixels(320 * 240);
        crtweb_runtime_pump(gRuntime, 100);
        crtweb_result r = crtweb_view_copy_frame(gView, pixels.data(), 320, 240, 320 * 4, nullptr);
        fprintf(stderr, "trace: copy_frame %d center %08x corner %08x\n", r, pixels[120 * 320 + 160], pixels[5 * 320 + 5]);
    }
    CHECK(firstSignature != "<no frame>" && lit > 320 * 240 / 2, "the first frame is drawn in the view (most pixels lit)");

    CHECK(js("m.play().then(function(){}),'ok'") == "ok", "play requested");
    CHECK(pumpUntil([&] { return num("m.currentTime") > 0.25; }, 8000), "the clock advances while playing");
    CHECK(js("String(m.paused)") == "false" && logHas("playing"), "playing, not paused");
    std::string playingSignature = viewSignature();
    js("m.pause(),'p'");
    CHECK(pumpUntil([&] { return logHas("pause"); }, 5000), "pause event");
    double pausedAt = num("m.currentTime");
    crtweb_runtime_pump(gRuntime, 300);
    CHECK(num("m.currentTime") - pausedAt < 0.05, "the clock stops when paused");
    CHECK(playingSignature != firstSignature, "a later frame differs from the first");

    js("window.log=[],m.currentTime=0.7,'s'");
    CHECK(pumpUntil([&] { return logHas("seeked"); }, 8000), "seeked event");
    double seekedTo = num("m.currentTime");
    CHECK(seekedTo > 0.55 && seekedTo < 0.9, "currentTime after seek");
    CHECK(viewSignature() != firstSignature, "the frame after seeking differs from the first");

    js("window.log=[],m.currentTime=0,m.play(),'e'");
    CHECK(pumpUntil([&] { return logHas("ended"); }, 10000), "ended event");
    if (gTrace)
        fprintf(stderr, "trace: after end: log %s ended %s t %s dur %s paused %s\n", js("window.log.join(',')").c_str(), js("String(m.ended)").c_str(),
            js("String(m.currentTime)").c_str(), js("String(m.duration)").c_str(), js("String(m.paused)").c_str());
    CHECK(num("m.currentTime") > duration - 0.2, "currentTime reaches the end");
    CHECK(js("String(m.ended)") == "true", "ended");

    js("m.volume=0.25;m.muted=true;'v'");
    CHECK(js("String(m.volume) + ',' + String(m.muted)") == "0.25,true", "volume and mute reflected");

    // ---- audio-only: MP3 and WAV ----
    writeFile(directory + "/crtweb_media_audio.html", pageFor("<audio id=m src='" + assetURL + "/test_tone.mp3'></audio>", ""));
    CHECK(loadPage(fileURLFor(directory + "/crtweb_media_audio.html")), "audio page");
    CHECK(pumpUntil([&] { return logHas("loadedmetadata"); }, 15000), "audio metadata");
    CHECK(num("m.duration") > 0.8 && num("m.duration") < 1.3, "audio duration");
    CHECK(js("typeof m.videoWidth") == "undefined" || js("String(m.videoWidth)") == "0", "no video size for audio");
    js("m.play(),'a'");
    CHECK(pumpUntil([&] { return logHas("ended"); }, 10000), "audio plays to the end");
    writeFile(directory + "/crtweb_media_wav.html", pageFor("<audio id=m src='" + assetURL + "/test_tone.wav'></audio>", ""));
    CHECK(loadPage(fileURLFor(directory + "/crtweb_media_wav.html")) && pumpUntil([&] { return logHas("loadedmetadata"); }, 15000), "wav metadata");
    CHECK(num("m.duration") > 0.2 && num("m.duration") < 0.4, "wav duration (0.25 s)");

    writeFile(directory + "/crtweb_media_stereo.wav", stereoPCMFixture());
    writeFile(directory + "/crtweb_media_stereo.html", pageFor("<audio id=m src='"
        + fileURLFor(directory + "/crtweb_media_stereo.wav") + "'></audio>", ""));
    CHECK(loadPage(fileURLFor(directory + "/crtweb_media_stereo.html"))
        && pumpUntil([&] { return logHas("loadeddata"); }, 15000), "stereo PCM metadata/data");
    CHECK(num("m.duration") > 0.29 && num("m.duration") < 0.31, "generated stereo PCM duration");
    crtweb_runtime_pump(gRuntime, 300);
    CHECK(js("m.paused && m.currentTime === 0 && !m.ended") == "true", "paused preroll retains stereo PCM");
    js("m.play(),'stereo'");
    CHECK(pumpUntil([&] { return logHas("ended"); }, 10000), "stereo PCM plays to end");

    // ---- sources that cannot play ----
    writeFile(directory + "/crtweb_media_bad.mp4", "this is not an mp4 file");
    writeFile(directory + "/crtweb_media_bad.html", pageFor("<video id=m src='" + fileURLFor(directory + "/crtweb_media_bad.mp4") + "'></video>", ""));
    CHECK(loadPage(fileURLFor(directory + "/crtweb_media_bad.html")), "bad media page");
    CHECK(pumpUntil([&] { return logHas("error"); }, 15000), "garbage reports an error");
    CHECK(js("m.error ? String(m.error.code) : 'none'") == "4", "MEDIA_ERR_SRC_NOT_SUPPORTED for garbage");
    writeFile(directory + "/crtweb_media_missing.html", pageFor("<video id=m src='file:///crtweb_media_missing_file.mp4'></video>", ""));
    CHECK(loadPage(fileURLFor(directory + "/crtweb_media_missing.html")), "missing media page");
    CHECK(pumpUntil([&] { return logHas("error"); }, 15000), "a missing file reports an error");

    // ---- through the browser's own HTTP loader ----
    {
        HttpServer server;
        server.setMediaDirectory(assets);
        writeFile(directory + "/crtweb_media_http.html", pageFor("<video id=m muted src='" + server.url("/media/test_video.mp4") + "'></video>", ""));
        CHECK(loadPage(fileURLFor(directory + "/crtweb_media_http.html")), "http media page");
        CHECK(pumpUntil([&] { return logHas("loadeddata"); }, 20000), "metadata and first frame over HTTP");
        CHECK(num("m.duration") > 0.8 && num("m.videoWidth") == 64, "http duration and size");
        js("m.play(),'h'");
        CHECK(pumpUntil([&] { return logHas("ended"); }, 10000), "HTTP media plays to the end");
    }

    // ---- repeated lifetimes: no thread growth in the Web process ----
    CHECK(loadPage(fileURLFor(directory + "/crtweb_media_video.html")), "page for lifetimes");
    auto playbackCycle = [&](int i) {
        CHECK(loadPage(fileURLFor(directory + "/crtweb_media_video.html")), "lifetime page");
        CHECK(pumpUntil([&] { return num("m.readyState") >= 3; }, 15000), "lifetime media is ready");
        js("window.playResult='pending';m.play().then(function(){window.playResult='resolved'},function(e){window.playResult=e.name}),'x'");
        bool advanced = pumpUntil([&] { return num("m.currentTime") > 0.1; }, 8000);
        if (!advanced || gTrace)
            fprintf(stderr, "trace: lifetime %d advanced=%d state=%s\n", i, advanced,
                js("JSON.stringify({ready:m.readyState,paused:m.paused,time:m.currentTime,error:m.error?m.error.code:0,play:window.playResult,log:window.log})").c_str());
        CHECK(advanced, "lifetime playback advances");
        if (i % 2)
            js("m.pause(),m.removeAttribute('src'),m.load(),'r'");
    };
#if defined(CRT_TARGET_OS_WINDOWS) || defined(WTF_CRT_COFF_ASM)
    // Take the baseline after representative playback, not a paused page.
    // Then measure twice the original cycle count with the SAME +3 bound:
    // a per-player leak must still fail; lazy process-wide pools may settle.
    for (int i = 0; i < 12; ++i)
        playbackCycle(i);
    constexpr int lifetimeCycles = 24;
#else
    constexpr int lifetimeCycles = 12;
#endif
    crtweb_runtime_pump(gRuntime, 500);
    auto processes = childrenNamed("WebProcess");
    long baseline = processes.empty() ? -1 : threadsOf(processes[0]);
#if defined(CRT_TARGET_OS_WINDOWS) || defined(WTF_CRT_COFF_ASM)
    // Toolhelp's process and thread observations are separate snapshots.
    // Require a positive observation within a bounded interval, never treat
    // an absent/transient snapshot (-1) as a valid resource baseline.
    CHECK(pumpUntil([&] {
        processes = childrenNamed("WebProcess");
        baseline = processes.empty() ? -1 : threadsOf(processes[0]);
        return baseline > 0;
    }, 3000), "baseline WebProcess/thread observation");
    pid_t baselinePid = processes.empty() ? -1 : processes[0];
#endif
    for (int i = 0; i < lifetimeCycles; ++i)
        playbackCycle(i);
    crtweb_runtime_pump(gRuntime, 500);
    processes = childrenNamed("WebProcess");
    long after = processes.empty() ? -1 : threadsOf(processes[0]);
#if defined(CRT_TARGET_OS_WINDOWS) || defined(WTF_CRT_COFF_ASM)
    CHECK(pumpUntil([&] {
        processes = childrenNamed("WebProcess");
        after = processes.empty() ? -1 : threadsOf(processes[0]);
        return after > 0;
    }, 3000), "final WebProcess/thread observation");
    CHECK(!processes.empty() && baselinePid == processes[0], "same WebProcess across measured lifetimes");
    fprintf(stdout, "crtweb_media_test: observed process %ld -> %ld\n", static_cast<long>(baselinePid),
        static_cast<long>(processes.empty() ? -1 : processes[0]));
#endif
    if (!processes.empty())
        printMappedFiles(processes[0]);
    fprintf(stdout, "crtweb_media_test: %d measured lifetimes; web process threads %ld -> %ld\n", lifetimeCycles, baseline, after);
    CHECK(baseline > 0 && after > 0 && after <= baseline + 3, "threads do not grow over repeated playback lifetimes");

    // ---- teardown ----
    crtweb_view_destroy(gView);
    crtweb_runtime_destroy(gRuntime);
    CHECK(childrenNamed("WebProcess").empty() && childrenNamed("NetworkProcess").empty(), "web processes are gone after the runtime");
    for (const char* name : { "video", "audio", "wav", "bad", "missing", "http" })
        unlink((directory + "/crtweb_media_" + name + ".html").c_str());
    unlink((directory + "/crtweb_media_bad.mp4").c_str());
    if (failures)
        return 1;
    printf("crtweb_media_test: ok\n");
    return 0;
}
