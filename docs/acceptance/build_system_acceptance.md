# Build-system consolidation acceptance

## Scope and ownership

This record covers the in-tree build entry points, not renewed acceptance of
the isolated SDK chain. [The target catalog](../guides/build_targets.json)
owns user-facing target names; [distribution](../guides/distribution.md) owns
SDK construction and isolation. [TODO](../../TODO.md#build-system-consolidation)
owns the ordered B0-B5 work. Existing Full SDK and proposed Development SDK
production remain separate from release publication approval.

## B0 baseline review (2026-10-22)

Reviewed commits `0208bcd` (preset features and build-time dependencies),
`027b5db` (stage caching), `0443910` (target catalog/classifier/CI) and
`aa81932` (measured OS differences), from a clean worktree at `aa81932`.

The supplied five-host CI report states that all 17 user-facing targets exist,
Linux and Windows each have equal target sets across their two architectures,
and the per-OS differences match `internal_os_scope` in the catalog. That is
prior reported inventory evidence, not five fresh builds performed here.
The CI workflow runs the classifier after its C-stage workflow and uploads
the inventory; it does not build every listed upper-stage target.

Local Windows/x64 replay:

```powershell
python -m unittest -v tools.test_list_cmake_targets tools.test_crt_stage_cache tools.test_lvgl_sources
cmake --fresh --preset windows-host-ninja-debug -B out/build-system-review
python tools/list_cmake_targets.py out/build-system-review --check --json out/build-system-review/targets-baseline.json
```

All 18 baseline tool tests pass. Fresh configure succeeds without pre-fetched Skia/LVGL
or manual option changes, registering 209 tests. The resulting Windows tree
contains 17 user-facing and 378 internal names, with no missing or unlisted
stage-like targets. Target counts are observations of this configuration, not
portable constants. No upper-stage compilation is established by configure.

Built `crt-c-build --parallel 8` from this fresh tree, including the recipe-built
Make and CRT-owned shell, then ran `crt-c-test`: 163 selected tests, 162 executed
passes and the existing `fork_heap_snapshot_test_runs` skip, zero failures,
41.18 seconds. Logs are `out/build-system-review/core-build.log` and
`out/build-system-review/core-test.log`. Native processes ran without visible
console windows, outside the sandbox's native-launch restriction.

### Findings and limits

- The five internal Wayland/xkbcommon entry points are still defined on all
  operating systems. The catalog documents this but does not prevent execution;
  they are not macOS/Windows backend targets. Preserve explicit
  cross-build uses before restricting their definitions.
- Recent build-system commits had no matching HISTORY entries and no active
  TODO checklist. This review restores that tracking instead of claiming their
  implementation was absent.
- Source hashing is useful, but the existing cache only checks whether declared
  output paths exist, often a directory. Removing a DLL or archive inside that
  directory can leave a false hit. CMake passes only a subset of build-affecting
  options, and the input globs exclude executed `test_*.py` drivers. B3 must
  tighten these boundaries before release targets rely on the cache. Two
  temporary-fixture probes reproduce false hits: deleting `payload/runtime.dll`
  inside a declared output directory skips the second run, as does changing an
  executed `tools/test_driver.py` from success to exit 7. Both still return
  success without executing their commands. This review does not fix that cache.
- `crt-libcxx-test` delegates to an uncached smoke/sysroot target. Do not describe
  all stage tests as cached. The classifier's current OS-specific exemption also
  was corrected in B1 to require an OS-specific target on its declared OS.
- `crt-web-*`, `crt-dev-release` and `crt-full-release` remain planned, not
  implemented. Development release assembly depends on the SDK split's S0-S3;
  an alias for `02-cxx` would omit the required networking ports.

## B1 configuration entry-point contract

`cmake --preset <preset>` remains the initial configure step. A
`crt-<stage>-config` target is a bounded preflight of that already configured
tree: it reports the effective target pair, compiler and cumulative feature
requirements and writes a local report under `stage-config/`. It neither
changes cached feature options nor starts downloads, target compilation or
nested CMake configuration. Third-party configure actions remain ordered
inside their existing build targets.

The public preflight requires the complete advertised stage. It fails with
the missing CMake option and the explicit reconfigure command when features
were intentionally disabled; internal and reduced-feature diagnostic build
targets remain usable without that strict public preflight. Web additionally
rejects unsupported target pairs, notably Windows/aarch64. A successful
preflight is not dependency build, runtime, hardware or release acceptance.

## B1 local implementation and replay (2026-10-22)

Added six preflight targets through `cmake/crt_stage_config.cmake` and
`tools/crt_stage_config.py`. They are opt-in and do not add dependencies to the
existing build targets. Reports are private build-tree diagnostics, not a new
SDK manifest/schema or CLI capability. No SDK/consumer lock is changed.

On a fresh Windows/x64 configure in `out/build-system-config spaces`, all six
targets pass, and no `external`, `port-tests`, `lvgl`, `rootfs` or `dist` tree
is created by preflight. With LVGL explicitly OFF, C and media configs still
pass, UI config rejects the missing option and removes its older success
report. Reapplying the preset restores the full configuration and all six
preflights pass again. That replay caught and fixed bare compiler names in a
reapplied preset's cache: CMake supplies its resolved tools, with PATH resolution
only as the direct-script fallback. No compiler is executed by the preflight.

Fresh Windows/aarch64 **cross configuration only**, in
`out/build-system-config-arm64`, passes the five lower-stage preflights and
rejects Web explicitly. The x64 and aarch64 inventories match: 23 public and
378 internal targets each. The configure reports missing aarch64 compiler-rt
builtins in this local installation; no aarch64 compilation or execution is
claimed. This is not a claim that every external build prerequisite was found.

Final automated checks: 30 unit tests cover stage requirements, missing tools,
wrong source/target identity, Linux libva, unsupported Web pairs, spaces, stable
reports, stale-report removal, resolved-vs-cached compilers, target catalog and
cache behavior, and the LVGL source list. README/catalog parity is tested;
the classifier now checks OS-scoped public targets on their intended OS.
These tests and C/C++ preflights join the **existing** five-host CI job, not a
second workflow/build. The changed CI and native Linux/macOS preflights have
not been run here; the native Windows C-stage result above is the build replay.

### Reproduction entry points

```powershell
python -m unittest tools.test_crt_stage_config tools.test_list_cmake_targets tools.test_crt_stage_cache tools.test_lvgl_sources
cmake --fresh --preset windows-host-ninja-debug -B "out/build-system-config spaces"
cmake --build "out/build-system-config spaces" --target crt-c-config crt-libcxx-config crt-gfx-simple-config crt-gfx-media-config crt-ui-config crt-web-config
python tools/list_cmake_targets.py "out/build-system-config spaces" --check
cmake --fresh --preset windows-host-ninja-debug -B out/build-system-config-arm64 -DCRT_TARGET_ARCH=aarch64
cmake --build out/build-system-config-arm64 --target crt-web-config
```

The last command must fail with the unsupported Windows/aarch64 message.
For the feature-off negative case, reconfigure the spaces tree with
`-DCRTUI_ENABLE_LVGL=OFF` and require `crt-ui-config` to fail; restore it by
reapplying the preset. B2-B5 stay open. In particular no Web aggregate,
Development SDK package, release target, upload, or new upper-stage acceptance
is claimed by this tranche.
