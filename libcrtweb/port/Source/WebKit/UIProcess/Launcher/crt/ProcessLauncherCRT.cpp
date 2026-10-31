// Copyright (c) 2026 The CRT Project.
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// The CRT process launcher (Web Tranche 3B, milestone B4): spawns the Web and Network process
// executables with a SOCK_SEQPACKET socket pair, the contract IPC::Connection's Unix backend and
// AuxiliaryProcessMain already speak (argv: processIdentifier, connection file descriptor).
//
// Where PlayStation asks its SDK to launch a .self and the GTK/WPE ports use GSubprocess, this uses
// posix_spawn from CRT libc, so the same file serves Linux, macOS and Windows. A reaper thread per
// child collects its exit status so a killed child leaves no zombie; the UI process learns of the
// death through the closed connection, as upstream does.

#include "config.h"
#include "ProcessLauncher.h"
#include "WKViewCRT.h"

#include "IPCUtilities.h"
#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <map>
#include <mutex>
#include <wtf/NeverDestroyed.h>
#include <wtf/FileSystem.h>
#include <wtf/RunLoop.h>
#include <wtf/Threading.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

extern char** environ;

namespace {
struct ChildProcesses {
    std::mutex lock;
    std::map<uint64_t, pid_t> running;
};
ChildProcesses& children()
{
    static NeverDestroyed<ChildProcesses> registry;
    return registry.get();
}
}

// Tokens are WebKit's unique process identifiers, not recyclable OS PIDs. Runtime teardown can
// wait for its own children without touching another runtime or a process which reused a PID.
bool WKViewCRTProcessIsRunning(uint64_t token)
{
    std::lock_guard<std::mutex> guard(children().lock);
    return children().running.count(token);
}
size_t WKViewCRTLiveProcessTokens(uint64_t* tokens, size_t capacity)
{
    std::lock_guard<std::mutex> guard(children().lock);
    size_t index = 0;
    for (const auto& process : children().running) {
        if (index < capacity) tokens[index] = process.first;
        ++index;
    }
    return index;
}
void WKViewCRTTerminateProcess(uint64_t token)
{
    std::lock_guard<std::mutex> guard(children().lock);
    auto found = children().running.find(token);
    if (found != children().running.end()) kill(found->second, SIGKILL);
}

namespace WebKit {

// CRTWEB_PROCESS_DIR names the directory holding WebProcess and NetworkProcess; without it the
// directory of the running executable is used, so a build tree or an installed layout both work.
static String processDirectory()
{
    if (const char* directory = getenv("CRTWEB_PROCESS_DIR"); directory && *directory)
        return String::fromUTF8(directory);

    char buffer[4096];
    ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length <= 0)
        return "."_s;
    buffer[length] = 0;
    return FileSystem::parentPath(String::fromUTF8(buffer));
}

static CString processPath(ProcessLauncher::ProcessType type)
{
    ASCIILiteral name = "WebProcess"_s;
    switch (type) {
    case ProcessLauncher::ProcessType::Network:
        name = "NetworkProcess"_s;
        break;
#if ENABLE(GPU_PROCESS)
    case ProcessLauncher::ProcessType::GPU:
        name = "GPUProcess"_s;
        break;
#endif
    case ProcessLauncher::ProcessType::Web:
    default:
        break;
    }
    return FileSystem::pathByAppendingComponent(processDirectory(), name).utf8();
}

void ProcessLauncher::launchProcess()
{
    if (m_launchOptions.shouldMakeProcessLaunchFailForTesting)
        return;

    // The server end is close-on-exec; the client end is inherited by the child.
    IPC::SocketPair socketPair = IPC::createPlatformConnection(SOCK_SEQPACKET, IPC::PlatformConnectionOptions::SetCloexecOnServer);

    char processIdentifierString[32];
    snprintf(processIdentifierString, sizeof processIdentifierString, "%llu", static_cast<unsigned long long>(m_launchOptions.processIdentifier.toUInt64()));
    char connectionIdentifierString[16];
    snprintf(connectionIdentifierString, sizeof connectionIdentifierString, "%d", socketPair.client.value());

    CString path = processPath(m_launchOptions.processType);
    char* argv[] = { const_cast<char*>(path.data()), processIdentifierString, connectionIdentifierString, nullptr };

    pid_t pid = 0;
    int result = posix_spawn(&pid, path.data(), nullptr, nullptr, argv, environ);
    if (result) {
        fprintf(stderr, "crtweb: failed to launch %s: %s\n", path.data(), strerror(result));
        return;
    }

    // The child owns its copy of the client end; dropping ours lets the server see EOF when it exits.
    socketPair.client = { };

    uint64_t token = m_launchOptions.processIdentifier.toUInt64();
    {
        std::lock_guard<std::mutex> guard(children().lock);
        children().running.emplace(token, pid);
    }
    Thread::create("CRT process reaper"_s, [pid, token] {
        int status = 0;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) { }
        std::lock_guard<std::mutex> guard(children().lock);
        children().running.erase(token);
    })->detach();

    RunLoop::mainSingleton().dispatch([protectedThis = Ref { *this }, pid, serverIdentifier = WTF::move(socketPair.server)] mutable {
        protectedThis->didFinishLaunchingProcess(pid, IPC::Connection::Identifier { WTF::move(serverIdentifier) });
    });
}

void ProcessLauncher::terminateProcess()
{
    if (!m_processID)
        return;
    kill(m_processID, SIGKILL);
}

void ProcessLauncher::platformInvalidate()
{
    m_processID = 0;
}

} // namespace WebKit
