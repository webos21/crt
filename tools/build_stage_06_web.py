#!/usr/bin/env python3
"""Isolated 05-ui -> 06-web development SDK builder.

Only the extracted source asset, installed predecessor, verified upstream source
and build-host tools are inputs. No in-tree runtime build is used. Publication
is deliberately gated separately from generating a development candidate.
"""
import argparse
import ast
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import shlex
import subprocess
import sys
import tarfile
import time

from build_stage_05_ui import runtime_env, remove_staged_absolute_elf_rpaths, publish_tree
from build_stage_04_gfx_media import tree_inventory, inventory_fingerprint
from crt_dist_prerequisites import external_prerequisites_for
from crt_elf import set_runtime_paths
from crt_linux_runtime import normalize_cxx_aliases
import crt_macho
from create_dist import DIST_PORTING_TOOLS
from fetch_ports import load_recipes, resolve_recipe_order
import build_webkit_crt_port as web
import check_webkit_security as security
import test_crtweb_network as network


def run(command, env=None, cwd=None):
    print("+", " ".join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), env=env, cwd=cwd, check=True)


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def target_path(path, target_os):
    # CRT ports (notably fontconfig's absolute-path test) use /c/... on
    # Windows. Native build-host fixture paths must cross that boundary too.
    return web.crt_path(path) if target_os == "windows" else str(path)


def compile_input(name):
    """Source identity for a packaging-only retry, never an object cache key."""
    if name in ("distribution/stages/06-web/CMakeLists.txt", "examples/web-browser/CMakeLists.txt", "examples/web-browser/main.cc"):
        # These two-object consumer projects are freshly configured below;
        # Their unconditional clean build below also covers changed sample
        # source, independently of the heavy WebKit/dependency phase reused.
        return False
    return ((name.startswith(("libcrtweb/", "porting/", "distribution/", "examples/web-browser/",
                              "libcrtgfx/", "libcrtmedia/")) and not name.endswith(".md")) or
            name in {"tools/crt-cc", "tools/crt-c++", "tools/build_webkit_jsc.py", "tools/build_webkit_crt_port.py", "tools/fetch_ports.py",
                     "tools/fetch_webkit_commit.py", "tools/crt-port-build.py", "tools/crt_icu_host_pkgdata.py"})


def driver_roots(source):
    for node in ast.walk(ast.parse(source)):
        if isinstance(node, ast.Assign) and any(isinstance(target, ast.Name) and target.id == "selected" for target in node.targets):
            if isinstance(node.value, ast.BinOp) and isinstance(node.value.left, ast.List):
                values = node.value.left.elts
                if all(isinstance(value, ast.Constant) and isinstance(value.value, str) for value in values):
                    return {value.value for value in values}
    raise ValueError("cannot identify explicit target-port roots")


def compile_driver_identity(source):
    """Ignore notice/SBOM/packaging repairs, not dependency/configure changes."""
    tree = ast.parse(source)
    selected = []
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in ("run", "host_environment", "build_host_tools", "build_windows_host_tools", "windows_fontconfig_recipe"):
            selected.append(node)
        if isinstance(node, ast.FunctionDef) and node.name == "main":
            active = False
            for statement in node.body:
                if isinstance(statement, ast.Assign):
                    names = [target.id for target in statement.targets if isinstance(target, ast.Name)]
                    if "recipes" in names:
                        active = True
                    if "build" in names:
                        break
                if active:
                    selected.append(statement)
    if len(selected) < 10:
        raise ValueError("cannot identify the stage compile phase")
    # Recompose/reconfigure is mandatory on a packaging retry. It changes no
    # compile option and may safely be added to the earlier invocation.
    class Normalize(ast.NodeTransformer):
        def visit_Assign(self, node):
            if any(isinstance(target, ast.Name) and target.id == "selected" for target in node.targets):
                if isinstance(node.value, ast.BinOp) and isinstance(node.value.left, ast.List):
                    node.value.left.elts = []  # additive roots are checked separately
            return self.generic_visit(node)

        def visit_List(self, node):
            # CMake FindCURL can append pkg-config's shared curl even when
            # IMPORTED_LOCATION_RELEASE names the static archive. Binding
            # that discovery alias to the SAME already-selected archive is
            # a link-only packaging repair, not a new dependency/configuration.
            static = [item.right for item in node.elts if isinstance(item, ast.BinOp)
                      and isinstance(item.left, ast.Constant) and item.left.value == "CURL_LIBRARY_RELEASE="]
            items, index = [], 0
            while index < len(node.elts):
                item = node.elts[index]
                following = node.elts[index + 1] if index + 1 < len(node.elts) else None
                if (static and isinstance(item, ast.Constant) and item.value == "--define"
                    and isinstance(following, ast.BinOp) and isinstance(following.left, ast.Constant)
                    and following.left.value == "pkgcfg_lib_PC_CURL_curl="
                    and ast.dump(following.right) == ast.dump(static[0])):
                    index += 2
                    continue
                if not isinstance(item, ast.Constant) or item.value != "--recompose":
                    items.append(item)
                index += 1
            node.elts = items
            return self.generic_visit(node)
    return ast.dump(Normalize().visit(ast.Module(body=selected, type_ignores=[])), include_attributes=False)


def validate_packaging_retry(archive, old_sha, asset):
    if digest(archive) != old_sha:
        raise ValueError("previous source asset does not match the recorded build input")
    previous = {}
    old_driver = None
    with tarfile.open(archive, "r:xz") as stream:
        for member in stream:
            if not member.isfile():
                continue
            if compile_input(member.name):
                previous[member.name] = hashlib.sha256(stream.extractfile(member).read()).hexdigest()
            elif member.name == "tools/build_stage_06_web.py":
                old_driver = stream.extractfile(member).read().decode()
    current = {file.relative_to(asset).as_posix(): digest(file) for file in asset.rglob("*")
               if file.is_file() and compile_input(file.relative_to(asset).as_posix())}
    new_driver = (asset / "tools/build_stage_06_web.py").read_text()
    if previous != current or old_driver is None or not driver_roots(old_driver) <= driver_roots(new_driver) or compile_driver_identity(old_driver) != compile_driver_identity(new_driver):
        raise ValueError("compiled inputs/configuration changed: a packaging-only retry requires a fresh work-root")


def require_predecessor(sdk, manifest):
    if manifest.get("stage") != "05-ui":
        raise ValueError("06-web requires the installed 05-ui SDK")
    names = {entry["name"] for entry in manifest.get("redistributed_dependencies", [])}
    if not {"freetype", "ffmpeg", "skia", "curl", "mbedtls", "zlib", "lvgl"} <= names:
        raise ValueError("06-web predecessor is missing its complete media/UI dependency provenance")
    target_os = manifest.get("target", {}).get("os")
    suffix = "dylib" if target_os == "macos" else "so"
    runtime = ("bin/libc.dll", "bin/libc++.dll", "bin/libcrtmedia.dll") if target_os == "windows" else (
        f"lib/libc.{suffix}", f"lib/libc++.{suffix}", f"lib/libcrtmedia.{suffix}")
    for file in (*runtime, "lib/libskia.a", "lib/libcrtui_skia.a", "include/include/core/SkSurface.h",
                 "include/crtgfx/shared_image.h", "include/crtgfx/skia_shared.h"):
        if not (sdk / file).is_file():
            raise ValueError(f"06-web requires the option-ON 05-ui predecessor: missing {file}")


