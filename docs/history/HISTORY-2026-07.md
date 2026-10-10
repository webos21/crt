# CRT History — July 2026

July 2026 is the **project bootstrap month** for CRT.

The retained Git history begins on **2026-07-27** with the initial commit and
contains **117 commits through 2026-07-31**. In only five days, the repository
moved from a minimal `hello` runtime into the foundations of what later became
the **`01-c` stage**, while also opening the first paths toward `02-cxx`,
shared libraries, dynamic loading, and external source-porting.

This file is a backfill reconstructed from the July 2026 Git log. It summarizes
stable outcomes rather than reproducing every commit.

---

## Month at a glance

The project evolved roughly in this order:

```text
minimal crt1 + write/exit
        |
Linux / Windows / macOS startup + syscall backends
        |
core libc
  string / fd / errno / malloc / stdio / mmap / time
        |
pthread / TLS / futex-like waits
        |
filesystem / process / signal / sockets
        |
libm
        |
ABI cleanup / libdl / shared libraries
        |
bootstrap C++ ABI
        |
CRT sysroot + external porting environment
        |
rootfs / fork-spawn groundwork for shell work
```

By the end of July, CRT was no longer just a startup/syscall experiment. It
already had the core architectural pieces needed for the August `01-c`
expansion:

- three-host runtime backends;
- a growing Bionic-shaped libc surface;
- pthread/TLS synchronization;
- filesystem, process, signal, and socket APIs;
- libm;
- preliminary dynamic loading and shared libraries;
- a bootstrap C++ ABI layer;
- sysroot/compiler wrappers and external port recipes;
- initial rootfs/process support for a shell environment.

---

## 2026-07-27 — Repository bootstrap and core libc

### Initial repository and host bring-up

The repository began with:

- root CMake build files and presets;
- architecture-specific startup code;
- `unistd.h`;
- minimal `exit()` and `write()`;
- a `hello` test;
- early design notes covering bring-up, project meaning, and stack choices.

The initial macOS implementation was quickly extended to Linux and Windows.
Windows arm64 support was also accounted for early by moving common Windows
startup/syscall code into a shared architecture-neutral location.

This established the first important project rule:

> The public runtime surface should remain common while startup/syscall/PAL
> details are implemented per host and architecture.

### Bionic-shaped libc import begins

The project then began filling out a Bionic-compatible libc surface rather than
inventing a new CRT API.

Early work added:

- `string.h` and basic memory/string functions;
- fd/open/read/write/close-style infrastructure;
- `errno`;
- `malloc`;
- `stdio` and file streams;
- `printf`;
- `mmap`;
- `ctype`;
- integer conversion and standard integer/type headers;
- time and monotonic clock support;
- scheduler helpers;
- atomics.

An explicit Bionic import document and third-party provenance area were added,
making source origin and compatibility intent part of the repository from the
beginning.

### `errno` becomes thread-local

The first Windows implementation used a global/static `errno`, but that was
quickly replaced with TLS-backed behavior.

This was one of the first examples of a pattern that continued throughout CRT:

> Temporary host-specific shortcuts are acceptable during bring-up, but the
> public semantics must converge toward the intended Bionic/POSIX contract.

### pthread foundation appears

Still on the first day, CRT gained the beginnings of its pthread layer:

- `pthread_t`;
- `pthread_create`;
- thread attributes;
- detach behavior;
- mutex attributes;
- condition variables;
- thread-specific keys;
- a futex/wait abstraction;
- contention tests.

Windows required its own wait/wake implementation, while Linux used its native
futex-oriented path.

By the end of 2026-07-27, threading was already a first-class runtime concern
rather than a later add-on.

---

## 2026-07-28 — pthread lifecycle, filesystem, libm, and networking

### pthread synchronization and lifecycle mature

The pthread layer expanded significantly:

- `pthread_once`;
- rwlocks;
- spinlocks;
- timed condition waits;
- contention tests;
- allocator/thread interaction;
- TLS key destructors;
- explicit Linux pthread lifecycle documentation.

Linux thread creation moved to `CLONE_THREAD`, and a reaper strategy was added
for detached-thread cleanup. Follow-up work addressed process/thread exit
behavior for that model.

This work established the foundation later hardened extensively by libc++,
Skia, FFmpeg, and JavaScriptCore.

### Filesystem and process-facing libc grows

The libc surface expanded with practical filesystem and environment behavior:

- `stat` family support;
- cwd handling;
- environment variables;
- `fcntl`;
- symlink/readlink;
- directory iteration;
- path/access behavior;
- additional stdio/file handling;
- qsort and extended integer/string helpers.

Windows also gained compiler/runtime glue such as `__chkstk`, reflecting CRT's
goal of supporting Clang/LLVM-generated native binaries rather than only a
portable C subset.

### Signals and process primitives start

Signal and process support was introduced together with:

- `signal.h`;
- process helpers;
- signal tests;
- early cross-host process semantics.

These July implementations were not yet the later fully hardened signal model,
but they established the API and PAL seams that August/October consumer-driven
work would refine.

### Locale, wide characters, and text runtime surface

CRT added early support for:

