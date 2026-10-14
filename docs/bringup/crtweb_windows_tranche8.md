# Web Tranche 8 Windows/x64 replay investigation

Date: 2026-10-14..15. Base: `0baeba4` (macOS physical-camera and audio evidence).
This preserves the investigation; final Windows automatic replay passes, but
hardware acceptance and Tranche 8 closure are not inferred.
Acceptance results belong to [`crtweb_acceptance.md`](../acceptance/crtweb_acceptance.md).

## SDK prerequisite

The existing `out/windows-host-ninja-debug/dist/05-ui` SDK was built with
`CRTMEDIA_ENABLE_FFMPEG=OFF`. Its media DLL and import library contain frame,
audio and capture APIs, but no `crtmedia_extractor_create` or codec APIs. Enabling
the WebCore playback engine compiled successfully but failed the final link on
those missing symbols. Windows must link `libcrtmedia_dll.dll.a`, not the static
`libcrtmedia.a`, and refresh an FFmpeg-enabled predecessor before replay.

The new Windows configure check performs an executable link for the extractor
symbol. The SDK toolchain defaults try-compile to a static archive; an ordinary
`check_library_exists` under that default reported a false positive. The scoped
executable check rejects the existing FFmpeg-OFF SDK as expected. The positive
check now passes against the refreshed SDK; the final WebKit automatic replay passes.

## CRT shell and Make

Build-host shells/Make can establish target-library behavior when compilation
uses the CRT SDK and the final ABI audit passes. Such a comparison does not prove
CRT mksh/Make or Windows SDK self-hosting. The final FFmpeg replay uses CRT tools.
Exploratory Git Bash driver changes were removed from the working tree.

The initial silent configure interval was provisionally treated as a hang. A
bounded `CRT_PORT_SHELL_XTRACE=1` run showed FFmpeg advancing through its
`check_deps` and feature-selection calculations after the last compiler probe;
`config.log` is not updated during that calculation. This does not establish a
mksh deadlock. The trace was stopped after establishing progress. The standard
untraced CRT mksh configure subsequently completed successfully in 1744.6 s;
CRT Make `-j8` then completed the build in 473.4 s and installation in 237.0 s,
writing the normal `ffmpeg.installed` stamp. These timings cover Debug build-host
tools, not the final WebKit gate.

The first SDK refresh in the restricted agent execution environment produced
recursive mksh startup relaunches (the same compiler-wrapper command repeated
in a parent/child chain), before the compiler ran. That build was interrupted;
cleanup is restricted to the identified SDK commands and their parents. A
single CRT mksh invocation outside that restricted environment starts and exits
normally. The startup code decides whether to relaunch from the effective ASLR
mitigation policy, so the restricted launcher is a suspected contributing factor,
not yet a confirmed CRT defect. Retry the SDK build on the normal Windows host
before changing runtime code; this is not a Git Bash fallback.

The normal-host retry has no recursive launcher behavior, but exposed a separate
packaging dependency gap: `crt-media` installs `crtmedia_player_demo` when
FFmpeg is enabled while `crt-media-build` did not depend on that target. The
aggregate now includes the demo conditionally. A fresh preset configuration
preserving the current CRT options and the repeat `crt-ui-dist` build pass;
`01-c` through `05-ui` distribution verification passes, and the installed
import library contains the extractor/codec symbols.

The initial media regression selection mistakenly included
`crtmedia_capture_encode_lifecycle_test`, which is not CPU-only: it automatically
opens available cameras and hardware encoders. It finished before the attempted
stop, reporting 15 capture cycles / 450 captured, released and decoded frames,
plus 15 HW encode cycles / 1,500 samples. This unintended device run is recorded
for transparency, not as an opted-in WebKit physical-camera acceptance. Do not
include that test in this tranche's default regression filter; rerun the twelve
CPU/base-media tests without it: 12/12 pass in 0.75 s. No browser
physical-camera probe was run.

The fresh WebKit configure also exposed native Windows backslashes in the
driver's untyped CMake `-D` test-directory paths: WebKit's source-list evaluation
interpreted `C:\\Users` as an invalid `\\U` escape. Driver-owned CMake paths now use
forward slashes; two regression tests cover Windows paths and single arguments
containing spaces. This does not claim a complete WebKit path-with-spaces replay.
The corrected fresh WebKit configuration passes (136.5 s), including the
executable extractor prerequisite; `ENABLE_VIDEO`, `ENABLE_MEDIA_STREAM`,
JIT/DFG/FTL and WebAssembly are ON. At that checkpoint the combined replay was
rebuilding; its final result is recorded in the 2026-10-15 follow-up below.

The refreshed media DLL's separate export inventory contains 5,981 names, only
90 beginning `crtmedia_`; MinGW auto-export also publishes linked FFmpeg/SDK GUID
symbols. The PE import audit checks runtime dependencies, not API export hygiene.
The Windows export allowlist is recorded under Tranche 10 distribution/security
closure, independently of this tranche's functional replay. macOS already has
an exported-symbol boundary; do not report equivalent Windows export hardening.

A read-only LLDB sample of the untraced configure found `malloc_unlocked`
(`malloc.c:341`) called by mksh's lexer/parser under `eval`. Walking all heap
headers while paused found 71,626 nodes, 18,642 free nodes and 5,506,752 bytes of
payload capacity, ending at null with valid magic and no cycle. The allocator
linearly searches the whole block list for each allocation and coalesces that
list on free. This supports an allocator-workload performance follow-up rather
than a linked-list corruption claim. One sample is not a full CPU profile;
compare an optimized CRT build before deciding on allocator changes.