def validate_layout(sdk, work, output):
    if output.exists():
        raise ValueError(f"output already exists: {output}")
    for left, right in ((sdk, work), (sdk, output), (work, output)):
        if left == right or left in right.parents or right in left.parents:
            raise ValueError("predecessor SDK, work-root and output must be disjoint")


def validate_source_cache(cache, asset):
    """Reject incomplete, dirty or object-bearing checkouts before building ports."""
    import fetch_webkit_commit as source
    pin = source.product_source(json.loads((asset / "libcrtweb/third_party/webkit/recipe.json").read_text()))
    checkout = cache / "checkout"
    if not (checkout / ".git/HEAD").is_file():
        raise ValueError("source cache requires a complete, self-contained Git checkout; omit --source-cache to fetch afresh")
    try:
        source.verify_commit(cache / "WebKit.git", pin)
        if source.git(checkout, "rev-parse", "HEAD").strip() != pin["commit"]:
            raise ValueError("source cache checkout does not match the product pin")
        # Ignored/untracked build outputs are not a source-only cache either:
        # the composer copies the checkout, not just tracked files.
        if source.git(checkout, "status", "--porcelain", "--untracked-files=all", "--ignored").strip():
            raise ValueError("source cache must be pristine and contain no generated/untracked files")
    except subprocess.CalledProcessError as error:
        raise ValueError("source cache Git metadata is incomplete; omit --source-cache to fetch afresh") from error


def install_stage_tools(asset, staged):
    for name in DIST_PORTING_TOOLS:
        shutil.copy2(asset / "tools" / name, staged / "tools" / name)


def copy_private_libraries(directory, destination, target_os):
    # WebKit's Darwin install name ends in .dylib.SOVERSION. Preserve the
    # actual image and relative aliases, not just names ending in .dylib.
    pattern = "*.dll" if target_os == "windows" else "*.dylib*" if target_os == "macos" else "*.so*"
    for file in directory.glob(pattern):
        if file.name.startswith(("libcurl", "libcrtweb", "libc++", "libunwind")) or not file.is_file():
            continue
        target = destination / file.name
        if target.exists() or target.is_symlink():
            continue
        if file.is_symlink():
            if file.resolve().parent != directory.resolve():
                raise ValueError(f"private library alias escapes its source family: {file}")
            target.symlink_to(file.resolve().name)
        else:
            shutil.copy2(file, target)


def audit_target_discovery(cache, sdk, prefix, tree):
    records = {}
    for line in cache.read_text().splitlines():
        if not re.match(r"(?:pkgcfg_lib_)?(?:PC_)?(?:ICU|JPEG|PNG|SQLite3|ZLIB|FONTCONFIG|Fontconfig|FREETYPE|HARFBUZZ|HarfBuzz|WEBP|LIBXML2|LIBPSL|OPENSSL|CURL|WPE|EGL|OpenGLES2).*:(?:FILEPATH|PATH|STRING|UNINITIALIZED|INTERNAL)=", line):
            continue
        name, value = line.split("=", 1)
        if not (name.startswith("pkgcfg_lib_") or any(field in name for field in
                ("LIBRARY", "LIBRARIES", "INCLUDE_DIR", "INCLUDEDIR", "LIBDIR"))):
            continue
        for entry in value.split(";"):
            if entry.startswith("/") or Path(entry).is_absolute():
                # CRT/pkg-config paths use /c/... even in a native Windows
                # CMake cache. Resolve that drive spelling before containment
                # checks; never accept a different drive or sibling/.. escape.
                native = entry
                drive = re.match(r"^/([A-Za-z])(?:/|$)", entry)
                if os.name == "nt" and drive:
                    native = drive[1] + ":/" + entry[drive.end():]
                path = Path(native).resolve()
                if not any(path.is_relative_to(root.resolve()) for root in (sdk, prefix, tree)):
                    raise ValueError(f"target dependency escaped the SDK/prefix/source: {name}={entry}")
        records[name] = value
    if not records:
        raise ValueError("no target dependency discovery evidence in WebKit cache")
    return records


def host_environment():
    # Host tools use host libc/headers by design; none enter the target SDK.
    env = {key: value for key, value in os.environ.items()
           if not key.startswith("CRT_") and key not in
           ("CC", "CXX", "CFLAGS", "CXXFLAGS", "CPPFLAGS", "LDFLAGS", "LD_LIBRARY_PATH",
            "DYLD_LIBRARY_PATH", "PKG_CONFIG_PATH", "PKG_CONFIG_LIBDIR", "CPATH", "LIBRARY_PATH")}
    env.update(CC=shutil.which("clang") or "cc", CXX=shutil.which("clang++") or "c++")
    return env


def build_host_tools(asset, sources, work, recipes, jobs):
    if os.name == "nt":
        return build_windows_host_tools(sources, work, recipes, jobs)
    env = host_environment()
    tools = web.jsc.require_host_tools()
    for name, configure_options in (("gperf", []), ("icu", ["--disable-tests", "--disable-samples",
                                                         "--disable-extras", "--disable-shared", "--enable-static"])):
        build = work / "host" / name
        build.mkdir(parents=True, exist_ok=True)
        source = sources / recipes[name]["source"]["source_dir"]
        if name == "icu":
            source /= "source"
        if not (build / "Makefile").exists():
            run([source / "configure", f"--prefix={build / 'install'}", *configure_options], env, build)
        run(["make", f"-j{jobs}"], env, build)
        if name == "gperf":
            run(["make", "install"], env, build)
    tools.update(gperf=str(work / "host/gperf/install/bin/gperf"),
                 icu_cross_build=str(work / "host/icu"),
                 cc=env["CC"], cxx=env["CXX"])
    tools["host_executables"] = {
        "gperf": tools["gperf"],
        "perl": shutil.which("perl"), "python": sys.executable, "ruby": shutil.which("ruby"),
        **{name: str(work / "host/icu/bin" / name) for name in ("genrb", "icupkg", "pkgdata")},
    }
    write_json(work / "host-tools.json", tools)
    return tools