- locale;
- `wchar_t`/`mbstate_t`;
- wide-character classification and conversion;
- more complete string/memory routines.

This was important preparation for importing increasingly complex upstream
software.

### `libm` starts and adopts BSD/msun sources

`libm` began as a basic implementation and quickly incorporated FreeBSD/msun
material for functions such as:

- ceil/floor/round/trunc;
- exp/log/pow;
- trig argument reduction and kernels;
- scaling/remainder families;
- floating-point environment support.

A dependency map was created to track the imported math implementation and its
relationships.

This established another long-running CRT pattern:

> Reuse proven upstream/BSD implementations where licensing and architecture
> fit, while keeping CRT-owned build/provenance boundaries explicit.

### Socket/network APIs appear

The first socket/network layer was added with:

- socket APIs;
- IPv4 address structures/helpers;
- DNS-facing headers;
- `poll`;
- `select`;
- Windows Winsock adaptation;
- network and poll/select tests.

Bionic import manifests and validation tooling were also introduced around this
time, making provenance machine-checkable instead of only documentary.

---

## 2026-07-29 — ABI definition, libdl, shared libraries, and C++ bootstrap

### pthread policy and CRT TLS become explicit

The threading work was consolidated with:

- `docs/design/pthread_policy.md`;
- barrier support;
- Bionic-facing pthread API tests;
- policy/type tests;
- dedicated CRT TLS infrastructure.

`crt_tls` separated CRT runtime TLS needs from ad-hoc compiler/host assumptions
and moved `errno` and pthread internals toward a more deliberate model.

### Public ABI is refined

A dedicated header ABI document and `bits/crt_types.h` were introduced.

The work tightened definitions for:

- fundamental CRT types;
- socket/select/poll structures;
- time types;
- floating-point/long-double behavior;
- data-model assumptions.

Tests were added specifically for header ABI and host-independent data-model
expectations.

This was an important transition from "functions compile" toward:

> The headers and type layouts are themselves part of CRT's compatibility
> contract.

### `libdl` and shared-library work begin

CRT introduced:

- `dlfcn.h`;
- an initial `libdl`;
- dynamic-loading design documentation;
- shared-library documentation;
- a linker/loader design area;
- shared targets for libc/libm/libdl/C++ support;
- Windows DLL entry-point support.

The loader was still preliminary, but the repository now had explicit
architecture for both static and shared runtime delivery.

### Bootstrap C++ ABI layer starts

The first `libstdc++` directory was created, not yet as the later imported
LLVM libc++, but as a bootstrap C++ ABI/runtime bridge.

Work included:

- C++ ABI support;
- C++ runtime tests;
- a Windows C++ bridge;
- C++ frontend smoke tests;
- host-specific build fixes.

This is the earliest direct predecessor of the later `02-cxx` stage.

### External source-porting environment is created

A major architectural milestone arrived with the initial sysroot/porting tools:

- `docs/porting/sysroot_ports.md`;
- `crt-cc`;
- `crt-c++`;
- `crt-port-build.py`;
- `fetch_ports.py`;
- CRT environment scripts for Unix/PowerShell/cmd;
- `setjmp` support across hosts;
- dtoa/strtod support needed by real upstream code.

This changed the project's validation model.

Instead of asking only:

> "Do CRT unit tests pass?"

CRT could now ask:

> "Can a real upstream project be configured, built, linked, and executed
> against the CRT sysroot?"

That model became central to the rest of the project.

---

## 2026-07-30 — First real port recipes and shell/rootfs groundwork

### Porting recipes become concrete

The development/porting environment was refined and the first explicit recipes
were added for:

- zlib;
- libpng;
- SQLite amalgamation;
- libffi.

`docs/porting/porting_status.md` was added to track actual consumer progress.

The libc surface expanded in response to those consumers with additional:

- `sysconf`;
- ioctl/mount/statfs-related headers;
- platform compatibility headers;
- environment/filesystem/process fixes.

This was the beginning of CRT's **consumer-driven PAL development** model:
upstream software revealed the missing runtime contract, and CRT fixed that
contract rather than maintaining downstream source patches.

### Rootfs and shell-process prerequisites begin

A rootfs creation tool and Android-shell environment documentation were added,
along with:

- `spawn.h`;
- `sys/wait.h`;
- fork/spawn-related process support;
- rootfs process tests.

This was the direct setup for the shell/rootfs work that dominates early
August.

At this point Windows did not yet have the later robust fork/spawn broker
model, but the process API and rootfs shape were already being designed around
that goal.

### stdio becomes substantially more complete

A large amount of stdio work landed:

- expanded stream behavior;
- `scanf`;
- BSD-oriented stdio implementation work;
- unlocked stdio;
- wide-character I/O;
- improved `printf`;
- dtoa-backed formatting;
- scanf edge-case matrix;
- temporary-file/name support;
- memory-stream/edge tests.

This matters historically because configure-driven builds depend heavily on
the unglamorous corners of stdio and libc correctness.

### Windows export/ABI hygiene is tested

Windows-specific compiler ABI support and export-hygiene tests were added,
strengthening the distinction between:

