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
