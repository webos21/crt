# Native Device Controls

These are **build-host diagnostics**, not CRT-linked tests or SDK dependencies.
They deliberately use Apple's SDK and libSystem to distinguish host framework
behavior from the CRT/PAL and browser layers. They are not added to CMake,
CTest, distribution stages or the WebKit acceptance gates.

## AVFoundation Lifecycle

From the repository root on macOS:

```sh
evidence=out/macos-host-ninja-debug/tranche8-replay-20261015
mkdir -p "$evidence"
/usr/bin/clang -O1 -Wall -Wextra -Werror -fno-objc-arc \
  -framework AVFoundation -framework Foundation -framework CoreMedia \
  -framework CoreVideo libcrtmedia/tests/native/avfoundation_lifecycle.m \
  -o "$evidence/native-camera-control"
env -u DYLD_LIBRARY_PATH "$evidence/native-camera-control" --allow-physical-camera
env -u DYLD_LIBRARY_PATH "$evidence/native-camera-control" --allow-physical-camera --idle-ms=300
```

Run these sequentially. Prior macOS camera authorization and explicit opt-in
are required. The diagnostic never changes permissions, effects or routing and
does not save images. It opens the default camera with 640x480/NV12 output,
waits for three real frame callbacks, stops the session, removes its delegate,
drains the serial callback queue, releases ownership and drains its autorelease
pool. Four warmups precede twenty measured sessions in the same process.
Default mode observes resources at 300ms and 2s after each release; rapid mode
restarts after 300ms. Both observe another thirty idle seconds at the end.

RSS/thread/fd counts are observations, not a substituted acceptance result.
Unlike the browser probe, this control has no WebKit, CRT allocator, DOM video
clock, permission bridge, navigation or cloned tracks. Plane copying is not the
PAL's NV12-to-YUV420P conversion, frame queue or RGBA presentation. A passing
control can isolate a difference but cannot identify which omitted layer caused
it or close the browser lifetime gate. Measurements and interpretation belong
in [Web acceptance](../../../docs/acceptance/crtweb_acceptance.md).

## CRT/PAL Counterpart

`../capture_lifetime_probe.cpp` performs the same four-warmup/twenty-session
exercise through the installed CRT capture API, without WebKit. It creates and
joins one CRT worker per session, verifies three owned YUV420P frames and their
increasing timestamps, releases them, then stops/releases capture. It reuses
the repository's test-only process observer, not a WebKit library. No images
are saved and an unavailable resource observation fails the probe.

Build and run on macOS with a verified `05-ui` SDK, sequentially with any other
camera probes:

```sh
sdk=out/macos-host-ninja-debug/dist/05-ui
evidence=out/macos-host-ninja-debug/tranche8-replay-20261015
env CRT_CXX_RUNTIME_LINKAGE=shared CRT_CXX_HOSTED=1 \
  "$sdk/tools/crt-c++" -O1 -std=c++17 -DWTF_CRT_MACHO_ASM=1 \
  libcrtmedia/tests/capture_lifetime_probe.cpp -lcrtmedia \
  -o "$evidence/pal-camera-control"
env -u DYLD_LIBRARY_PATH "$evidence/pal-camera-control" --allow-physical-camera
```

Shared linkage keeps one CRT allocator/thread registry in this process, just
as in the browser. `WTF_CRT_MACHO_ASM` selects the test observer's macOS branch
despite the wrapper's Linux-shaped preprocessor persona. Do not interpret a
run with RSS/threads `-1` as resource evidence. The full browser camera gate
remains the authority for stop/revoke/navigation/clone and presentation.