- CRT-owned symbols;
- compiler-support symbols;
- host OS imports;
- public shared-library exports.

That work foreshadowed the later Host ABI firewall used by upper-runtime
stages.

---

## 2026-07-31 — Closing the bootstrap week

The final July commits were small but indicative of the project's next phase:

- a `TODO.md` focused on shell work was introduced;
- stdio EOF/boundary tests were added;
- the branch was synchronized with `main`.

By this point the repository had enough runtime surface that the next problem
was no longer "can CRT print hello?" but rather:

> Can CRT host a realistic shell/rootfs and rebuild real Unix-oriented
> software against its own headers, libraries, process model, and tools?

That became August's main theme.

---

## Architecture decisions established in July

Several project principles were already visible in the first five days and
remained important throughout later stages.

### 1. Bionic-shaped public API, host-specific implementation

Linux, Windows, and macOS use different native mechanisms, but the public
surface follows a Bionic/POSIX-shaped contract.

```text
upstream source
      |
Bionic-shaped headers/API
      |
     CRT
      |
+-----+------+------+
|            |      |
Linux      Windows  macOS
```

### 2. Consumer-driven completeness

CRT did not attempt to implement every POSIX API up front.

The project increasingly used:

- tests;
- imported runtime components;
- real upstream ports;

to determine which missing APIs or semantics mattered next.

### 3. Source portability, not binary emulation

The porting wrappers and sysroot work made the intended model explicit:

```text
source
  |
recompile against CRT
  |
native host executable
```

CRT was not trying to run Linux/Android binaries unchanged.

### 4. Provenance is part of the architecture

Bionic/BSD imports were tracked through documentation, manifests, and
third-party directories from the first week.

That provenance discipline later became essential for LLVM, Skia, FFmpeg,
LVGL, and WebKit.

### 5. Cross-host behavior must be tested early

Many July commits are paired with immediate Linux/Windows/macOS fixes.

The project therefore entered August already following the pattern:

```text
implement
   |
test on another host
   |
discover hidden platform assumption
   |
fix CRT/PAL
```

---

## End-of-month state

By 2026-07-31, the future stage names had not yet been formalized, but the
technical contents of the later `01-c` stage were clearly taking shape.

### Present or substantially underway

- startup/runtime entry on Linux, Windows, and macOS;
- Bionic-shaped public headers;
- string/memory/stdlib/stdio core;
- fd/filesystem/path APIs;
- mmap and time;
- pthread synchronization and TLS;
- process/signal foundations;
- sockets/poll/select;
- locale/wchar support;
- libm;
- preliminary libdl/shared-library support;
- bootstrap C++ ABI support;
- header/data-model ABI tests;
- compiler wrappers and CRT sysroot environment;
- first external-port recipes;
- initial rootfs/fork/spawn groundwork.

### Not yet closed

The following major capabilities belonged to later months:

- mature shell/rootfs execution;
- robust Windows fork/spawn behavior;
- large configure/make port suite;
- imported LLVM libc++/libc++abi/libunwind;
- formal cumulative `01-c` / `02-cxx` stage packaging;
- graphics/window stages;
- Skia/FFmpeg production integration;
- GPU/media/networking upper-runtime work.

Those developments continue in [August](HISTORY-2026-08.md) and
[September](HISTORY-2026-09.md).

---

## Commit-volume note

The July backfill is based on **117 commits**:

| Date | Commits | Main theme |
| --- | ---: | --- |
| 2026-07-27 | 37 | bootstrap, core libc, pthread foundation |
| 2026-07-28 | 41 | pthread lifecycle, filesystem, libm, networking |
| 2026-07-29 | 21 | ABI, TLS, libdl/shared libs, C++ ABI, sysroot/porting |
| 2026-07-30 | 15 | real port recipes, rootfs/process setup, stdio hardening |
| 2026-07-31 | 3 | shell TODO and stdio boundary cleanup |

The high commit density reflects an initial bring-up period. This archive
therefore groups commits by durable capability rather than preserving each
individual debugging step.

---

## Source note

This backfill was reconstructed from the repository's July 2026 Git log,
covering the initial commit on 2026-07-27 through the final 2026-07-31 merge.
Commit hashes remain available in Git for exact implementation history; this
file is the human-readable monthly summary.


## Evidence boundary and replay

The pre-archive `HISTORY.md` starts on August 2 and contains no July entries.
This July document is therefore a Git-based backfill, not a summary verified
against that log. The 117 commits and daily counts above were checked against
the retained repository history. Later stage names describe ancestry; they do
not imply that July already had cumulative SDK acceptance.

Start from initial commit `47d816e` and inspect the dated commits with:

```sh
git log 4e5eead68048723c37e46c22d80bca43915ac093 --reverse --format='%as %h %s' | sed -n '/^2026-07-/p'
git show 47d816e --stat
```

For a specific capability, select its commit from that list and inspect its
patch, CMake presets, tests, and import metadata. Replay using that revision's
instructions and host/toolchain prerequisites. This backfill does not provide
per-host run logs or exact July test totals, so API introductions must not be
cited as proof of complete runtime behavior on every host. Later regression
fixes in August and September are independent evidence.