def build_windows_host_tools(sources, work, recipes, jobs):
    """Native MinGW host generators, never CRT target executables/SDK inputs.

    Git's shell is a hosted configure tool only; target ports still use the
    installed CRT mksh/make and unmodified upstream recipes.
    """
    env = host_environment()
    bash = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Git/bin/bash.exe"
    gcc, gxx = shutil.which("gcc"), shutil.which("g++")
    make = shutil.which("gmake") or shutil.which("mingw32-make")
    if not bash.is_file() or not all((gcc, gxx, make)):
        raise ValueError("Windows host generators require Git Bash and native MinGW gcc/g++/make")
    import ctypes
    shell = bash.parent.parent / "usr/bin/sh.exe"
    needed = ctypes.windll.kernel32.GetShortPathNameW(str(shell), None, 0)
    if not needed:
        raise ctypes.WinError()
    buffer = ctypes.create_unicode_buffer(needed)
    if not ctypes.windll.kernel32.GetShortPathNameW(str(shell), buffer, needed):
        raise ctypes.WinError()
    host_shell = buffer.value.replace("\\", "/")
    perl = shell.parent / "perl.exe"
    if not perl.is_file():
        raise ValueError("Windows OpenSSL configure requires Git's POSIX-path Perl (build-host only)")
    needed = ctypes.windll.kernel32.GetShortPathNameW(str(perl), None, 0)
    if not needed:
        raise ctypes.WinError()
    buffer = ctypes.create_unicode_buffer(needed)
    if not ctypes.windll.kernel32.GetShortPathNameW(str(perl), buffer, needed):
        raise ctypes.WinError()
    posix_perl = buffer.value.replace("\\", "/")
    probe = subprocess.run([posix_perl, "-MFile::Spec", "-e", 'print File::Spec->catfile("foo","bar")'],
                           env=env, capture_output=True, text=True, check=True)
    if probe.stdout != "foo/bar" or " " in posix_perl:
        raise ValueError("OpenSSL host Perl must produce POSIX paths and have a space-free command alias")
    perl_env = dict(env)
    # A configured BUILD-host module search root is not a target dependency.
    # Probe the full requirement before expensive compilation; never install
    # modules globally or silently mix another interpreter's native extensions.
    perl_lib = os.environ.get("CRT_WEB_HOST_PERL_LIB", "")
    if perl_lib:
        perl_lib = Path(perl_lib).resolve()
        if not perl_lib.is_dir():
            raise ValueError("CRT_WEB_HOST_PERL_LIB is not a build-host module directory")
        perl_env["PERL5LIB"] = web.crt_path(perl_lib)
    else:
        perl_env.pop("PERL5LIB", None)
    modules = subprocess.run([posix_perl, "-MIPC::Cmd", "-MExtUtils::MakeMaker", "-e", "print 1"],
                             env=perl_env, capture_output=True, text=True)
    if modules.returncode or modules.stdout != "1":
        raise ValueError("OpenSSL POSIX host Perl needs IPC::Cmd/ExtUtils::MakeMaker; configure "
                         "CRT_WEB_HOST_PERL_LIB for a prepared host module root: " + modules.stderr[-2000:])
    perl_module_inputs = ({file.relative_to(perl_lib).as_posix(): digest(file)
                           for file in perl_lib.rglob("*") if file.is_file()} if perl_lib else {})
    # ICU's make recipes expand $(SHELL) unquoted. Select the installed shell's
    # Windows alias, not a short SDK/work directory; relocation still tests the
    # real spaced SDK path. Never patch the upstream generated/source files.
    if " " in host_shell:
        raise ValueError("ICU host make requires a space-free shell alias (Windows 8.3 name)")
    env.update(CC="gcc", CXX="g++", AR="ar", RANLIB="ranlib", MAKE=Path(make).as_posix())
    env["PATH"] = os.pathsep.join((str(bash.parent.parent / "usr/bin"), str(Path(gcc).parent), env.get("PATH", "")))
    tools = web.jsc.require_host_tools()
    def hosted_shell(command, cwd):
        # Git's bash launcher prepends its own MinGW DLLs after receiving
        # env. Restore this compiler's runtime precedence INSIDE the shell;
        # otherwise a freshly linked ICU tool exits 127 with Git's C++ DLLs.
        script = "export PATH=" + shlex.quote(web.crt_path(Path(gcc).parent)) + ":$PATH; exec " + shlex.join(command)
        run([bash, "--noprofile", "--norc", "-c", script], env, cwd)
    for name, options in (("gperf", []), ("icu", ["--disable-tests", "--disable-samples", "--disable-extras",
                                                "--disable-shared", "--enable-static"])):
        build = work / "host" / name
        build.mkdir(parents=True, exist_ok=True)
        # Host tools must also run from CRT make's native-tool boundary, not
        # only from the Git configure shell. Bind their compiler runtime by
        # adjacency; these GNU DLLs never enter the target dependency prefix.
        runtime_bin = build / ("install/bin" if name == "gperf" else "bin")
        runtime_bin.mkdir(parents=True, exist_ok=True)
        for library in ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"):
            file = Path(gcc).parent / library
            if not file.is_file():
                raise ValueError(f"native host compiler runtime missing: {file}")
            shutil.copy2(file, runtime_bin / library)
        source = sources / recipes[name]["source"]["source_dir"]
        if name == "icu":
            source /= "source"
        commands = []
        if not (build / ".configured").exists():
            hosted_shell([
                (source / "configure").as_posix(), "--build=x86_64-w64-mingw32", "--host=x86_64-w64-mingw32",
                "--srcdir=" + os.path.relpath(source, build).replace("\\", "/"),
                "--prefix=" + (build / "install").as_posix(), *options], build)
            (build / ".configured").write_text("configured\n")
        # ICU's MinGW fragment links -licu* even in static-only mode. Match
        # its supported make-level naming variable, not patched sources or
        # aliases to target libraries. This applies to host generators only.
        host_defines = ["STATIC_PREFIX=", "STATIC_PREFIX_WHEN_USED="] if name == "icu" else []
        commands.append([Path(make).as_posix(), f"-j{jobs}", "SHELL=" + host_shell, *host_defines])
        if name == "gperf":
            commands.append([Path(make).as_posix(), "install", "SHELL=" + host_shell])
        for command in commands:
            hosted_shell(command, build)
    tools.update(gperf=str(work / "host/gperf/install/bin/gperf.exe"), icu_cross_build=web.crt_path(work / "host/icu"),
                 cc=gcc, cxx=gxx, shell=host_shell, posix_perl=posix_perl,
                 posix_perl_sha256=digest(Path(posix_perl)),
                 posix_perl_version=web.jsc.tool_version([posix_perl, "-e", "print $^V"]),
                 posix_perl_lib=web.crt_path(perl_lib) if perl_lib else "",
                 perl_module_inputs=perl_module_inputs,
                 generator_runtime="native MinGW/UCRT; never redistributed")
    tools["runtime_dlls"] = {file.name: digest(file) for file in (work / "host/icu/bin").glob("*.dll")}
    write_json(work / "host-tools.json", tools)
    return tools


def fontconfig_host_generator_recipe(recipe, sdk, work, gperf, target_os):
    """Both bare configure probes and Make's GPERF use the BUILD-host tool."""
    recipe = json.loads(json.dumps(recipe))
    recipe["dependencies"] = [name for name in recipe["dependencies"] if name != "gperf"]
    if target_os != "windows":
        recipe["build"]["env"]["GPERF"] = str(Path(gperf))
        recipe["build"]["path_prepend"] = [str(Path(gperf).parent)]
        return recipe
    relay = work / "host/fontconfig-tools/gperf"
    relay.parent.mkdir(parents=True, exist_ok=True)
    command = ["/system/bin/mksh", web.crt_path(sdk / "tools/crt-native-tool"), web.crt_path(Path(gperf))]
    relay.write_text('#!/system/bin/mksh\nexec ' + shlex.join(command) + ' "$@"\n', encoding="utf-8", newline="\n")
    relay.chmod(0o755)
    recipe["build"]["env"]["GPERF"] = shlex.join(["/system/bin/mksh", web.crt_path(relay)])
    recipe["build"]["path_prepend"] = [web.crt_path(relay.parent)]
    return recipe


