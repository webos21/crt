# Windows Narrow-String Model

Status: **adopted and implemented 2026-10-31** for CRT-built Windows executables
(option B below), replayed on native Windows/arm64 only; Windows/x64 is not yet
replayed. The wide-call rewrite (option C) stays the fallback. Decisions: B adopted;
minimum Windows 10 1903; the query is `crt_get_narrow_encoding()` and CRT startup does
not abort; no opt-out for CRT-built executables; UTF-8 validation belongs to callers
that take untrusted names (the CLI validates on Windows). Measurements and commands are in the CLI repository's
[acceptance record](https://github.com/webos21/crt-cli/blob/main/docs/acceptance/bootstrap_acceptance.md#windows-narrow-string-model-prototype-2026-10-31).

## Problem

CRT code speaks POSIX: paths, `argv`, environment names and `readdir` results are
`char*`. On Windows that has to mean one encoding, and today it means two.

| Layer | Narrow encoding today |
| --- | --- |
| `argv`, environment, `mkdir`/`fopen`/`opendir` (well over a hundred `...A` Win32 call sites in `libc/src/arch/windows/common/syscall.c` and `crt1.c`) | the active **ANSI code page** (949 on the Korean measurement host) |
| imported libc++ `filesystem` (`__char_to_wide`: `AreFileApisANSI() ? CP_ACP : CP_OEMCP`) | ANSI code page |
| `mbstowcs`/`wcstombs`, `nl_langinfo(CODESET)`, `crt_dl_iterate_images` paths | **UTF-8** |
| clang and other UTF-8 tools (dependency files, diagnostics) | **UTF-8** |

Consequences measured on Windows/arm64: Hangul in `argv` arrives as CP949 bytes; UTF-8
bytes passed to `mkdir` fail with `ENOENT` and make libc++ throw
`__char_to_wide: Invalid or incomplete multibyte`; a character outside the code page
(`é` on 949) is silently best-fit to `e`, so a *different path* is opened; and a
CLI `build` in a Hangul directory fails because clang writes its dependency file in
UTF-8 while the CLI reads it as ANSI. The pieces that already claim UTF-8 are
inconsistent with the system calls beneath them.

## Constraints

- The public surface stays Bionic/POSIX; no `TCHAR`-style duality and no `wchar_t`
  paths in application code.
- Runtime imports stay minimal (`kernel32.dll` and one API-set DLL for the CLI).
- Imported libc++ and other upstream sources are not patched to hide CRT gaps; any
  exception needs review, rationale and removal conditions.
- A consumer must not duplicate Win32 calls to compensate; missing platform
  behaviour belongs in CRT.
- Supported targets are current Windows/x64 and Windows/arm64.

## Options

**A. Keep ANSI; convert at the consumer.** Rejected. It contradicts the UTF-8 that
CRT itself reports, characters outside the code page cannot be represented at all
(and are altered silently), and every consumer would need a Win32 conversion.

**B. UTF-8 active code page by application manifest.** A manifest with
`<activeCodePage>UTF-8</activeCodePage>` makes the process ANSI code page 65001
(Windows 10 version 1903, build 18362, or later). Every `...A` call, the command
line, the environment block and libc++'s `CP_ACP` conversion become UTF-8 without
touching CRT or libc++ sources.

**C. Wide calls inside CRT.** Replace the `...A` calls with `...W`, convert at the
boundary, build `argv`/`environ` from the wide command line and environment, and patch
libc++ to `CP_UTF8`. Independent of the manifest and of the OS build, and the most
complete result, but well over a hundred call sites, a libc++ patch to an upstream design, and
a larger regression surface (fork relaunch, spawn, signals, descriptors).

**D. B now, C only if needed.** C remains a compatible later step: the contract
(narrow means UTF-8) is identical, only the mechanism differs.

| Criterion | A | B | C |
| --- | --- | --- | --- |
| One narrow encoding for `argv`, environment, paths, libc++, PAL | no | yes | yes |
| Lossless for every Unicode scalar | no | yes | yes |
| CRT source changes | consumer-side | none in the runtime; link policy and build files | very large |
| Upstream patch | no | no | libc++ |
| Needs Windows 10 1903+ | no | **yes** | no |
| Works in a host process without the manifest | no | **no** (process-wide setting) | yes |
| Child processes | unchanged | correct (see below) | correct |

## Recommendation

Adopt **B** for CRT-built executables, record C as the fallback, and define the
contract below. The narrow-string model of CRT on Windows is then: **`char*` is UTF-8
in every CRT API and in the process's `...A` calls**, which also makes the already
UTF-8 pieces consistent.

## Contract

1. **Manifest.** The SDK ships `share/crt/windows/utf8.manifest` (the activeCodePage
   setting plus the Windows 10 `supportedOS` entry). The Windows `crt-cc`/`crt-c++`
   wrappers add `-Wl,/manifest:embed -Wl,/manifestinput:<sysroot>/share/crt/windows/utf8.manifest`
   to every **executable** link (not objects, archives or DLLs). CRT's own executables
   (mksh, toybox, awk, make and the tests) take the same option from their build files.
   Because the exporter records the wrapper's argv, the CLI and any provider-driven
   builder inherit the manifest from the provider without hard-coding it. User
   manifests are merged: a second `/manifestinput` linked and both settings were
   embedded.
2. **Encoding.** UTF-8 for `argv`, `environ`, path arguments and results (`readdir`,
   `getcwd`, `readlink`, `realpath`, `crt_dl_iterate_images`, ...). Invalid sequences are
   **not** diagnosed by the OS: `MultiByteToWideChar` replaces them with U+FFFD, so
   distinct invalid byte strings create or open the same name. Callers that take paths
   from untrusted input validate UTF-8 first (the CLI, see follow-ups).
3. **Detection.** A process can lack the setting (a CRT DLL inside a foreign host, an
   executable linked without the policy, Windows before 1903). CRT should expose the
   result, not guess: a platform query (name open) reports `utf-8` or `ansi` plus the
   code page. The CLI's bootstrap doctor checks it and fails its own startup check when
   it is not `utf-8`. CRT startup does not abort a process for it.
4. **Children.** `CreateProcessA` converts the command line and environment with the
   *parent's* active code page, so a UTF-8 parent hands any child, CRT or not,
   correct UTF-16. What the child does with it follows the child's own setting. Verified
   with clang: a compile in a Hangul/`é` directory succeeded, and the dependency file it
   wrote (UTF-8) is readable by the manifested CLI.
5. **Shared runtime.** `libc.dll` is a library; it cannot choose the process code page.
   Non-ASCII paths in a process that loads it without the manifest are unsupported
   and detectable through the query above.
6. **Out of scope.** Console input and output code pages (a program that prints UTF-8
   to a console selects the console code page itself), Unicode normalization and
   case folding (NTFS preserves the byte-for-byte UTF-16 name; NFC and NFD names stay
   distinct), ill-formed UTF-16 names already on disk (listed as U+FFFD, not
   round-trippable), long-path and reserved-name policy.

## Evidence (prototype, Windows/arm64, Windows 11 build 26300, ACP 949)

| Check | Without the manifest | With it |
| --- | --- | --- |
| `GetACP()` | 949 | 65001 |
| `argv` for `한글` / for `é` | `c7 d1 b1 db` / `65` (best-fit `e`) | `ed 95 9c ea b8 80` / `c3 a9` |
| environment value `한글é` | CP949 bytes | UTF-8 bytes |
| C `mkdir`, `fopen`, `opendir`/`readdir` with the UTF-8 name | `ENOENT` | works, names read back as UTF-8 |
| libc++ `exists`, `create_directories`, `directory_iterator`, `ofstream`, `remove_all` | throws `__char_to_wide` | works |
| Supplementary plane (emoji) and NFC versus NFD names | not applicable | distinct entries, as written |
| Invalid bytes, encoded lone surrogate | not applicable | accepted as U+FFFD names (collision hazard above) |
| Two manifest inputs at link time | not applicable | merged, both settings embedded |
| Full CLI `new`, `configure`, `build`, `test`, `run` in `한글 é 공백/hello` | `build` fails | all exit 0, JSON paths are valid UTF-8 |

The prototype links with a provider copy that appends the two manifest options before
`@OBJECTS@`; the manifest file sits outside the SDK. It is not an SDK change.

## Implementation status (2026-10-31)

Done in CRT `85546d9`, replayed on native Windows/arm64:

1. The SDK ships `share/crt/windows/utf8.manifest`; the `crt-cc`/`crt-c++` executable
   links embed it when the sysroot has the file; `create_dist.py` installs it,
   `verify_dist.py` checks it, and the stage-source packages carry it. The Dev profile
   gains the `crt-narrow-encoding-1` capability and the exporter requires it.
2. In-tree executables take the same link option from the top-level CMake file
   (`mksh.exe` and `toybox.exe` in the SDK carry the manifest). `crt-c-test` passes
   167 of 167, including the new `windows_utf8_narrow_test` (argv, environment, file
   names, a child process) and the extended `platform_test`.
3. `crt_get_narrow_encoding()` is in `include/crt/platform.h` and documented in
   `platform_queries.md`.
4. The CLI checks the encoding in `doctor`, validates arguments and compiler-reported
   dependency paths on Windows, and runs its native suite with a Unicode fixture.

Not done: replay on Windows/x64; the port tests (mksh, toybox, awk, make) in Unicode
directories; a mention of the Windows 10 1903 minimum in the user guides; behaviour
under a Windows build before 1903 (the setting is ignored there, which the query reports).

## Remaining decisions

- Whether CRT adds UTF-8 validation to path-taking calls, or leaves it to callers.
- Whether a foreign-host DLL scenario ever needs option C.
