#!/usr/bin/env python3
"""Fetch/configure/build the libstdc++ third_party runtime recipes.

Drives libstdc++/third_party/{libunwind,libcxxabi,libcxx}/recipe.json --
each a small, declarative description of one imported LLVM/Android C++
runtime component -- through the same tools/crt-cc/tools/crt-c++ compiler
wrappers every other CRT target and porting recipe already uses. This
replaces the previous hardcoded tools/fetch_libcxx_runtimes.py and
tools/build_libcxx_runtimes.py: those scripts had every CMake flag, source
URL, and per-OS branch baked directly into Python, duplicated no small
amount of Windows/Darwin build-shim logic other tools already had, and
gave a future contributor no single place to see "what does building
libcxx actually take" without reading the script itself.

This is a *dedicated* engine, not a reuse of tools/crt-port-build.py's
generic porting-recipe engine (porting/recipes/*.json): the two build
shapes are genuinely different (git source with a sparse subpath vs. a
tarball+sha256; three CMake projects with a strict, cross-referencing
build order vs. one independent ./configure-and-make each; a single
shared install prefix all three stage into together vs. one prefix per
port) and forcing them through the same schema/script would have made
both harder to read, not easier.

Recipe JSON shape (schema_version 1):
{
  "schema_version": 1,
  "name": "<matches the containing directory name>",
  "source": {
    "type": "git",
    "repository": "<git URL>",
    "ref": "refs/heads/main",
    "sparse_paths": ["<repo-relative dir(s) to check out>"],  // optional;
        // omit for a repo that IS the component (libcxx, libcxxabi);
        // required for a monorepo where only a subdirectory is wanted
        // (libunwind lives inside AOSP's full toolchain/llvm-project
        // checkout, which also carries clang/llvm/compiler-rt/... --
        // sparse-checkout keeps the fetch to tens of MB instead of the
        // full monorepo).
    "checkout_subdir": "<path inside the clone that IS the component>",
        // required alongside sparse_paths; the source this recipe
        // actually builds is repo_root/checkout_subdir, not the clone
        // root itself.
    "extra_checkout_dirs": ["<sibling dir>", ...]  // optional; extra
        // top-level clone dirs to also copy, as siblings of every
        // recipe's own checkout_dir directly under the shared source
        // root -- for a source that is not self-contained and expects
        // shared monorepo-sibling directories at a relative path (see
        // libunwind/recipe.json's own notes for a real example: shared
        // LLVM CMake utility modules it include()s via "../cmake").
  },
  "dependencies": ["<other recipe name>", ...],  // built (and, if
      // relevant, configured with @SOURCE:@ substitution available)
      // strictly before this one; a dependency disabled for the active
      // target_os (see below) is silently skipped, not an error, so
      // libcxxabi can depend on libunwind and still build correctly on
      // macOS, which never enables libunwind at all.
  "target_os": ["linux", "windows"],  // omit for "every OS"; present
      // means an explicit allow-list -- this recipe (and any dependency
      // edge onto it) is skipped entirely on any OS not listed.
  "cmake": {
    "options": ["-DFOO=ON", ...],  // applied on every enabled OS
    "target_overrides": {
      "<os>": { "options": ["-DBAR=ON", ...] }  // appended after the
          // common options above, OS-specific
    },
    "build_targets": ["cxx", "cxx_filesystem"]  // cmake --build --target
        // for each, in order; omit for a plain default-target build
  },
  "patches": [  // optional; same shape and semantics as
                // tools/crt-port-build.py's apply_source_patches --
                // exact-text substring find/replace against the fetched
                // source tree, applied once per configure (idempotent:
                // re-running configure on an already-patched tree is a
                // no-op find/replace via already-patched text simply not
                // matching "find" a second time, which is intentionally
                // treated as already-applied rather than an error -- see
                // apply_patches() below)
    {"file": "src/CMakeLists.txt", "find": "...", "replace": "...", "reason": "..."}
  ],
  "extra_files": [  // optional; write a small new file into the fetched
                     // source tree before configuring (e.g. a shim
                     // source file a patch above then wires into the
                     // build) -- content is written verbatim, UTF-8
    {"path": "src/__crt_dso_handle.cpp", "content": "..."}
  ],
  "post_install_copy": {  // optional per-OS: extra files to copy from
                           // the source tree into the shared install
                           // prefix after `cmake --install` succeeds
    "macos": [{"from": "lib/notweak.exp", "to": "lib/libc++.notweak.exp"}]
  },
  "notes": ["free-text provenance/troubleshooting notes, not consumed by tooling"]
}

@SOURCE:<recipe-name>@ and @INSTALL_PREFIX@ tokens are substituted into
every string in "cmake.options"/"cmake.target_overrides.*.options" (not
elsewhere -- these two are the only cross-recipe/shared-path references
this build actually needs, matching exactly the two cross-references
tools/build_libcxx_runtimes.py's own hardcoded Python previously
expressed as plain f-strings: LIBCXX_CXX_ABI_INCLUDE_PATHS pointing at
libcxxabi's source, and LLVM_EXTERNAL_LIBCXX_SOURCE_DIR pointing back at
libcxx's).
"""

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
from pathlib import Path