The installed CRT Make binary dated 2026-10-11 was rebuilt against the current
CRT through its `make` recipe. That recipe declares no port tests. Separate
minimal two-target Makefiles pass with CRT mksh at `-j2` and `-j8`; these are
basic launcher checks, not a substitute for the full FFmpeg build.

```powershell
python tools/crt-port-build.py --preset windows-host-ninja-debug `
  --target-os windows --target-arch x86_64 --port make --use-crt-shell `
  --skip-sysroot-build --rebuild --jobs 8
python tools/crt-port-build.py --preset windows-host-ninja-debug `
  --target-os windows --target-arch x86_64 --port ffmpeg --use-crt-shell `
  --skip-sysroot-build --jobs 8
```

Generated logs: `out/web-windows-make-rebuild.log`,
`out/web-windows-ffmpeg-crt-configure.log` (trace), and
`out/web-windows-ffmpeg-crt.log` (final untraced recipe). Inspect bounded tails;
the trace is large and is not committed.

## Replay changes prepared

- Enable the installed media engine/capture on Windows; keep Linux VA-API stubs
  scoped to Linux and select the Windows DLL import library.
- Add Windows executable suffixes and Toolhelp process/thread/module observations
  to the media gate. File URLs use the CRT `/c/...` path exposed by `getcwd`.
- Record OS runtime mappings and require one CRT libc++/libc++abi DLL pair.
- Accept the distribution's declared OLE32/Media Foundation PAL imports only
  from `libcrtmedia.dll`. A direct WebProcess media import, `ucrtbase.dll`, or a
  host C-runtime API-set import remains an audit failure. Audit tests pass 6/6;
  device-evidence parser tests pass 4/4 and licensing tests 15/15.
- CMake path regression tests pass 2/2; the core Windows test preset passes
  155/155. No API/runtime acceptance is inferred from a successful configure.

## Follow-up on 2026-10-15

The first complete build passed, but replay exposed two test/runtime limits.
The host OpenSSL executable was compiled with a missing
`Z:/extlib/_5040x__/ssl/openssl.cnf`; the PKI fixture now owns its configuration
through a subprocess-local `OPENSSL_CONF`. A fixture regression test and real
certificate generation pass, and all fifteen browser/WebView network cases pass.

WASAPI open fails at `GetDefaultAudioEndpoint`, HRESULT `0x80070490`, for all
four tested PCM format/rate/channel combinations. The Windows audio services are
running and audio controllers are present, but no default render endpoint is
available to this process. An opt-in PAL trace records the failed stage/HRESULT
without exposing native types. Forty-eight open/close calls on fresh CRT threads
keep the standalone process at four threads; this is not evidence of an audio
open/close thread leak. No PCM was delivered; do not claim real output.

The browser's original paused-page baseline was 14 threads and became 24 over
twelve real playback lifetimes. Windows now performs twelve representative
playback warm-ups and measures twenty-four additional lifetimes with the same
`+3` limit, also checking that each playback clock advances. The focused run is
22 -> 22 threads and passes. This demonstrates bounded initialization in that
run, not a larger accepted leak allowance. Other hosts retain twelve measured
lifetimes. At that checkpoint the final combined replay and repeats were pending;
their subsequent results and fixes follow below.

The first combined automatic gate passed 11/11, but extra repeats found issues
that prevent using that single pass as a stability claim. A media baseline
observation returned -1 once. Windows now requires positive observations within
three seconds and the same WebProcess PID across the measured lifetimes; the
thread bound is unchanged.

Synthetic capture repeats then reproduced a real use-after-free at late
`crtweb_media_permission_complete()` after `crtweb_view_destroy()`. The view's
release deleted the resolved token while the contract still required safe
rejection of the application's delayed decision. Tokens now move to inert
runtime-owned storage when their view is released, carry no retained WebKit
request/device reference after resolution, and are freed at runtime teardown.
Completion validates ownership before dereferencing token state; reserved
addresses prevent stale-token ABA grants within that runtime. The public
contract clarifies that applications discard tokens at runtime destruction.
Five synthetic capture runs pass with allocation churn, post-destroy completion,
replacement-request identity and unknown-token regressions. This is common code:
Linux/macOS replay of the strengthened capture regression is still required.
After that fix, three consecutive combined media/synthetic-capture repetitions
pass. The same WebProcess PID is observed before/after each 24-cycle measurement;
thread counts are 23 -> 23, 23 -> 15 and 23 -> 15. The subsequent full replay
of this version is described below; the earlier 11/11 result predates the token fix.
Console, multimedia and communications default render endpoints all return
`0x80070490` in the read-only endpoint probe (no stream started).

The subsequent full replay passed ten entries but exposed a stalled lifetime
playback and a cold 11 -> 23 thread baseline. Lifecycle navigation had waited
for the HTML document, not decoded media readiness. Each measured/warm-up cycle
now requires numeric `readyState >= 3`, a progressing playback clock and records
the `play()` promise/state on failure. Three consecutive media/synthetic-capture
runs pass with the same PID and 23 -> 22, 23 -> 15, 22 -> 15 threads. This does
not claim exhaustive early-play/loading-race coverage. The final full replay of
this strengthened version passes 11/11 (`out/web-windows-tranche8-ready-final.log`,
`C:/crtw/b3w/acceptance-b3.json`): media 23 -> 15 threads, same WebProcess PID,
network 15/15, static audit 17 binaries and runtime audit 140 mapped files with
no violations. The earlier failed full replay remains a failed historical run,
not the evidence used for this final result. Final measurements and reproduction
belong to the Windows replay section of the acceptance document.

No browser physical-camera probe was run. Browser hardware runs remain explicitly
opt-in and cannot be inferred from the synthetic camera test or the unintended
earlier libcrtmedia device test. Real audio needs an available default render
endpoint and a passing `--require-audio-output` replay; that gate stays open.
