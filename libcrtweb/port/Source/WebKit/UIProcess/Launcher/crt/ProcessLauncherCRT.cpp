// Copyright (c) 2026 The CRT Project. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the
//    distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
// OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
// AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

// The CRT process launcher (Web Tranche 3B, milestone B4): spawns the Web and Network process
// executables with a SOCK_SEQPACKET socket pair, the contract IPC::Connection's Unix backend and
// AuxiliaryProcessMain already speak (argv: processIdentifier, connection file descriptor).
//
// Where PlayStation asks its SDK to launch a .self and the GTK/WPE ports use GSubprocess, this uses
// posix_spawn from CRT libc, so the same file serves Linux and macOS; Windows has its own
// launcher in upstream (win/ProcessLauncherWin.cpp) and arrives with Tranche 6. A reaper thread per
// child collects its exit status so a killed child leaves no zombie; the UI process learns of the
// death through the closed connection, as upstream does.

#include "config.h"
#include "ProcessLauncher.h"

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
#include <wtf/FileSystem.h>
#include <wtf/RunLoop.h>
#include <wtf/Threading.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

extern char** environ;

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

    Thread::create("CRT process reaper"_s, [pid] {
        int status = 0;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) { }
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