def build_kind(target, host_system=None, host_machine=None):
    host_system = host_system or platform.system()
    host_machine = (host_machine or platform.machine()).lower()
    normalized = "x86_64" if host_machine in ("x86_64", "amd64") else "aarch64" if host_machine in ("aarch64", "arm64") else host_machine
    native = ((target == {"os": "linux", "arch": "x86_64"} and host_system == "Linux" and normalized == "x86_64")
              or (target == {"os": "macos", "arch": "aarch64"} and host_system == "Darwin" and normalized == "aarch64")
              or (target == {"os": "windows", "arch": "x86_64"} and host_system == "Windows" and normalized == "x86_64"))
    if native:
        return "native"
    if target == {"os": "linux", "arch": "aarch64"} and host_system == "Linux" and normalized == "x86_64":
        return "cross-linux-aarch64"
    raise ValueError("06-web supports native Linux/x86_64, macOS/aarch64, Windows/x86_64, "
                     "and x86_64-host -> Linux/aarch64-target")


def linux_cross_recipe(recipe):
    """Give Autoconf an explicit cross identity without changing upstream."""
    recipe = json.loads(json.dumps(recipe))
    build = recipe["build"]
    if build["system"] == "configure" and build.get("configure_script", "./configure") == "./configure" \
            and recipe["name"] != "zlib":
        build["configure_args"] += ["--build=x86_64-pc-linux-gnu", "--host=aarch64-linux-gnu"]
    elif recipe["name"] == "zlib":
        build.setdefault("env", {})["CHOST"] = "aarch64-linux-gnu"
    return recipe


def verified_notice_bytes(source, expected_sha256):
    """Ship exact inventory-pinned bytes, accepting only proven CRLF checkout conversion."""
    if not source.is_file():
        raise ValueError(f"WebKit notice missing: {source}")
    data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() == expected_sha256:
        return data
    canonical = data.replace(b"\r\n", b"\n")
    if hashlib.sha256(canonical).hexdigest() != expected_sha256:
        raise ValueError(f"WebKit notice differs from inventory: {source}")
    return canonical


def package_provenance(asset, staged, tree, sources, ordered, manifest, report, args, predecessor):
    provenance = staged / "share/crt/dependencies/webkit"
    provenance.mkdir(parents=True, exist_ok=True)
    recipe_dir = asset / "libcrtweb/third_party/webkit"
    for source in recipe_dir.glob("*.json"):
        shutil.copy2(source, provenance / source.name)
    shutil.copytree(asset / "libcrtweb/patches", provenance / "patches", dirs_exist_ok=True)
    shutil.copytree(asset / "libcrtweb/port", provenance / "port", dirs_exist_ok=True)
    recipe = json.loads((recipe_dir / "recipe.json").read_text())
    notices = []
    for record in json.loads((recipe_dir / "license-inventory.json").read_text())["license_files"]:
        source = tree / record["path"]
        notice = verified_notice_bytes(source, record["sha256"])
        relative = Path("share/licenses/webkit") / record["path"]
        (staged / relative).parent.mkdir(parents=True, exist_ok=True)
        (staged / relative).write_bytes(notice)
        notices.append(relative.as_posix())
    deps = manifest.setdefault("redistributed_dependencies", [])
    windows = manifest["target"]["os"] == "windows"
    suffix = "dylib" if manifest["target"]["os"] == "macos" else "so"
    web_links = ["bin/libcrtweb.dll", "lib/libcrtweb.dll.a"] if windows else [f"lib/libcrtweb.{suffix}"]
    web_runtime = (["libexec/crtweb/lib/libcrtweb.dll", "libexec/crtweb/lib/libcrtmedia.dll"] +
                   ["libexec/crtweb/" + name + ".exe" for name in ("crtweb_host", "WebProcess", "NetworkProcess")]) if windows else [
        f"libexec/crtweb/lib/libWebKit.{suffix}", f"libexec/crtweb/lib/libcrtmedia.{suffix}", "libexec/crtweb/crtweb_host",
        "libexec/crtweb/WebProcess", "libexec/crtweb/NetworkProcess"]
    deps.append({"name": "webkit", "kind": "private-replaceable-web-runtime",
                 "headers": ["include/crtweb/crtweb.h"], "link_artifacts": web_links,
                 "runtime_artifacts": web_runtime,
                 "notices": notices, "provenance": "share/crt/dependencies/webkit/recipe.json"})
    # SPDX 2.3 JSON: package-level composition, not a claim to classify every
    # WebKit file. Mixed/unreviewed expressions remain NOASSERTION; the full
    # per-file scan and bundled-component inventory are shipped alongside it.
    packages = []
    all_records = [("webkit", recipe["version"], recipe["source"]["archive_url"],
                    recipe["source"]["archive_sha256"])]
    for port in ordered:
        if port["name"] in ("make", "gperf"):
            continue
        source = sources / port["source"]["source_dir"]
        relative_root = Path("share/licenses/web-deps") / port["name"]
        port_notices = []
        for file in source.rglob("*"):
            if file.is_file() and re.match(r"^(LICENSE|LICENCE|COPYING|NOTICE|COPYRIGHT)(?:[.\-_]|$)", file.name, re.I):
                destination = relative_root / file.relative_to(source)
                (staged / destination).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(file, staged / destination)
                port_notices.append(destination.as_posix())
        if not port_notices:
            # EGL-Registry keeps its permission notices in the public headers,
            # not in a standalone LICENSE. Ship the exact pinned files.
            if port["name"] == "crt-egl":
                for name in ("api/EGL/egl.h", "api/EGL/eglext.h", "api/EGL/eglplatform.h", "api/KHR/khrplatform.h"):
                    destination = relative_root / name
                    (staged / destination).parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(source / name, staged / destination)
                    port_notices.append(destination.as_posix())
            elif port["name"] == "sqlite-amalgamation":
                # SQLite's public-domain dedication is in its public header.
                destination = relative_root / "sqlite3.h"
                (staged / destination).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source / "sqlite3.h", staged / destination)
                port_notices.append(destination.as_posix())
            else:
                raise ValueError(f"no upstream notice found for {port['name']}")
        port_provenance = Path("share/crt/dependencies/web-" + port["name"]) / "recipe.json"
        write_json(staged / port_provenance, port)
        deps.append({"name": "web-" + port["name"], "kind": "private-web-dependency",
                     "headers": [], "link_artifacts": [], "runtime_artifacts": [],
                     "notices": port_notices, "provenance": port_provenance.as_posix()})
        all_records.append((port["name"], port["version"], port["source"]["url"], port["source"]["sha256"]))
    for index, (name, version, url, sha) in enumerate(all_records):
        packages.append({"SPDXID": f"SPDXRef-Package-{index}", "name": name, "versionInfo": version,
                         "downloadLocation": url, "filesAnalyzed": False,
                         "licenseConcluded": "NOASSERTION", "licenseDeclared": "NOASSERTION",
                         "copyrightText": "NOASSERTION",
                         "checksums": [{"algorithm": "SHA256", "checksumValue": sha}]})
    for dependency in deps:
        if dependency["name"] == "webkit" or dependency["name"].startswith("web-"):
            continue
        inherited = json.loads((staged / dependency["provenance"]).read_text())
        source = inherited.get("source", {})
        package = {"SPDXID": "SPDXRef-Inherited-" + dependency["name"], "name": dependency["name"],
                   "versionInfo": str(inherited.get("version", inherited.get("revision", "NOASSERTION"))),
                   "downloadLocation": source.get("url", source.get("repository", "NOASSERTION")),
                   "filesAnalyzed": False, "licenseConcluded": "NOASSERTION", "licenseDeclared": "NOASSERTION",
                   "copyrightText": "NOASSERTION", "comment": "Inherited unchanged from the installed 05-ui SDK; see its bundled provenance/notice."}
        if source.get("sha256"):
            package["checksums"] = [{"algorithm": "SHA256", "checksumValue": source["sha256"]}]
        packages.append(package)
    write_json(staged / "share/crt/web-sbom.spdx.json", {
        "spdxVersion": "SPDX-2.3", "dataLicense": "CC0-1.0", "SPDXID": "SPDXRef-DOCUMENT",
        "name": "CRT Web dependency composition", "documentNamespace":
        "https://github.com/webos21/crt/sbom/" + args.source_sha256,
        "creationInfo": {"created": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                         "creators": ["Tool: CRT build_stage_06_web.py"]},
        "packages": packages, "relationships": [{"spdxElementId": "SPDXRef-DOCUMENT",
            "relationshipType": "DESCRIBES", "relatedSpdxElement": p["SPDXID"]} for p in packages]})
    write_json(staged / "share/crt/web-security.json", report)
    write_json(staged / "share/crt/web-source-correspondence.json", {
        "recipe": "share/crt/dependencies/webkit/recipe.json",
        "patches": "share/crt/dependencies/webkit/patches/manifest.json",
        "overlay": "share/crt/dependencies/webkit/port",
        "stage_recipe": args.recipe_location, "stage_source_sha256": args.source_sha256,
        "predecessor": predecessor,
        "source_commit": recipe["product_source"]["commit"],
        "rebuild": "tools/crt-stage-build.py with the exact source recipe and 05-ui predecessor",
        "replacement": "Replace the private WebKit/runtime libraries and matching helper processes together; never link them into the application.",
        "public_release_allowed": False})
    manifest["web_runtime"] = {"sbom": "share/crt/web-sbom.spdx.json", "security": "share/crt/web-security.json",
                               "source_correspondence": "share/crt/web-source-correspondence.json",
                               "development_candidate": True}


