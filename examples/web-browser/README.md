# CRT Web browser example

Build with the installed `06-web` SDK only:

```sh
cmake -S examples/web-browser -B browser-build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/absolute/path/to/sdk/crt-toolchain.cmake
cmake --build browser-build
CRTWEB_HOST=/absolute/path/to/sdk/libexec/crtweb/crtweb_host \
  ./browser-build/crtweb_browser --smoke
```

The packaged `examples/bin/crtweb_browser` locates its host relative to itself;
it can be run without environment overrides. With no arguments it displays a
local HTML page. A single URL opens that page. `--smoke` is a bounded offscreen
load/JavaScript/title/frame-pixel check, not a GPU or window-presentation test.
Both paths use the separate-process CPU-shared-memory WebView. The separately
packaged GPU worker remains available through `crtweb/gpu_surface.h`.

This is a development example, **not a security-hardened browser**. The port has
no application sandbox. Camera, popup, download and dialog requests are denied
by default. Website data is ephemeral. HTTPS fails closed unless the application
explicitly supplies a PEM trust bundle (`CRTWEB_CA_PEM_FILE` for this example);
there is no host certificate-store fallback or insecure command-line switch.
Use local/trusted content. The WebKit pin/update and public-release conditions
are recorded in the SDK's `share/crt/web-security.json` and Web acceptance.

An externally rebuilt sample needs `CRTWEB_HOST` explicitly. It finds the
package's relative fontconfig configuration beside that host; no build-machine
font path, host WebKit, GLib, GTK or WPE header is part of the application API.