def progress(message):
    print(f"[libcxx] {message}", flush=True)


def detect_target_arch(target_arch_arg):
    """Resolves to "aarch64" or "x86_64" for CRT_TARGET_ARCH -- same
    priority chain and reasoning as tools/crt-port-build.py's own
    detect_target_arch(): explicit --target-arch, then platform.machine()
    (a real Win32 API query via Python's own stdlib, reliable regardless
    of which uname this happens to run under). Needed so
    tools/crt-cc/tools/crt-c++'s own uname-based arch-detection fallback
    is never reached at all on Windows: that fallback shells out to a
    real `uname -m`/`uname -s`, which is not on PATH at all for a bare
    "<mksh.exe> <wrapper-script>" CMAKE_C_COMPILER_ARG1 launch (no rootfs
    PATH setup the way tools/crt-port-build.py's own native-Windows-
    configure env otherwise provides) -- confirmed for real: without
    this, `uname -m` inside the wrapper script exits 127 ("command not
    found"), which crt-cc/crt-c++'s own `set -eu` then turns into every
    single compiler invocation failing outright."""
    arch = target_arch_arg or platform.machine()
    arch = arch.lower()
    if arch in ("aarch64", "arm64"):
        return "aarch64"
    if arch in ("x86_64", "amd64", "x64"):
        return "x86_64"
    raise SystemExit(
        f"crt-libcxx-build.py: unrecognized target architecture {arch!r} "
        "(expected aarch64/arm64 or x86_64/amd64/x64; pass --target-arch to override)"
    )


def run(args, cwd=None, env=None, label=None):
    if label:
        progress(f"start {label}")
    print("+", " ".join(str(a) for a in args), flush=True)
    subprocess.run(args, cwd=cwd, env=env, check=True)
    if label:
        progress(f"done {label}")


def load_recipes(recipe_root):
    """Loads every <recipe_root>/<name>/recipe.json, keyed by its "name"
    field (which must match the containing directory name -- a simple
    consistency check that catches a copy-pasted recipe left with its
    donor's old name)."""
    recipes = {}
    for recipe_path in sorted(recipe_root.glob("*/recipe.json")):
        with open(recipe_path, "r", encoding="utf-8") as f:
            recipe = json.load(f)
        dir_name = recipe_path.parent.name
        if recipe["name"] != dir_name:
            raise SystemExit(
                f"{recipe_path}: recipe name {recipe['name']!r} does not match "
                f"its directory name {dir_name!r}"
            )
        recipes[recipe["name"]] = recipe
    return recipes


def recipe_enabled(recipe, target_os):
    allowed = recipe.get("target_os")
    return allowed is None or target_os in allowed