def portable_web_macho(staged):
    """Give only new Web images package-local dependencies and loader paths.

    The private closure wins inside the browser; public SDK consumers retain
    the predecessor's media/C++ ABI. Never mutate inherited runtime images.
    """
    private = staged / "libexec/crtweb"
    files = [private / name for name in ("crtweb_host", "WebProcess", "NetworkProcess", "crtweb_gpu_worker")]
    files += [staged / "examples/bin/crtweb_browser", staged / "lib/libcrtweb.dylib"]
    files += [file for file in (private / "lib").iterdir() if file.is_file() and not file.is_symlink()]
    for file in files:
        ident = crt_macho.dylib_id(file)
        changes = []
        if ident:
            changes += ["-id", "@rpath/" + file.name]
        for dep in crt_macho.dependencies(file):
            if dep == ident or dep.startswith(("/usr/lib/", "/System/Library/")):
                continue
            leaf = Path(dep).name
            if not (private / "lib" / leaf).is_file() and not (staged / "lib" / leaf).is_file():
                raise ValueError(f"unpackaged Mach-O dependency: {file.name}: {dep}")
            if dep != "@rpath/" + leaf:
                changes += ["-change", dep, "@rpath/" + leaf]
        for value in crt_macho.rpaths(file):
            changes += ["-delete_rpath", value]
        if file.parent == private:
            paths = (["@loader_path/../../lib"] if file.name == "crtweb_gpu_worker"
                     else ["@loader_path/lib", "@loader_path/../../lib"])
        elif file.parent == private / "lib":
            paths = ["@loader_path", "@loader_path/../../../lib"]
        elif file.name == "libcrtweb.dylib":
            paths = ["@loader_path", "@loader_path/../libexec/crtweb/lib"]
        else:
            paths = ["@loader_path/../../lib"]
        # ld64 rejects deleting and adding the same RPATH in one invocation.
        # Separate edits also make this normalization safe to replay.
        if changes:
            run(["install_name_tool", *changes, file])
        additions = [item for value in paths for item in ("-add_rpath", value)]
        run(["install_name_tool", *additions, file])
        run(["codesign", "--force", "--sign", "-", file])


