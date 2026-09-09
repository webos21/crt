# Porting Libraries With A CRT Distribution

The porting tools are an optional SDK feature. Running CRT applications and
the prebuilt examples does not require Python. Fetching and building upstream
libraries requires Python 3.9 or newer, the external compiler tools declared
by `manifest.json`, and network access unless an archive cache is supplied.

Activate the SDK, then fetch and build in a writable directory outside the
distribution:

```sh
python3 tools/fetch_ports.py --dest <work>/sources --cache <work>/downloads --port zlib
python3 tools/crt-port-build.py --sdk-root . --source-root <work>/sources \
  --work-root <work>/build --install-prefix <work>/install --port zlib --test
```

The recipes preserve upstream sources and verify archive SHA-256 digests.
The configure/make lane uses this distribution's `/system/bin/mksh`, make,
and Toybox applets; MSYS and Git Bash are not prerequisites. `zlib` is the
initial packaged-SDK smoke coverage and has completed configure, static/shared
build and install, and both round-trip execution tests on Windows. Other
recipes are included so their exact project policy is inspectable, but are not
standalone distribution claims until their complete dependency chain has
passed from an extracted archive on each host.