def resolve_build_order(recipes, target_os, selected=None):
    """Topologically sorts recipes by "dependencies", silently dropping any
    recipe (or dependency edge onto one) not enabled for target_os -- see
    the recipe.json docstring above for why that is not an error."""
    ordered = []
    visiting = set()
    visited = set()

    def visit(name):
        if name in visited:
            return
        recipe = recipes.get(name)
        if recipe is None:
            raise SystemExit(f"recipe not found: {name}")
        if not recipe_enabled(recipe, target_os):
            visited.add(name)
            return
        if name in visiting:
            raise SystemExit(f"dependency cycle includes {name}")
        visiting.add(name)
        for dep in recipe.get("dependencies", []):
            visit(dep)
        visiting.discard(name)
        visited.add(name)
        ordered.append(recipe)

    for name in (selected or sorted(recipes)):
        visit(name)
    return ordered


def fetch_recipe(recipe, source_root, rebuild=False):
    source = recipe["source"]
    if source["type"] != "git":
        raise SystemExit(f"{recipe['name']}: unsupported source.type {source['type']!r}")
    clone_dir = source_root / f".clone-{recipe['name']}"
    checkout_dir = source_root / recipe["name"]
    ref = source.get("ref", "refs/heads/main")
    sparse_paths = source.get("sparse_paths")

    if checkout_dir.exists() and not rebuild:
        progress(f"{recipe['name']}: already fetched: {checkout_dir}")
        return

    if sparse_paths:
        # A repo-with-a-subpath fetch (libunwind inside AOSP's full
        # toolchain/llvm-project monorepo mirror): a plain --depth 1 clone
        # would still download the *entire* tree at that one commit
        # (hundreds of MB -- llvm/clang/compiler-rt/... all live in the
        # same repo), so use a real partial clone (--filter=blob:none,
        # which defers file *content* until checkout) plus cone-mode
        # sparse-checkout scoped to just the wanted subpath. Verified this
        # keeps the fetch to ~25MB instead of the full monorepo.
        if clone_dir.exists() and rebuild:
            shutil.rmtree(clone_dir)
        if not clone_dir.exists():
            run(
                ["git", "clone", "--filter=blob:none", "--no-checkout", "--depth", "1",
                 "--branch", ref.removeprefix("refs/heads/"), source["repository"], str(clone_dir)],
                label=f"{recipe['name']}: clone (sparse)",
            )
            run(["git", "sparse-checkout", "init", "--cone"], cwd=clone_dir)
            run(["git", "sparse-checkout", "set"] + sparse_paths, cwd=clone_dir)
            run(["git", "checkout", ref.removeprefix("refs/heads/")], cwd=clone_dir)
        elif rebuild:
            run(["git", "fetch", "--depth", "1", "origin", ref], cwd=clone_dir)
            run(["git", "checkout", "--detach", "FETCH_HEAD"], cwd=clone_dir)
        checkout_subdir = source.get("checkout_subdir")
        if not checkout_subdir:
            raise SystemExit(f"{recipe['name']}: source.sparse_paths requires source.checkout_subdir")
        source_dir = clone_dir / checkout_subdir
        if not source_dir.is_dir():
            raise SystemExit(f"{recipe['name']}: checkout_subdir {checkout_subdir!r} not found under {clone_dir}")
        if checkout_dir.exists():
            shutil.rmtree(checkout_dir)
        shutil.copytree(source_dir, checkout_dir, ignore=shutil.ignore_patterns(".git"))
        # extra_checkout_dirs: some fetched sources are not self-contained
        # and include() shared sibling directories from the same monorepo
        # by relative path (libunwind -- see its own recipe.json notes,
        # which needs top-level cmake/ and runtimes/cmake/ alongside it).
        # Copied as siblings of every recipe's own checkout_dir, directly
        # under source_root, matching the relative path shape the fetched
        # source itself expects; shared across recipes (harmless to copy
        # more than once, skipped when --rebuild is not requested and the
        # directory already exists from an earlier recipe's fetch).
        for extra_dir in source.get("extra_checkout_dirs", []):
            dest = source_root / extra_dir
            if dest.exists() and not rebuild:
                continue
            src = clone_dir / extra_dir
            if not src.is_dir():
                raise SystemExit(f"{recipe['name']}: extra_checkout_dirs entry {extra_dir!r} not found under {clone_dir}")
            if dest.exists():
                shutil.rmtree(dest)
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(src, dest, ignore=shutil.ignore_patterns(".git"))
    else:
        if not checkout_dir.exists():
            run(
                ["git", "clone", "--depth", "1", "--branch", ref.removeprefix("refs/heads/"),
                 source["repository"], str(checkout_dir)],
                label=f"{recipe['name']}: clone",
            )
        elif rebuild:
            run(["git", "fetch", "--depth", "1", "origin", ref], cwd=checkout_dir)
            run(["git", "checkout", "--detach", "FETCH_HEAD"], cwd=checkout_dir)