def validate_candidate(staged, work, asset, env):
    """Check a generated candidate without publishing or touching its inputs."""
    private = staged / "libexec/crtweb"
    target_os = env.get("CRT_TARGET_OS", "linux")
    normalize_candidate_paths(staged, target_os)
    candidate_env = dict(env)
    for name in ("LD_LIBRARY_PATH", "LD_PRELOAD", "DYLD_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES", "CRTWEB_PROCESS_DIR", "FONTCONFIG_FILE"):
        candidate_env.pop(name, None)
    windows = target_os == "windows"
    exe = ".exe" if windows else ""
    candidate_env["CRTWEB_HOST"] = str(private / ("crtweb_host" + exe))
    # No build/prefix loader path: the package must work on its own.
    sample_env = dict(candidate_env)
    if windows:
        sample_env["PATH"] = str(Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32")
    run([staged / ("examples/bin/crtweb_browser" + exe), "--smoke"], sample_env, work)
    run([sys.executable, asset / "tools/test_stage_06_browser.py", "--sdk-root", staged,
         "--output", work / "packaged-browser-observation"], candidate_env)
    # Static/private OpenSSL curl is a packaging change: replay actual HTTP
    # redirects/POST/cookies and positive/negative TLS using only the installed
    # C API and installed browser processes, not the WebKit build directory.
    probe = work / ("packaged-network-probe" + exe)
    if windows:
        # A direct C-API import (unlike the static app) needs standard PE
        # package activation before its first instruction. No build prefix.
        candidate_env["PATH"] = os.pathsep.join((str(private / "lib"), str(staged / "bin"), candidate_env.get("PATH", "")))
    runtime_link = [] if windows else ["-Wl,-rpath," + str(staged / "lib")]
    run([staged / ("tools/crt-c++.cmd" if windows else "tools/crt-c++"), asset / "libcrtweb/tests/webkit/crtweb_network_test.cpp",
         "-I" + str(staged / "include"), "-L" + str(staged / "lib"), "-lcrtweb",
         *runtime_link, "-o", probe], candidate_env)
    network_env = dict(candidate_env, CRTWEB_PROCESS_DIR=str(private),
                       FONTCONFIG_FILE=web.crt_path(work / "packaged-browser-observation/fonts.conf") if windows else
                       str(work / "packaged-browser-observation/fonts.conf"))
    network_result = network.run_gate(probe, network_env, work,
                                      path_for_target=lambda path: target_path(path, target_os))
    write_json(work / "packaged-network.json", network_result)
    if not network_result["passed"]:
        raise ValueError("packaged HTTP/TLS regression failed")
    audit = web.jsc.host_abi_audit(staged, staged, private,
        [private / (name + exe) for name in ("crtweb_host", "WebProcess", "NetworkProcess")], candidate_env, target_os)
    write_json(work / "packaged-host-abi.json", audit)
    if not audit["ok"]:
        raise ValueError("packaged browser Host ABI audit failed")
    run([sys.executable, asset / "tools/verify_dist.py", "--dist", staged, "--stage", "06-web"], env)


def normalize_candidate_paths(staged, target_os):
    """Apply package-local loader paths without executing target programs."""
    private = staged / "libexec/crtweb"
    if target_os == "macos":
        portable_web_macho(staged)
    elif target_os == "linux":
        for file in [private / name for name in ("crtweb_host", "WebProcess", "NetworkProcess", "crtweb_gpu_worker")] + [staged / "examples/bin/crtweb_browser"]:
            portable = "$ORIGIN/../../lib" if file.name in ("crtweb_gpu_worker", "crtweb_browser") else "$ORIGIN/lib:$ORIGIN/../../lib"
            set_runtime_paths(file, portable)
        set_runtime_paths(staged / "lib/libcrtweb.so", "$ORIGIN:$ORIGIN/../libexec/crtweb/lib")
        for file in (private / "lib").glob("*.so*"):
            if file.is_file() and not file.is_symlink():
                set_runtime_paths(file, "$ORIGIN")
        remove_staged_absolute_elf_rpaths(staged)


def readelf_machines(path):
    readelf = shutil.which("llvm-readelf") or shutil.which("readelf")
    if not readelf:
        raise ValueError("cross-target proof requires llvm-readelf or readelf")
    result = subprocess.run([readelf, "-h", str(path)], capture_output=True, text=True, check=True)
    machines = sorted(set(re.findall(r"^\s*Machine:\s*(.+?)\s*$", result.stdout, re.M)))
    if not machines:
        raise ValueError(f"no ELF machine identity found: {path}")
    return machines


def first_artifact(root, patterns):
    for pattern in patterns:
        found = sorted(path for path in root.rglob(pattern) if path.is_file() and not path.is_symlink())
        if found:
            return found[0]
    raise ValueError(f"target artifact missing under {root}: {patterns}")


def prove_linux_cross_split(staged, prefix, build, work, tools):
    """Record executable host generators and AArch64 target artifacts."""
    if platform.system() != "Linux" or platform.machine().lower() not in ("x86_64", "amd64"):
        raise ValueError("cross-target evidence must be generated on a Linux/x86_64 build host")
    host = {}
    for name, value in tools["host_executables"].items():
        path = Path(value).resolve() if value else None
        if path is None or not path.is_file():
            raise ValueError(f"build-host generator missing: {name}")
        if any(path.is_relative_to(root.resolve()) for root in (staged, prefix)):
            raise ValueError(f"build-host generator entered the target tree: {name}: {path}")
        machines = readelf_machines(path)
        if machines != ["Advanced Micro Devices X86-64"]:
            raise ValueError(f"build-host generator is not x86_64: {name}: {machines}")
        host[name] = {"path": str(path), "sha256": digest(path), "machines": machines}
    artifacts = {
        "icu": first_artifact(prefix / "lib", ("libicuuc.so.*", "libicuuc.so")),
        "libc++": first_artifact(staged / "lib", ("libc++.so.*",)),
        "JavaScriptCore": first_artifact(build / "lib", ("libJavaScriptCore.so.*", "libJavaScriptCore.so")),
        "WebCore": first_artifact(build, ("libWebCore.a",)),
        "WebKit": first_artifact(build / "lib", ("libWebKit.so.*", "libWebKit.so")),
    }
    target = {}
    for name, path in artifacts.items():
        machines = readelf_machines(path)
        if machines != ["AArch64"]:
            raise ValueError(f"target artifact is not AArch64: {name}: {machines}")
        target[name] = {"path": str(path), "sha256": digest(path), "machines": machines}
    evidence = {"passed": True, "build_host": {"os": "linux", "arch": "x86_64"},
                "target": {"os": "linux", "arch": "aarch64"},
                "host_generators": host, "target_artifacts": target,
                "target_execution": "not attempted on the build host"}
    write_json(work / "cross-build-evidence.json", evidence)
    return evidence


def validate_cross_candidate(staged, work, asset, env):
    normalize_candidate_paths(staged, "linux")
    run([sys.executable, asset / "tools/verify_dist.py", "--dist", staged, "--stage", "06-web"], env)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("sdk-root", "asset-root", "work-root", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--recipe-location", required=True)
    parser.add_argument("--source-sha256", required=True)
    parser.add_argument("--dependency-jobs", type=int, default=4)
    parser.add_argument("--reuse-work-root", action="store_true")
    parser.add_argument("--source-cache", type=Path, help="verified WebKit product cache (sources only, never object files)")
    parser.add_argument("--previous-source-asset", type=Path,
                        help="with reuse: retry notice/packaging-only fixes after proving compile inputs unchanged")
    args = parser.parse_args()
    if args.dependency_jobs < 1:
        parser.error("--dependency-jobs must be positive")
    sdk, asset, work, output = [getattr(args, name).resolve() for name in ("sdk_root", "asset_root", "work_root", "output")]
    validate_layout(sdk, work, output)
    manifest = json.loads((sdk / "manifest.json").read_text())
    target = manifest["target"]
    mode = build_kind(target)
    cross = mode == "cross-linux-aarch64"
    target_os = target["os"]
    windows = target_os == "windows"
    exe = ".exe" if windows else ""
    suffix = "dylib" if target_os == "macos" else "so"
    require_predecessor(sdk, manifest)
    if args.source_cache:
        validate_source_cache(args.source_cache.resolve(), asset)
    work.mkdir(parents=True, exist_ok=True)
    identity = {"source_sha256": args.source_sha256,
                "predecessor_sha256": inventory_fingerprint(tree_inventory(sdk, normalize_manifest=True))}
    marker = work / "inputs.json"
    if marker.exists():
        previous = json.loads(marker.read_text())
        if not args.reuse_work_root or previous.get("predecessor_sha256") != identity["predecessor_sha256"]:
            raise ValueError("work-root already used: choose a fresh directory or reuse identical inputs")
        if previous["source_sha256"] != identity["source_sha256"]:
            if not args.previous_source_asset:
                raise ValueError("changed source asset: use a fresh work-root or prove a packaging-only retry")
            validate_packaging_retry(args.previous_source_asset.resolve(), previous["source_sha256"], asset)
            identity["previous_source_sha256"] = previous["source_sha256"]
            identity["packaging_retry"] = True
            write_json(marker, identity)
    else:
        if any(work.iterdir()):
            raise ValueError("work-root must start empty")
        write_json(marker, identity)
    staged = work / "sdk"
    if staged.exists():
        checkpoint = work / ("sdk-checkpoint-" + str(time.time_ns()))
        staged.rename(checkpoint)
        print(f"Retained previous generated SDK checkpoint: {checkpoint}", flush=True)
    shutil.copytree(sdk, staged, symlinks=True)
    if cross:
        # Compiler wrappers are build interfaces, not target runtime objects.
        # The 06 source asset carries the cross-aware revision; libc/libc++ and
        # every other target artifact remain inherited byte-for-byte.
        for name in ("crt-cc", "crt-c++"):
            shutil.copy2(asset / "tools" / name, staged / "tools" / name)
    env = runtime_env(staged, manifest)
    env.update(CRT_CXX_RUNTIME_LINKAGE="shared", CRT_CXX_HOSTED="1")
    if target_os == "linux" and not cross:
        run([sys.executable, asset / "tools/test_crt_cxx_shared_loader.py", "--sdk-root", staged,
             "--wrapper", staged / "tools/crt-c++", "--output", work / "loader-preflight"], env)
    # SDK-mode recipe tests resolve @ROOT@ at the SDK, not the source asset.
    # These are temporary test fixtures, not headers or runtime dependencies.
    shutil.copytree(asset / "libcrtgfx/assets/fonts", staged / "libcrtgfx/assets/fonts", dirs_exist_ok=True)
    recipes = load_recipes(asset / "porting/recipes")
    selected = ["icu", "gperf", "libpng", "sqlite-amalgamation"] + [name for name, recipe in recipes.items() if recipe.get("group") == "web"]
    ordered = resolve_recipe_order(recipes, selected)
    sources, prefix = work / "sources", work / "dependencies"
    run([sys.executable, asset / "tools/fetch_ports.py", "--dest", sources,
         "--cache", work / "downloads", "--recipe-dir", asset / "porting/recipes",
         *[value for name in selected for value in ("--port", name)]])
    tools = build_host_tools(asset, sources, work, recipes, args.dependency_jobs)
    target_recipes = work / "recipes"
    shutil.copytree(asset / "porting/recipes", target_recipes, dirs_exist_ok=True)
    icu = json.loads((target_recipes / "icu.json").read_text())
    icu["build"]["configure_args"].append("--with-cross-build=" + tools["icu_cross_build"])
    if windows:
        # These two commands now execute native HOST ICU tools, not the CRT
        # target tools the ordinary same-architecture port recipe launches.
        # Use the established narrow native-tool boundary: spawn hint and
        # POSIX-drive argument conversion, without exposing host headers/libs.
        invoke = "/system/bin/mksh " + web.crt_path(staged / "tools/crt-native-tool")
        icu["build"]["target_overrides"]["windows"]["make_args"] = [
            "INVOKE=" + invoke, "PKGDATA_INVOKE=",
            "PKGDATA=" + invoke + " " + web.crt_path(Path(sys.executable)) + " " +
            web.crt_path(asset / "tools/crt_icu_host_pkgdata.py") +
            " --sdk " + web.crt_path(staged) + " --host-program " + web.crt_path(work / "host/icu/bin/pkgdata.exe") +
            " --shell-dir " + shlex.quote(web.crt_path(Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Git/bin")) +
            " --compiler-dir " + web.crt_path(Path(tools["cc"]).parent) +
            " -- $(PKGDATA_OPTS) -q -c -s $(CURDIR)/out/build/$(ICUDATA_PLATFORM_NAME) -d $(ICUPKGDATA_OUTDIR)"]
    write_json(target_recipes / "icu.json", icu)
    if cross:
        for recipe in ordered:
            path = target_recipes / (recipe["name"] + ".json")
            write_json(path, linux_cross_recipe(json.loads(path.read_text())))
    if windows:
        openssl = json.loads((target_recipes / "openssl.json").read_text())
        # Its Unix CRT target requires a POSIX-path BUILD-host Perl, not
        # Strawberry's Win32 File::Spec. Generators remain outside the SDK.
        openssl["build"]["configure_interpreter"] = tools["posix_perl"]
        openssl["build"]["target_overrides"]["windows"]["make_args"] = ["PERL=" + invoke + " " + tools["posix_perl"]]
        openssl["build"]["target_overrides"]["windows"]["env"]["PERL5LIB"] = tools["posix_perl_lib"]
        write_json(target_recipes / "openssl.json", openssl)
    fontconfig = json.loads((target_recipes / "fontconfig.json").read_text())
    write_json(target_recipes / "fontconfig.json",
               fontconfig_host_generator_recipe(fontconfig, staged, work, tools["gperf"], target_os))
    # The SDK-mode port builder resolves project-owned shims/tests from its
    # root. These are source inputs copied from the asset, not host headers.
    shutil.copytree(asset / "porting/shims", staged / "porting/shims", dirs_exist_ok=True)
    shutil.copytree(asset / "porting/tests", staged / "porting/tests", dirs_exist_ok=True)
    shutil.copytree(asset / "libcrtweb/platform/gles-stub", staged / "libcrtweb/platform/gles-stub", dirs_exist_ok=True)
    port_command = [sys.executable, asset / "tools/crt-port-build.py", "--sdk-root", staged,
         "--source-root", sources, "--work-root", work / "ports", "--install-prefix", prefix,
         "--recipe-dir", target_recipes, "--jobs", args.dependency_jobs]
    if not cross:
        port_command.append("--test")
    port_command += [value for name in selected if name != "gperf" for value in ("--port", name)]
    run(port_command, env)
    web_work = work / "webkit"
    if args.source_cache and not (web_work / "product").exists():
        shutil.copytree(args.source_cache.resolve(), web_work / "product", symlinks=True)
    # Keep OpenSSL curl private/static inside WebKit. The predecessor's media
    # library already owns a different libcurl SONAME (mbedTLS); overwriting it
    # or resolving both backends by loader order is not a packaging contract.
    private_flags = ("" if windows else "-Wl,-unexported_symbol,_curl*" if target_os == "macos" else
                     "-Wl,--allow-shlib-undefined -Wl,--exclude-libs,ALL")
    web_command = [sys.executable, asset / "tools/build_webkit_crt_port.py", "--sdk-root", staged,
         "--deps-prefix", prefix, "--work-root", web_work, "--gperf", tools["gperf"],
         "--webkit", "--reuse-tree", "--recompose", "--target", "crtweb_host", "--target", "crtweb_api_test",
         "--define", "CURL_LIBRARY_RELEASE=" + (prefix / "curl-openssl/lib/libcurl.a").as_posix(),
         "--define", "pkgcfg_lib_PC_CURL_curl=" + (prefix / "curl-openssl/lib/libcurl.a").as_posix(),
         "--define", "CMAKE_SHARED_LINKER_FLAGS=" + private_flags,
         "-j", args.dependency_jobs]
    if cross:
        web_command[web_command.index("-j"):web_command.index("-j")] = [
            "--define", "CMAKE_SYSTEM_NAME=Linux", "--define", "CMAKE_SYSTEM_PROCESSOR=aarch64"]
    run(web_command, env)
    build = web_work / "build"
    write_json(work / "target-discovery.json", audit_target_discovery(build / "CMakeCache.txt", staged, prefix,
                                                                    web_work / "src/webkit-crt"))
    (web_work / "fonts.conf").write_text(web.font_configuration(
        target_path(asset / "libcrtgfx/assets/fonts", target_os), target_path(work / "font-cache", target_os)))
    run_env = web.gate_environment(build, staged, prefix, web_work, env, target_os)
    if not cross:
        run([build / ("bin/crtweb_api_test" + exe)], run_env, work)
    native = work / "consumer-build"
    # The SDK-Skia application/GPU worker uses the normal static CRT runtime,
    # just like the accepted in-tree consumers. Only the separate CPU browser
    # processes require WebKit's shared runtime. Do not leak that build setting
    # into a native window/GPU PAL consumer with transitive host-driver libs.
    consumer_env = dict(env, CRT_CXX_RUNTIME_LINKAGE="static")
    consumer_configure = ["cmake", "--fresh", "-S", asset / "distribution/stages/06-web", "-B", native, "-G", "Ninja",
         "-DCMAKE_TOOLCHAIN_FILE=" + (staged / "crt-toolchain.cmake").as_posix(),
         "-DCMAKE_INSTALL_PREFIX=" + staged.as_posix(), "-DCRT_STAGE_SOURCE_ROOT=" + asset.as_posix(),
         "-DCRT_STAGE_TARGET_OS=" + target_os, "-DCRT_STAGE_TARGET_ARCH=" + target["arch"]]
    if cross:
        consumer_configure += ["-DCMAKE_SYSTEM_NAME=Linux", "-DCMAKE_SYSTEM_PROCESSOR=aarch64"]
    run(consumer_configure, consumer_env)
    # Two small consumers: also invalidate environment-only wrapper settings,
    # which Ninja cannot see in an otherwise unchanged command line.
    run(["cmake", "--build", native, "--clean-first", "-j", args.dependency_jobs], consumer_env)
    run(["cmake", "--install", native], consumer_env)
    private = staged / "libexec/crtweb"
    private.mkdir(parents=True, exist_ok=True)
    private_lib = private / "lib"
    private_lib.mkdir(exist_ok=True)
    for name in ("crtweb_host", "WebProcess", "NetworkProcess"):
        shutil.copy2(build / "bin" / (name + exe), private / (name + exe))
    if windows:
        shutil.copy2(build / "bin/libcrtweb.dll", staged / "bin/libcrtweb.dll")
        shutil.copy2(build / "lib/libcrtweb.dll.a", staged / "lib/libcrtweb.dll.a")
    else:
        shutil.copy2(build / f"lib/libcrtweb.{suffix}", staged / f"lib/libcrtweb.{suffix}")
    for directory in ((prefix / "bin", prefix / "lib", build / "bin", build / "lib") if windows else (prefix / "lib", build / "lib")):
        copy_private_libraries(directory, private_lib, target_os)
    # The browser's software-only crtmedia/VA closure is private. Never put
    # libva stubs in SDK/lib and disable hardware decode for ordinary apps.
    media = staged / ("bin/libcrtmedia.dll" if windows else f"lib/libcrtmedia.{suffix}")
    shutil.copy2(media, private_lib / media.name)
    if target_os == "linux":
        normalize_cxx_aliases(staged)
    if windows:
        for file in (staged / "bin").glob("*.dll"):
            # A self-contained private PE search directory. Equal inherited
            # names must really be the same image, not overwrite-by-order.
            destination = private_lib / file.name
            if destination.exists() and digest(destination) != digest(file):
                raise ValueError(f"conflicting private/inherited DLL: {file.name}")
            shutil.copy2(file, destination)
    for file in ([] if windows else (staged / "lib").glob("*.dylib*" if target_os == "macos" else "*.so*")):
        if not file.is_file() or file.name in (f"libcrtweb.{suffix}", f"libcrtmedia.{suffix}"):
            continue
        destination = private_lib / file.name
        if not destination.exists() and not destination.is_symlink():
            destination.symlink_to(os.path.relpath(file, private_lib))
    shutil.copytree(asset / "examples/web-browser", staged / "examples/web-browser", dirs_exist_ok=True)
    # Metadata/porting helpers describe the output stage, not the old input
    # stage. Keep the predecessor's actual compiler/runtime wrappers intact.
    install_stage_tools(asset, staged)
    shutil.copytree(asset / "libcrtgfx/assets/fonts", staged / "share/fonts/crtweb", dirs_exist_ok=True)
    write_json(staged / "share/crt/dependencies/crtweb-fonts/recipe.json", {
        "name": "crtweb-fonts", "version": "bundled",
        "source": {"repository": "https://github.com/webos21/crt/tree/main/libcrtgfx/assets/fonts"},
        "files": {file.name: digest(file) for file in (asset / "libcrtgfx/assets/fonts").iterdir() if file.is_file()}})
    manifest.setdefault("redistributed_dependencies", []).append({
        "name": "crtweb-fonts", "kind": "font-data", "headers": [], "link_artifacts": [],
        "runtime_artifacts": [], "notices": ["share/fonts/crtweb/DejaVu-LICENSE.txt", "share/fonts/crtweb/PretendardGOV-LICENSE.txt"],
        "provenance": "share/crt/dependencies/crtweb-fonts/recipe.json"})
    font_config = web.font_configuration("../fonts/crtweb", "~/.cache/crtweb-fontconfig")
    font_config = font_config.replace("<dir>", '<dir prefix="relative">')
    (staged / "share/crt/web-fonts.conf").write_text(font_config)
    # Build generators never enter the SDK; this temporary stub source is not
    # a public API and is retained only under the corresponding-source record.
    shutil.rmtree(staged / "libcrtweb")
    shutil.rmtree(staged / "libcrtgfx")
    report = security.snapshot(json.loads((asset / "libcrtweb/third_party/webkit/recipe.json").read_text())["version"])
    if not report["passed"]:
        write_json(work / "security-failed.json", report)
        raise ValueError("WebKit pin is affected by an advisory; no SDK is published")
    # The shared cache helper uses a truncated fingerprint internally. Source
    # correspondence carries a full SHA-256 over a documented canonical tree.
    predecessor_inventory = json.dumps(tree_inventory(sdk, normalize_manifest=True),
                                       sort_keys=True, separators=(",", ":")).encode()
    predecessor = {"stage": "05-ui", "version": (sdk / "VERSION").read_text().strip(),
                   "content_sha256": hashlib.sha256(predecessor_inventory).hexdigest(),
                   "content_algorithm": "SHA256 of sorted-key compact JSON tree_inventory; manifest created_utc excluded"}
    package_provenance(asset, staged, web_work / "src/webkit-crt", sources, ordered, manifest, report, args, predecessor)
    manifest.update(stage="06-web", created_utc=datetime.now(timezone.utc).isoformat(),
                    built_from={"stage": "05-ui", "source_recipe": args.recipe_location,
                    "source_sha256": args.source_sha256, "predecessor_content_sha256": predecessor["content_sha256"],
                    "predecessor_version": predecessor["version"]},
                    external_prerequisites=external_prerequisites_for(target_os, "06-web", target["arch"]))
    manifest["web_runtime"]["build_host_tools"] = "share/crt/web-host-tools.json"
    if cross:
        evidence = prove_linux_cross_split(staged, prefix, build, work, tools)
        write_json(staged / "share/crt/web-cross-build.json", evidence)
        manifest["web_runtime"]["cross_build_evidence"] = "share/crt/web-cross-build.json"
    write_json(staged / "share/crt/web-host-tools.json", json.loads((work / "host-tools.json").read_text()))
    write_json(staged / "manifest.json", manifest)
    if cross:
        validate_cross_candidate(staged, work, asset, env)
    else:
        validate_candidate(staged, work, asset, env)
    output.parent.mkdir(parents=True, exist_ok=True)
    publish_tree(staged, output)
    print(f"CRT development stage ready: {output}; public release NOT authorized")


if __name__ == "__main__":
    main()
