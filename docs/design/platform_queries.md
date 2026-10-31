# Public Platform Queries

Adopted for the SDK/CLI contract on 2026-10-29. These are explicitly **CRT
extensions**, not functions claimed to exist in Android Bionic. They supplement
the Bionic/POSIX surface without changing its types, constants or symbols.

## Ownership

An SDK consumer must use existing libc/PAL operations before adding an OS
branch. Process creation, pipes, descriptors, clocks and child waiting belong
to CRT even when the caller is a build driver. Native loader/procfs/Win32 calls
must not be duplicated in CLI command handlers or diagnosis.

`include/crt/platform.h` supplies:

- `crt_get_platform_info()`: immutable runtime-target OS/architecture, real
  image format and artifact naming. It is not cross-compilation selection or
  external-tool identity. The producer's compiler policy still owns build flags.
- `crt_get_user_home(buffer, capacity)`: absolute `HOME` on Unix and
  `USERPROFILE` on Windows, copied into caller storage. Missing, relative or
  overlong configuration fails; it never silently chooses `/`, a working
  directory, AppData or an XDG location. It is not a security identity lookup.
- `crt_get_narrow_encoding()`: the encoding of the `char*` strings the process uses for
  `argv`, the environment and paths, `"utf-8"` or `"ansi"`; never NULL. Every target
  is UTF-8 except a Windows process without the UTF-8 active code page (see the
  [Windows narrow-string model](windows_narrow_strings.md)). It reports the state and
  never changes it; the SDK's `crt-narrow-encoding-1` capability promises the query
  and, on Windows, the manifest policy.
- `crt_is_native_executable(path)`: bounded ELF64/Mach-O64/universal/PE header
  and architecture screening before a native-tool launch. Scripts and other
  targets return 0, I/O/argument errors -1 with errno. The OS loader remains
  authoritative; this is neither full binary validation nor authentication.

`include/crt/dl.h` supplies `crt_dl_iterate_images(callback, data)` in libdl.
The callback receives a borrowed path, base and actual format, with structure
size. A positive callback return stops traversal; -1 indicates an error.
Linux reuses the existing real ELF loader/auxv traversal; macOS uses dyld;
Windows takes a Toolhelp module snapshot and converts UTF-16 paths to UTF-8.
Callbacks must not load/unload images. Enumeration is diagnostic best-effort
under concurrent loader changes, not a synchronized process snapshot.

`dl_iterate_phdr()` remains Bionic-shaped and ELF-specific: Mach-O/PE hosts do
not invent ELF program headers. `dladdr()` remains the address-ownership API.
Images mapped by an OS facility must not automatically be classified as CRT
runtime imports; allocation/symbol ownership and declared import audits remain
separate evidence.

## Consumer Contract

The default CLI SDK store on **all three hosts** is
`$USER_HOME/.crt/sdks/<sdk-id>/`. User home comes through the PAL above. SDK
internal layout stays unchanged. Explicit `--sdk-root` handoffs remain an
experimental selection mechanism, not an alternate installer default.
`--sdk-id` resolves an existing installation under the common store; verified
download/extraction/publication and garbage collection remain CLI D2 work.

CLI JSON/SHA source copies are not part of the PAL. The Development SDK
supplies cJSON (embedded C), nlohmann/json (C++) and the already-ported Mbed TLS
SHA-2 implementation; recipe pins/notices/tests are producer-owned. No claim
of measured "most widely used" market share is made. cJSON's small ANSI C
implementation and MIT license fit the embedded requirement; SHA-2 reuse avoids
maintaining another crypto implementation. See upstream
[cJSON](https://github.com/DaveGamble/cJSON),
[JSON integration](https://json.nlohmann.me/integration/) and
[Mbed TLS](https://github.com/Mbed-TLS/mbedtls).

## Verification

`platform_test_runs` exercises native-image screening, user-home validation,
image enumeration, callback stop and error handling on each native host.
External SDK and CLI target consumers must also link/run with both static and
shared CRT policy. The consumer-owned compatibility ledger records exact
revisions, SDK/provider hashes, patched-source identity and observed hosts;
compile-only foreign-host checks are not runtime replay.
