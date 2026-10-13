#pragma once

// Host-side observations for lifecycle acceptance only. No native types enter crtweb's API.
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <string>
#include <vector>

namespace crtweb_test {
struct Process { pid_t pid; std::string name; };
struct Resources { long memoryKiB; long threads; long descriptors; };

#if defined(WTF_CRT_COFF_ASM) || defined(_WIN32)
extern "C" {
struct ProcessEntry {
    unsigned size, usage, pid;
    uintptr_t heap;
    unsigned module, threads, parent;
    int priority;
    unsigned flags;
    char executable[260];
};
struct MemoryCounters {
    unsigned size, faults;
    uintptr_t peak, resident, quotaPeakPaged, quotaPaged, quotaPeakNonPaged, quotaNonPaged,
        pagefile, peakPagefile;
};
void* __stdcall CreateToolhelp32Snapshot(unsigned, unsigned);
int __stdcall Process32First(void*, ProcessEntry*);
int __stdcall Process32Next(void*, ProcessEntry*);
int __stdcall CloseHandle(void*);
void* __stdcall OpenProcess(unsigned, int, unsigned);
unsigned __stdcall WaitForSingleObject(void*, unsigned);
void* __stdcall GetCurrentProcess(void);
int __stdcall GetProcessHandleCount(void*, unsigned*);
int __stdcall K32GetProcessMemoryInfo(void*, MemoryCounters*, unsigned);
}
inline std::vector<Process> children(pid_t parent = getpid())
{
    std::vector<Process> result;
    void* snapshot = CreateToolhelp32Snapshot(2, 0);
    if (snapshot == reinterpret_cast<void*>(-1)) return result;
    ProcessEntry entry { };
    entry.size = sizeof entry;
    for (int more = Process32First(snapshot, &entry); more; more = Process32Next(snapshot, &entry)) {
        if (entry.parent != static_cast<unsigned>(parent)) continue;
        void* process = OpenProcess(0x00100000, 0, entry.pid);
        bool alive = process && WaitForSingleObject(process, 0) != 0;
        if (process) CloseHandle(process);
        if (!alive) continue;
        std::string name = entry.executable;
        if (name.size() > 4 && name.substr(name.size() - 4) == ".exe") name.resize(name.size() - 4);
        result.push_back({static_cast<pid_t>(entry.pid), name});
    }
    CloseHandle(snapshot);
    return result;
}
inline Resources resources()
{
    MemoryCounters memory { };
    memory.size = sizeof memory;
    unsigned handles = 0;
    long threads = -1;
    void* snapshot = CreateToolhelp32Snapshot(2, 0);
    ProcessEntry entry { };
    entry.size = sizeof entry;
    if (snapshot != reinterpret_cast<void*>(-1)) {
        for (int more = Process32First(snapshot, &entry); more; more = Process32Next(snapshot, &entry))
            if (entry.pid == static_cast<unsigned>(getpid())) threads = entry.threads;
        CloseHandle(snapshot);
    }
    bool ok = K32GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof memory)
        && GetProcessHandleCount(GetCurrentProcess(), &handles);
    return {ok ? static_cast<long>(memory.resident / 1024) : -1, threads, ok ? static_cast<long>(handles) : -1};
}
#else
inline long descriptors()
{
    long count = 0, limit = sysconf(_SC_OPEN_MAX);
    if (limit < 0 || limit > 65536) limit = 65536;
    for (int fd = 0; fd < limit; ++fd) if (fcntl(fd, F_GETFD) >= 0) ++count;
    return count;
}
#if defined(__APPLE__) || defined(WTF_CRT_MACHO_ASM)
extern "C" int proc_listchildpids(pid_t, void*, int);
extern "C" int proc_name(int, void*, uint32_t);
extern "C" int proc_pidinfo(int, int, uint64_t, void*, int);
struct TaskInfo {
    uint64_t virtualSize, residentSize, totalUser, totalSystem, threadsUser, threadsSystem;
    int32_t policy, faults, pageIns, copyOnWriteFaults, messagesSent, messagesReceived,
        machCalls, unixCalls, switches, threads, runningThreads, priority;
};
inline std::vector<Process> children(pid_t parent = getpid())
{
    std::vector<Process> result;
    pid_t pids[256] { };
    int count = proc_listchildpids(parent, pids, sizeof pids);
    for (int i = 0; i < count && i < 256; ++i) {
        char name[256] { };
        if (pids[i] > 0 && proc_name(pids[i], name, sizeof name) > 0) result.push_back({pids[i], name});
    }
    return result;
}
inline Resources resources()
{
    TaskInfo info { };
    if (proc_pidinfo(getpid(), 4, 0, &info, sizeof info) != sizeof info) return {-1, -1, -1};
    return {static_cast<long>(info.residentSize / 1024), info.threads, descriptors()};
}
#else
inline std::vector<Process> children(pid_t parent = getpid())
{
    std::vector<Process> result;
    DIR* directory = opendir("/proc");
    if (!directory) return result;
    while (dirent* entry = readdir(directory)) {
        pid_t pid = atoi(entry->d_name);
        if (pid <= 0) continue;
        std::string root = "/proc/" + std::to_string(pid);
        FILE* stat = fopen((root + "/stat").c_str(), "r");
        if (!stat) continue;
        char line[1024] { }, state = 0;
        int ppid = 0;
        bool read = fgets(line, sizeof line, stat);
        fclose(stat);
        char* tail = strrchr(line, ')');
        if (!read || !tail || sscanf(tail + 1, " %c %d", &state, &ppid) != 2 || ppid != parent) continue;
        // Keep zombies visible: shutdown has to reap, not only stop execution.
        char name[4096] { };
        ssize_t length = readlink((root + "/exe").c_str(), name, sizeof name - 1);
        if (length > 0) {
            name[length] = 0;
            const char* leaf = strrchr(name, '/');
            result.push_back({pid, leaf ? leaf + 1 : name});
        } else if (state == 'Z') result.push_back({pid, "zombie"});
    }
    closedir(directory);
    return result;
}
inline Resources resources()
{
    long memory = -1, threads = -1;
    FILE* file = fopen("/proc/self/status", "r");
    if (file) {
        char line[256];
        while (fgets(line, sizeof line, file)) {
            if (!strncmp(line, "VmRSS:", 6)) memory = strtol(line + 6, nullptr, 10);
            if (!strncmp(line, "Threads:", 8)) threads = strtol(line + 8, nullptr, 10);
        }
        fclose(file);
    }
    return {memory, threads, descriptors()};
}
#endif
#endif
}
