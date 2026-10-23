#!/usr/bin/env python3
"""In-tree Web target adapter; the isolated builder remains the producer."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import uuid

ROOT = Path(__file__).resolve().parent.parent


def run(command, **kwargs):
    print("+", " ".join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), check=True,
                   stdout=sys.stdout, stderr=sys.stderr,
                   creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0, **kwargs)


def identity(sdk, stage, target_os, arch):
    manifest = json.loads((sdk / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("stage") != stage or manifest.get("target") != {"os": target_os, "arch": arch}:
        raise ValueError(f"expected {stage} {target_os}/{arch} SDK, got {manifest.get('stage')} {manifest.get('target')}")
    return manifest


def preflight_host_inputs(target_os):
    """Fail missing generator prerequisites before fetching/building target ports.

    Reuse the established WebKit generator checks. The POSIX Perl is a separate
    Windows host tool from native Perl used by WebKit's generators.
    """
    import build_stage_06_web as web
    web.web.jsc.require_host_tools()
    if target_os != "windows":
        return
    git = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Git"
    perl = git / "usr/bin/perl.exe"
    if not (git / "bin/bash.exe").is_file() or not perl.is_file() or not all(
            shutil.which(name) for name in ("gcc", "g++")) or not (shutil.which("gmake") or shutil.which("mingw32-make")):
        raise ValueError("Windows Web build needs Git Bash/POSIX Perl and native MinGW gcc/g++/make host tools")
    env = web.host_environment()
    modules = os.environ.get("CRT_WEB_HOST_PERL_LIB", "")
    if modules:
        path = Path(modules).resolve()
        if not path.is_dir():
            raise ValueError("CRT_WEB_HOST_PERL_LIB is not a prepared build-host module directory")
        env["PERL5LIB"] = web.web.crt_path(path)
    else:
        env.pop("PERL5LIB", None)
    probe = subprocess.run([str(perl), "-MIPC::Cmd", "-MExtUtils::MakeMaker", "-e", "print 1"],
                           env=env, capture_output=True, text=True,
                           creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    if probe.returncode or probe.stdout != "1":
        raise ValueError("OpenSSL POSIX host Perl modules unavailable; configure CRT_WEB_HOST_PERL_LIB: " + probe.stderr[-1500:])


def publish(candidate, output):
    """Retain the previous SDK and roll back its name if publication fails."""
    output.parent.mkdir(parents=True, exist_ok=True)
    backup = output.with_name(output.name + ".previous-" + uuid.uuid4().hex)
    existed = output.exists()
    if existed:
        output.rename(backup)
    try:
        from build_stage_05_ui import publish_tree
        publish_tree(candidate, output)
    except BaseException:
        if existed and not output.exists():
            backup.rename(output)
        raise
    if existed:
        print(f"Previous SDK retained: {backup}", flush=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phase", required=True, choices=("build", "test", "dist"))
    parser.add_argument("--sdk", required=True, type=Path)
    parser.add_argument("--predecessor", type=Path)
    parser.add_argument("--recipe", type=Path)
    parser.add_argument("--asset", type=Path)
    parser.add_argument("--source-cache", type=Path)
    parser.add_argument("--retry-run", type=Path, help="explicit owned failed run; existing isolated input guards still apply")
    parser.add_argument("--previous-source-asset", type=Path, help="with retry-run: prove a packaging-only source change")
    parser.add_argument("--work-root", required=True, type=Path)
    parser.add_argument("--archive-dir", type=Path)
    parser.add_argument("--target-os", required=True)
    parser.add_argument("--target-arch", required=True)
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args(argv)
    if args.previous_source_asset and not args.retry_run:
        parser.error("--previous-source-asset requires --retry-run")
    if args.phase != "build" and (args.retry_run or args.previous_source_asset):
        parser.error("retry options apply only to build")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    # This adapter is native-only. The separate isolated driver's explicit
    # Linux cross-build path is not silently treated as runtime acceptance.
    from build_stage_06_web import build_kind
    target = {"os": args.target_os, "arch": args.target_arch}
    if build_kind(target) != "native":
        raise ValueError("in-tree Web targets require a native supported pair; use the explicit cross-stage procedure")
    sdk, work = args.sdk.resolve(), args.work_root.resolve()
    if sdk == work or sdk in work.parents or work in sdk.parents:
        raise ValueError("Web SDK and work-root must be disjoint")
    work.mkdir(parents=True, exist_ok=True)
    if args.phase == "test":
        # Even identity/ordinary verification failures invalidate old evidence.
        (work / "test-result.json").unlink(missing_ok=True)
    if args.phase == "build":
        if args.predecessor is None or args.recipe is None:
            parser.error("build requires --predecessor and --recipe")
        predecessor = args.predecessor.resolve()
        identity(predecessor, "05-ui", args.target_os, args.target_arch)
        from build_stage_06_web import require_predecessor
        for left, right in ((predecessor, work), (predecessor, sdk)):
            if left == right or left in right.parents or right in left.parents:
                raise ValueError("predecessor, output and work-root must be disjoint")
        require_predecessor(predecessor, json.loads((predecessor / "manifest.json").read_text()))
        preflight_host_inputs(args.target_os)
        # Fresh by default. Explicit recovery is delegated to the isolated
        # driver's existing predecessor/source/packaging-only identity guards.
        if args.retry_run:
            run_root = args.retry_run.resolve()
            marker = run_root / "driver/build/06-web/inputs.json"
            if (args.retry_run.is_symlink() or run_root.parent != work or
                    not run_root.name.startswith("run-") or not marker.is_file() or
                    not marker.resolve().is_relative_to(run_root)):
                raise ValueError("retry-run must be an owned direct run-* child with isolated input evidence")
        else:
            run_root = Path(tempfile.mkdtemp(prefix="run-", dir=work))
        candidate = run_root / "candidate" / "06-web"
        command = [sys.executable, ROOT / "tools/crt-stage-build.py", "--sdk-root", predecessor,
                   "--recipe", args.recipe.resolve(), "--work-root", run_root / "driver",
                   "--output", candidate, "--dependency-jobs", args.jobs]
        if args.retry_run:
            command += ["--reuse-work-root"]
        if args.previous_source_asset:
            command += ["--previous-source-asset", args.previous_source_asset.resolve()]
        asset = args.asset
        if asset is None:
            recipe = json.loads(args.recipe.read_text(encoding="utf-8"))
            local = args.recipe.parent / recipe["source"]["archive"]
            if local.is_file():
                asset = local
        if asset:
            command += ["--asset", asset.resolve()]
        if args.source_cache:
            command += ["--source-cache", args.source_cache.resolve()]
        run(command)
        identity(candidate, "06-web", args.target_os, args.target_arch)
        run([sys.executable, ROOT / "tools/verify_dist.py", "--dist", candidate, "--stage", "06-web"])
        publish(candidate, sdk)
    else:
        manifest = identity(sdk, "06-web", args.target_os, args.target_arch)
        run([sys.executable, ROOT / "tools/verify_dist.py", "--dist", sdk, "--stage", "06-web"])
        if args.phase == "test":
            receipt = work / "test-result.json"
            # validate_candidate normalizes loader paths: do that only in a
            # disposable copy, never mutate the producer or replay SDK.
            from build_stage_05_ui import runtime_env
            from build_stage_06_web import validate_candidate
            test_root = Path(tempfile.mkdtemp(prefix="test-", dir=work))
            candidate = test_root / "sdk"
            shutil.copytree(sdk, candidate, symlinks=True)
            validate_candidate(candidate, test_root, ROOT, runtime_env(candidate, manifest))
            receipt.write_text(json.dumps({"passed": True, "sdk": str(sdk), "evidence": str(test_root)}, indent=2)
                               + "\n", encoding="utf-8")
        else:
            if args.archive_dir is None:
                parser.error("dist requires --archive-dir")
            from prepare_release_assets import archive_directory
            args.archive_dir.mkdir(parents=True, exist_ok=True)
            # Archive helper requires the canonical top-level directory name.
            # Keep the imported SDK immutable even if its directory is sdk06.
            with tempfile.TemporaryDirectory(prefix="archive-", dir=work) as directory:
                candidate = Path(directory) / "06-web"
                shutil.copytree(sdk, candidate, symlinks=True)
                archive = archive_directory(candidate, "06-web", (sdk / "VERSION").read_text().strip(),
                                            args.target_os, args.target_arch, args.archive_dir.resolve())
            print(f"Web archive ready (not a release approval): {archive}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f"crt-web-stage: {error}", file=sys.stderr)
        sys.exit(1)