def apply_patches(recipe, checkout_dir):
    for extra in recipe.get("extra_files", []):
        target = checkout_dir / extra["path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists():
            target.write_text(extra["content"], encoding="utf-8")
            progress(f"{recipe['name']}: wrote {extra['path']}")
    for patch in recipe.get("patches", []):
        target = checkout_dir / patch["file"]
        text = target.read_text(encoding="utf-8")
        find = patch["find"]
        if patch["replace"] in text and find not in text:
            # Already applied by a previous run -- see the recipe.json
            # docstring's "idempotent" note above.
            continue
        if find not in text:
            raise SystemExit(
                f"{recipe['name']}: patch target text not found in {patch['file']} "
                f"(upstream source changed? patch: {patch.get('reason', '(no reason given)')})"
            )
        target.write_text(text.replace(find, patch["replace"], 1), encoding="utf-8")
        progress(f"{recipe['name']}: patched {patch['file']}")


_TOKEN_PATTERN = re.compile(r"@(SOURCE:([A-Za-z0-9_-]+)|INSTALL_PREFIX)@")


def substitute_tokens(value, source_root, install_prefix):
    def replace(match):
        if match.group(1) == "INSTALL_PREFIX":
            return str(install_prefix)
        return str(source_root / match.group(2))

    return _TOKEN_PATTERN.sub(replace, value)


def cmake_options_for(recipe, target_os, source_root, install_prefix):
    cmake = recipe.get("cmake", {})
    options = list(cmake.get("options", []))
    options += cmake.get("target_overrides", {}).get(target_os, {}).get("options", [])
    return [substitute_tokens(opt, source_root, install_prefix) for opt in options]


def find_windows_host_tool(names):
    """Same purpose as tools/crt-port-build.py's own
    find_windows_host_tool(): resolve a real host tool (clang(++), llvm-
    ar, llvm-ranlib) *before* PATH gets restricted to the rootfs's own
    POSIX-shaped entries for the mksh.exe compiler launcher (see
    common_cmake_args()) -- once that restriction is in effect, a bare
    "clang"/"llvm-ar" lookup would only ever find something under the
    rootfs (nothing is there under those names), not the real host
    compiler tools/crt-cc/tools/crt-c++ themselves need to invoke.
    Returns a forward-slash path: mksh's own exec logic only treats an
    argument as a literal path (bypassing its own $PATH search) when it
    contains at least one "/" -- a bare backslash-only Windows path (e.g.
    "C:\\Program Files\\LLVM\\bin\\clang++.exe", exactly what
    shutil.which()/Path() produce on Windows) has none at all, so mksh
    instead treats the *entire* string as a single command name to search
    $PATH for and reports it "inaccessible or not found", confirmed for
    real by isolating this down to a two-line repro script. Plain forward
    slashes fix this outright, spaces included -- no 8.3 short-path
    workaround needed once mksh actually recognizes the argument as a
    path at all."""
    for name in names:
        found = shutil.which(name)
        if found:
            return Path(found).as_posix()
    for root in (os.environ.get("ProgramFiles"), os.environ.get("ProgramFiles(x86)")):
        if not root:
            continue
        for name in names:
            candidate = Path(root) / "LLVM" / "bin" / name
            if candidate.exists():
                return candidate.as_posix()
    return None


def compiler_supports_flag(compiler, flag):
    if compiler is None:
        return False
    try:
        probe = subprocess.run(
            [str(compiler), "-x", "c++", "-", "-fsyntax-only", flag],
            input="int main(){return 0;}\n",
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError:
        return False
    return probe.returncode == 0


def common_cmake_args(root, install_prefix, sysroot, rootfs, target_os, windows_sdk_libpath, env):
    # The external projects are intentionally compiled through the CRT
    # wrappers, exactly as tools/build_libcxx_runtimes.py already did.
    # -U__APPLE__ prevents an accidental macOS SDK personality; pthread is
    # this project's Bionic-shaped public ABI on all three hosts. Android's
    # current libc++abi has a startup guard for Clang's typed new/delete
    # optimization -- disable that optimization for this standalone
    # runtime build so static initialization cannot call the guarded
    # operator new before libc++ has initialized its dispatch state. Some
    # host Clang toolchains reject the flag entirely, so only add it when
    # the active compiler supports it.
    cxx_flags = "-D__BIONIC__"
    toolchain_cxx = (
        os.environ.get("CRT_HOST_CXX") or shutil.which("clang++") or shutil.which("clang++-18") or shutil.which("c++")
    )
    if compiler_supports_flag(toolchain_cxx, "-fno-typed-cxx-new-delete"):
        cxx_flags += " -fno-typed-cxx-new-delete"
    if target_os == "macos":
        cxx_flags = f"-U__APPLE__ {cxx_flags}"

    c_compiler = root / "tools" / "crt-cc"
    cxx_compiler = root / "tools" / "crt-c++"
    compiler_arg_options = []
    if target_os == "windows":
        # A native Windows process cannot execute the wrappers' shebangs.
        # tools/crt-cc.cmd/tools/crt-c++.cmd are small native-executable
        # launchers built for exactly this: point CMAKE_C_COMPILER/
        # CMAKE_CXX_COMPILER straight at them (a single, plain, directly-
        # executable path each -- no CMAKE_<LANG>_COMPILER_ARG1 needed at
        # all). ARG1 was the first approach tried here and does work for
        # CMake's initial compiler *identification* probe, but not
        # reliably for every later CMake-driven TryCompile -- confirmed
        # for real: CMAKE_CXX_COMPILER_ARG1 silently failed to reach the
        # "Detecting CXX compiler ABI info" TryCompile (the actual
        # generated build command ran mksh.exe with no crt-c++ argument at
        # all), even though the exact same mechanism worked correctly for
        # C in the same configure run. The .cmd wrappers still need
        # mksh.exe to actually run tools/crt-cc/tools/crt-c++'s own
        # shebang scripts, passed via CRT_MKSH_EXE (an env var, not a
        # CMake cache entry, so there is nothing here for CMake's own
        # ARG1 handling to drop). Must be the *rootfs* copy of mksh.exe,
        # not the sysroot's: only the rootfs has toybox's applet aliases
        # actually installed (system/bin/printf and friends), which
        # tools/crt-cc/tools/crt-c++ themselves call -- confirmed for
        # real: the sysroot's own mksh.exe (present there for unrelated
        # reasons) has no toybox alongside it at all, so every compile
        # failed with "printf: inaccessible or not found" the moment the
        # wrapper script tried to use it.
        if rootfs is None:
            raise SystemExit("--rootfs is required on Windows (its mksh.exe is the CMake compiler launcher)")
        mksh = rootfs / "system" / "bin" / "mksh.exe"
        if not mksh.is_file():
            raise SystemExit(f"CRT mksh is missing from the rootfs: {mksh} (build the \"rootfs\" target first)")
        env["CRT_MKSH_EXE"] = str(mksh)
        c_compiler = root / "tools" / "crt-cc.cmd"
        cxx_compiler = root / "tools" / "crt-c++.cmd"
        # PATH inside `env` (set by main() before this call) is restricted
        # to the rootfs's own POSIX-shaped entries, same as tools/crt-
        # port-build.py's native-Windows-configure env -- so tools/crt-cc/
        # tools/crt-c++'s own bare "clang"/"clang++" fallback (when
        # CRT_HOST_CC/CRT_HOST_CXX aren't set) would never find the real
        # host compiler once that restriction is in effect. Resolve both,
        # plus CMake's own AR/RANLIB (whose auto-detection heuristics key
        # off a directly-visible compiler executable, which the .cmd
        # wrappers above are not), from the *real* PATH now, before it is
        # gone from the child process's view.
        host_cc = env.get("CRT_HOST_CC") or find_windows_host_tool(("clang.exe", "clang"))
        host_cxx = env.get("CRT_HOST_CXX") or find_windows_host_tool(("clang++.exe", "clang++"))
        if host_cc:
            env["CRT_HOST_CC"] = host_cc
        if host_cxx:
            env["CRT_HOST_CXX"] = host_cxx
        llvm_ar = find_windows_host_tool(("llvm-ar.exe", "llvm-ar"))
        llvm_ranlib = find_windows_host_tool(("llvm-ranlib.exe", "llvm-ranlib"))
        # Same PATH-restriction reasoning as above, but for CMake's own
        # Ninja generator lookup rather than anything tools/crt-cc/tools/
        # crt-c++ do: "-G Ninja" still needs to find a real ninja.exe, and
        # find_windows_host_tool()'s own Program Files/LLVM fallback does
        # not apply here (ninja is not an LLVM component) -- fall back to
        # the CMAKE_MAKE_PROGRAM this project's own preset already
        # resolved, via the CMAKE_MAKE_PROGRAM cache entry in this
        # preset's own build directory.
        ninja = shutil.which("ninja") or shutil.which("ninja.exe")
        if llvm_ar:
            compiler_arg_options.append(f"-DCMAKE_AR={Path(llvm_ar).as_posix()}")
        if llvm_ranlib:
            compiler_arg_options.append(f"-DCMAKE_RANLIB={Path(llvm_ranlib).as_posix()}")
        if ninja:
            compiler_arg_options.append(f"-DCMAKE_MAKE_PROGRAM={Path(ninja).as_posix()}")

    return [
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        f"-DCMAKE_C_COMPILER={c_compiler}",
        f"-DCMAKE_CXX_COMPILER={cxx_compiler}",
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
        f"-DCMAKE_INSTALL_PREFIX={install_prefix}",
        "-DCMAKE_BUILD_TYPE=Debug",
        f"-DCMAKE_CXX_FLAGS={cxx_flags}",
    ] + compiler_arg_options


def configure_recipe(cmake, recipe, source_root, build_root, install_prefix, common, target_os, env, root):
    checkout_dir = source_root / recipe["name"]
    if not (checkout_dir / "CMakeLists.txt").is_file():
        raise SystemExit(f"{recipe['name']}: source is missing: {checkout_dir}; run --phase fetch first")
    apply_patches(recipe, checkout_dir)
    build_dir = build_root / recipe["name"]
    build_dir.mkdir(parents=True, exist_ok=True)
    options = cmake_options_for(recipe, target_os, source_root, install_prefix)
    # cmake.driver: some fetched sources (libunwind -- see its own
    # recipe.json notes) never call project() themselves and only expect
    # to be add_subdirectory()'d from a parent LLVM "runtimes" driver this
    # project's sparse-checkout fetch never has. When set, configure a
    # small project-owned driver CMakeLists.txt (repo-relative path)
    # instead of the fetched source directly, telling it where the real
    # source landed via -DCRT_<NAME>_SOURCE_DIR (uppercased, "-" -> "_").
    driver = recipe.get("cmake", {}).get("driver")
    configure_source = (root / driver) if driver else checkout_dir
    driver_options = []
    if driver:
        var_name = "CRT_" + recipe["name"].upper().replace("-", "_") + "_SOURCE_DIR"
        driver_options.append(f"-D{var_name}={checkout_dir}")
    run(
        [cmake, "-S", str(configure_source), "-B", str(build_dir), "-G", "Ninja"] + common + options + driver_options,
        env=env,
        label=f"{recipe['name']}: configure",
    )


def build_recipe(cmake, recipe, source_root, build_root, install_prefix, common, target_os, env, root):
    build_dir = build_root / recipe["name"]
    if not (build_dir / "build.ninja").is_file():
        configure_recipe(cmake, recipe, source_root, build_root, install_prefix, common, target_os, env, root)
    targets = recipe.get("cmake", {}).get("build_targets")
    build_cmd = [cmake, "--build", str(build_dir)]
    if targets:
        build_cmd += ["--target"] + targets
    run(build_cmd, env=env, label=f"{recipe['name']}: build")
    run([cmake, "--install", str(build_dir)], env=env, label=f"{recipe['name']}: install")

    checkout_dir = source_root / recipe["name"]
    for copy in recipe.get("post_install_copy", {}).get(target_os, []):
        src = checkout_dir / copy["from"]
        dst = install_prefix / copy["to"]
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        progress(f"{recipe['name']}: post-install copy {copy['from']} -> {copy['to']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--recipe-root", required=True, type=Path)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--build-root", required=True, type=Path)
    parser.add_argument("--install-prefix", required=True, type=Path)
    parser.add_argument("--sysroot", required=True, type=Path)
    parser.add_argument("--rootfs", type=Path, help="required on Windows -- see common_cmake_args()")
    parser.add_argument("--target-os", required=True, choices=["linux", "macos", "windows"])
    parser.add_argument("--windows-sdk-libpath", type=Path)
    parser.add_argument("--target-arch", help="aarch64/x86_64; auto-detected via platform.machine() if omitted")
    parser.add_argument("--phase", required=True, choices=["fetch", "configure", "build"])
    parser.add_argument("--recipe", action="append", help="recipe name to process; defaults to all")
    parser.add_argument("--rebuild", action="store_true")
    args = parser.parse_args()

    root = args.root.resolve()
    recipe_root = args.recipe_root.resolve()
    source_root = args.source_root.resolve()
    build_root = args.build_root.resolve()
    install_prefix = args.install_prefix.resolve()
    sysroot = args.sysroot.resolve()
    rootfs = args.rootfs.resolve() if args.rootfs else None
    source_root.mkdir(parents=True, exist_ok=True)

    recipes = load_recipes(recipe_root)
    order = resolve_build_order(recipes, args.target_os, args.recipe)
    if not order:
        progress(f"nothing to do for target-os={args.target_os} (all recipes disabled)")
        return

    if args.phase == "fetch":
        for recipe in order:
            fetch_recipe(recipe, source_root, rebuild=args.rebuild)
        return

    cmake = shutil.which("cmake") or "cmake"
    env = os.environ.copy()
    if args.target_os == "windows":
        if rootfs is None:
            raise SystemExit("--rootfs is required on Windows")
        env["CRT_TARGET_ARCH"] = detect_target_arch(args.target_arch)
        # CRT_ROOTFS + a POSIX-shaped PATH: mksh.exe (the CMake compiler
        # launcher on Windows -- see common_cmake_args()) resolves "/xxx"
        # PATH entries relative to CRT_ROOTFS internally, the same
        # convention tools/crt-port-build.py's own native-Windows-
        # configure env already relies on for the identical reason
        # (finding toybox's applet aliases, e.g. printf, that tools/
        # crt-cc/tools/crt-c++ themselves call).
        env["CRT_ROOTFS"] = str(rootfs)
        env["PATH"] = "/system/bin:/bin:/usr/bin"
    env["CRT_SYSROOT"] = str(sysroot)
    env["CRT_TARGET_OS"] = args.target_os
    env["CRT_CXX_ENABLE_EXCEPTIONS"] = "1"
    env["CRT_CXX_ENABLE_RTTI"] = "1"
    env["CRT_CXX_BUILDING_RUNTIME"] = "1"
    if args.windows_sdk_libpath:
        env["CRT_WINDOWS_SDK_LIBPATH"] = str(args.windows_sdk_libpath.resolve())

    common = common_cmake_args(root, install_prefix, sysroot, rootfs, args.target_os, args.windows_sdk_libpath, env)

    if args.phase == "configure":
        for recipe in order:
            configure_recipe(cmake, recipe, source_root, build_root, install_prefix, common, args.target_os, env, root)
        return

    for recipe in order:
        build_recipe(cmake, recipe, source_root, build_root, install_prefix, common, args.target_os, env, root)


if __name__ == "__main__":
    main()
