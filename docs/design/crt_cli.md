# CRT-CLI: Development Environment and Source-first Ports

Design baseline: 2026-10-20. `webos21/crt-cli`, the `crt` commands below,
project/lock schemas, and the Host Tools Bundle are **implementation targets**,
not a record of a new CLI implemented in this repository.
The [SDK Development Contract](sdk_development_contract.md) owns producer
boundaries; [CRT Studio](crt_studio.md) owns the future GUI design.

## 1. Adopted Direction and Superseded Proposals

CRT-CLI is an independent development tool built against CRT libc/PAL and its
C++ runtime. The intended application workflow is shown below; command syntax
is a v1 draft.

```sh
crt sdk install --profile cxx
crt doctor
crt new hello --template cxx
cd hello
crt add zlib
crt build
crt run
```

The Rust reference concerns the **toolchain-selection, project-declaration,
and lock-file experience**. It does not mean implementing the CLI in Rust or
converting C/C++ libraries into Rust crates. Additional ports use
Homebrew-style upstream fetch/recipe/local builds, with stricter SDK, ABI,
and variant identities for C/C++.

The final direction supersedes these earlier proposals:

- Complete the IDE/Desktop UI first -> deliver the CLI, installation, and port engine first.
- Include the CLI in Studio -> separate `crt-cli`, with Studio as a later consumer.
- Require a host-native Go/Rust CLI -> CRT-native CLI plus bootstrap runtime closure.
- Mix independently versioned SDK stages like crates -> one validated SDK combination, separate port dependencies.
- Start with a binary-port registry -> source-first, local cache, then a trusted binary cache.

The initial scope excludes a general-purpose C/C++ package manager, a public
registry server, automatic execution of arbitrary recipes, a complete LLVM
port, and an IDE plugin marketplace.

## 2. Product and Internal Boundaries

```text
CLI / CI / AI / external IDE / future Studio
                 |
        crt commands + versioned JSON contract
                 |
  SDK resolver / environment resolver / project runner
                 |
  port resolver -> fetch/verify -> recipe executor -> immutable store
                 |
       installed SDK wrapper/toolchain/runtime contract
```

Separate the SDK/port installation control plane from port-build and app-run
environments. The CLI delegates technical policy to the SDK and supplies common
process execution, cancellation, and result collection. Do not reimplement
compile/link flags, startup order, or the Windows exception ABI in individual
commands. Logs must expose actual tools, argv, and environment selections.

Product sources target CRT-native C/C++. If an initial execution adapter calls
the SDK's existing Python port driver, declare Python as a requirement.
Do not hide it or immediately introduce a second, incompatible recipe engine.

## 3. CRT_HOME and Immutable Installations

Proposed default user data locations follow. System-wide/administrator
installation is not the initial default.

| Host | Default data root |
| --- | --- |
| Windows | `%LOCALAPPDATA%/CRT` |
| Linux | `$XDG_DATA_HOME/crt`, otherwise `~/.local/share/crt` |
| macOS | `~/Library/Application Support/CRT` |

Allow a `CRT_HOME` override. Linux configuration/cache data may use separate
XDG locations while retaining the same identity rules. A space-free home path
must not be required.

```text
CRT_HOME/
  bin/                         optional stable launcher only
  cli/<cli-version>-<digest>/   crt executable + app-local runtime closure
  host/<host-tuple>/<tools-id>/ manifest + existing rootfs-relative layout
  toolchains/<toolchain-id>/    external LLVM/OS SDK paths, versions, fingerprints
  sdk/<release>/<target>/<sdk-content-id>/
                               manifest.json, include/, lib/, bin/, system/,
                               libexec/, tools/, share/, crt-toolchain.cmake
  ports/store/<build-key>/      include/, lib/, bin/, manifest, notices, evidence
  registry/<revision>/         verified, pinned recipe/index snapshot
  sources/<source-digest>/      verified original archive / pristine source
  builds/<job-id>/              per-job source copy, build/install staging, rootfs
  downloads/                   incomplete downloads and verified cache
  cache/                       regenerable data
  logs/                        bounded job logs and diagnostics
  state/                       active pointers, install transactions, references
```

This is a logical target layout, not an instruction to rearrange existing SDK
internals. In particular, do not assume `crt-toolchain.cmake` lives in a new
`cmake/` directory.

SDKs, host tools, CLI versions, and ports have independent installation
identities and lifetimes. Updates install a new directory and atomically change
a selection pointer. Never overwrite an in-use executable, runtime DLL, or SDK.
Removing an SDK must not remove the CLI itself. Initial bootstrap must also
work through OS archive extraction and direct CLI execution from a version
directory. A future stable launcher needs its own first-run/update validation.

Installation proceeds through TLS/origin verification, digest checks, safe
temporary extraction, inventory/ABI validation, and atomic publication on the
same filesystem. Reject traversal, absolute/drive paths, escaping symlink
targets, case collisions, Windows reserved paths, and extraction-size abuse.
Preserve safe, real internal SDK symlinks. Fail clearly when Windows Developer
Mode is missing; do not change it automatically.

Serialize concurrent installations with per-identity locks and recover
interrupted transactions. Failures preserve previous installations and project
locks. Cache cleanup/GC respects usage, lock references, and active jobs; it
must not arbitrarily delete installed SDKs or project sources.

## 4. Host, Target, and Execution Environments

Always track `build_host`, `target`, `host_tools_identity`, and `sdk_identity`
separately. Record both tuples even for native development and explicitly
reject unverified cross combinations. Do not execute a target generator on the
host or report runtime success based only on cross compilation.

| Profile | Execution | Boundary |
| --- | --- | --- |
| `control` | SDK/registry fetch, selection, checks | CLI-private runtime; no accidental selection of another SDK's DLLs through PATH |
| `port-build` | configure/make and approved generators | CRT mksh/toybox/awk/make, pinned SDK wrappers and port dependencies |
| `app-build` | Project CMake/Ninja | SDK toolchain and project dependency view; no automatic host header/library discovery |
| `app-run` | Applications/workers | Application runtime closure only; no build tools or unrestricted package-store PATH |

Do not modify system PATH, registry settings, or user shell configuration by
default. Adding `CRT_HOME/bin` is optional; do not put mksh/make/awk on the
global PATH. Construct child environments and exact executable paths per job.
Do not silently substitute Git Bash/MSYS/native tools for a target port's
CRT shell/make.

Existing `CRT_SYSROOT` identifies the **target SDK**; `CRT_ROOTFS` identifies
the **process filesystem namespace**. Initially, prepare an execution view
that preserves read-only bundle/SDK files and provides separate writable
scratch space. Resolve current port-driver coupling between `CRT_ROOTFS` and
the SDK root through adapters/contract tests before separating host tools.
Two environment variables alone do not prove cross-host separation works.

Distinguish the Windows shell's guest PATH, `/system/bin:/bin:/usr/bin`, from
native Clang/CMake paths. Delegate approved native-tool invocation to the
existing bridge and preserve paths containing spaces or Korean characters in
actual argv. Pin exact environment construction to an SDK contract version.

`crt doctor` must diagnose at least host/target identity, Developer Mode and
symlinks, LLVM/LLD/OS SDK, CMake/Ninja, required Python/Perl/pkg-config/generators,
tool runtime DLLs, recipe applet requirements, and actual CRT compile/link/run
probes. It must not automatically install OS SDKs/compilers or change security
settings.

Restricted PATH/include/library roots plus output audits are **not an OS
sandbox**. Recipes execute code. Initially use official reviewed revisions;
make external-registry opt-in and execution risks explicit. Do not claim that
`--offline` plus PATH restrictions prevent malicious scripts from accessing
the network.

## 5. Source-first Port Recipe Engine

### Reuse Existing Assets

Initial official recipe sources are `crt`'s `porting/recipes/*.json`,
`porting/tests/`, `porting/shims/`, and installed
`tools/crt-port-build.py`. Do not bulk-rewrite the current JSON
`schema_version: 1` into TOML. New registry metadata points to exact CRT
revisions, recipe/test/shim digests, and engine requirements. Consider a TOML
recipe DSL only after demonstrating a need, with version negotiation and
conversion tests.

Do not interpret tools such as make in existing `dependencies` as target
libraries. The registry adapter must distinguish `host tools`,
`host generators`, `target ports`, and `required SDK stage/capabilities`.
CRT, as recipe owner, reviews these technical meanings; the CLI must not
guess them from names.

Start with the existing zlib recipe, then `sqlite-amalgamation` and libpng.
Providing the user-facing name `sqlite3` requires an explicit registry alias.
OpenAL Soft and SDL3 are future porting candidates, not packages assumed to
be fully validated already.

### Execution and Installation

```text
resolve exact SDK/recipe/dependencies
  -> fetch pinned source -> verify -> pristine source copy
  -> SDK-owned configure/build -> tests + binary/ABI audit
  -> immutable port-store publish -> project dependency view + lock commit
```

- Do not rebuild SDK binaries. Diagnose missing upper stages/tools explicitly
  and request separate SDK installation/selection.
- Do not arbitrarily patch upstream sources. Investigate CRT/PAL deficiencies
  against the Bionic baseline first. Unavoidable exceptions require prior
  review, rationale, removal conditions, and exact patch hashes/provenance.
  Recipes must not become hidden workarounds.
- Resolve target headers/libraries only from the SDK and selected store
  closure. Allowed OS/framework/driver boundaries must match SDK declarations.
- Build-host generators may be built separately as native tools. Do not ship
  those executables or their host runtimes in target ports/application bundles.
- Attempt native static/shared variants and include link/run smokes.
  Record unsupported variants with reasons and host/target-specific status;
  omissions are not successful acceptance.
- Installation into private build staging is allowed, but do not publish a
  store item as accepted until tests/audits pass. Keep failed outputs/logs
  distinct from accepted cache entries.

A build key hashes the normalized inputs below.

```text
K = H(source digest, registry/recipe/test/shim revision,
      SDK content identity, host/target, external toolchain identity,
      host-tools/generator identities, language/runtime ABI policy,
      linkage/build type/features, relevant declared environment,
      transitive dependency build keys)
```

Even for one upstream version, different SDKs, PIC settings, C++ exception/TLS
policies, backends, static/shared linkage, or Debug/Release configurations
produce separate variants. Identical build keys alone do not guarantee
byte-for-byte reproducibility. Installation manifests record input identities,
file/alias inventories, runtime closures, license/SBOM data, tool versions,
test results, and unsupported scope. Validate inventory/compatibility on local
cache reuse. A trusted binary cache is a later feature: verify the same key and
provenance, and retain an explicit source-build option.

Do not overwrite an SDK's public dependency when a port with the same name is
requested. Resolve whether to use the SDK-provided instance or explicit
isolation, and report conflicts. Do not promote WebKit-private libraries to
public application ports. v1 does not allow conflicting package ABIs within
the same link/runtime closure.

## 6. Project Manifest, Lock, and Build/Run/Package

`crt.toml` expresses user intent; `crt.lock` records exact resolution.
The following conceptual future-schema example does not replace existing
port JSON syntax.

```toml
[package]
name = "hello"
version = "0.1.0"

[sdk]
profile = "cxx"
channel = "integration"
target = "native"

[dependencies]
zlib = "1.3.1"

[build]
system = "cmake"
```

Locks record schema, normalized host/target, SDK archive/content digests and
source commit, registry revision, every port's source/recipe/variant/build key,
host-tools identities, and external-toolchain identities. Resolve
`native`/channels/latest into concrete identities. Keep local executable
absolute paths in machine state, but verify content/version identities against
the lock. Distinguish per-OS resolutions.

`--locked` forbids re-resolution and manifest/lock changes. Downloads/builds
of exactly locked inputs remain possible; `--offline` separately forbids
network use. Missing offline archives/registry/store entries cause failure.
Only explicit update operations change the lock. SDK updates do not
automatically migrate existing project locks.

Project views expose only selected store entries without modifying SDKs.
Generated CMake import targets/pkg-config metadata express transitive links,
static-library ordering, and runtime files. Reuse existing upper imports and
examples without assuming every stable public target name already exists.
Do not add host `/usr/lib` as a fallback. Delegate CMake configuration to the
installed SDK's `crt-toolchain.cmake`.

`new`, `add`, `remove`, and `update` show their change scope and preserve
original manifests/locks on failure. Remove changes project declarations;
it does not immediately delete shared store entries. Do not overwrite user
sources or existing projects.

Run selects an explicit program/argv/cwd and runtime closure. Web projects
preserve private Host/Web/Network/GPU worker and resource layouts. Package
assembles the application, required runtimes, ports, notices, and source
obligations. Acceptance requires execution on a target machine without the
development SDK/build/store. Declare OS driver/framework exceptions in manifests.

## 7. CLI and Automation Contract

Representative v1 command groups follow. Explicitly reject commands that the
initial implementation does not support; do not provide successful placeholders.

| Command | Responsibility |
| --- | --- |
| `sdk install/list/use/update`, `doctor` | Immutable SDK/host-tool selection, installation, diagnostics |
| `new`, `configure`, `build`, `run`, `test`, `package` | Project lifecycle |
| `search`, `info`, `add`, `install`, `remove`, `update`, `rebuild`, `tree` | Source-first ports and project dependencies |
| `env`, `shell` | Display a computed profile or run a shell in that environment |
| `cache clean` | Reference-aware cache cleanup |

Use `new` as the primary project-creation command. The earlier `create` name
may be a compatibility alias, not a separate feature. `install <port>` prepares
the store; `add <port>` includes project declaration/locking and source build.

All commands provide help/version, non-interactive operation, and clear exit
status. With `--json`, stdout contains one versioned final JSON object;
human/child logs go to stderr or log artifacts. Route application stdout from
run through a separate stream/file so it cannot corrupt the JSON envelope.
Long-running JSONL job events use a separate opt-in contract. Structure
command/target/plan, status/code, artifact/log references, diagnostics,
suggested actions, and cancellation results. Never record passwords, tokens,
or sensitive environment values.

Stable error codes distinguish at least SDK/host mismatch, unsupported schemas,
missing host tools, symlink prerequisites, source-digest mismatch, dependency
conflicts, and configure/build/test/audit failures. Freeze numeric exit-code
mappings with the v1 schema. Distinguish actual application exit/signal status
from wrapper errors.

Execute argv arrays rather than interpolating user paths/names into shell
strings. Jobs have timeouts, cancellation, and explicit process ownership;
avoid global name-based kills or hidden infinite retries. Windows automation
uses hidden/non-interactive children by default; open explicit shells/terminals
only at user request. If unfinished Windows process-group/terminal PAL support
is required, address it through a narrow reproducer and a separate implementation
gate.

## 8. Cross-repository Operations

`crt` owns SDK/Host Tools production and official recipe contracts;
`crt-cli` owns the resolver/UX and execution engine. Do not hardcode registry
content into CLI sources; start from a verified CRT revision. Split out
`crt-ports` only when port scale warrants it, not as an immediate fourth
repository requirement.

Linked issues/PRs record both revisions, contract changes, immutable
artifact/SDK locks, and host/target results. Merge/promote the producer first,
then update consumer locks; do not assume atomic cross-repository merges.
Isolate experimental APIs through opt-in/versioning and prevent runtime
dependencies on CLI implementation.

Fast CI covers parsers/resolvers/fixtures/SDK contracts; integration consumes
selected existing artifacts; full replay verifies native hosts, paths, and
failure recovery at milestones. Do not duplicate a full WebKit build for
every documentation change. External PRs receive no publishing tokens;
only a separate trusted promotion process publishes artifacts. CI success
and actual GUI/native-device acceptance are distinct.

## 9. Joint Development Sequence

This sequence consolidates the initial IDE-first J0-J6, CLI-only C0-C5, and
binary-first B/P proposals. Windows host-tools H0-H6 priorities are reflected
in D0-D4. This table defines future work order and acceptance goals, not a
completion checklist. Each repository's TODO/acceptance owns actual status.

| Stage | `crt` producer work | `crt-cli` consumer work | Acceptance goal |
| --- | --- | --- | --- |
| D0 Contracts/inventory | Inspect SDK/host tools/app imports/current driver gaps; schema fixtures | Minimal CRT-native CLI consuming a pinned SDK; lock/JSON drafts | CLI compile/link/run; no private headers/build tree; distinct host/target identities |
| D1 Windows environment/bootstrap | Host tools/aliases/DLL/rootfs bundle and technical validation | Independent CLI closure, environment resolver, doctor/env/shell | CLI runs without an SDK; actual CRT mksh/awk/make without global PATH changes |
| D2 Safe SDK management | Immutable stage artifacts and contract metadata | Profile installation/selection/update, verification/transactions/rollback | Linux/x86_64, macOS/arm64, Windows/x64 installs; spaces/Korean paths; tamper/interruption/DLL-collision handling |
| D3 First source port | Existing zlib recipe/tests and packaged driver contract | Fetch/verify/build/test/store adapter | Windows CRT mksh/make static/shared zlib link/run; SDK/source roots unchanged |
| D4 Dependencies/cache/lock | Review SQLite/libpng and host-generator/target-dependency roles | Resolver, variants/build keys, local cache, project views | SQLite and libpng->zlib; rerun/recovery/ABI-mixing rejection; three-native-OS replay |
| D5 Application workflow | Improve SDK-only CMake/runtime/public imports | new/add/configure/build/run, layer-specific templates | External C/C++ and supported GFX/Media/UI/Web apps run; Web worker/ABI checks |
| D6 Deployment/automation stability | Runtime closure and notices/source contracts | test/package, locked/offline/cancel, versioned JSON | App runs without SDK/store/build roots; CI/AI controls workflow through commands; repeated child-process acceptance |
| D7 Studio integration | Consumer-driven generic UI/input/PAL gaps | CLI compatibility and debug-adapter contracts | A thin [Studio](crt_studio.md) frontend uses the same workflow |

D0 snapshots may start with manually installed existing SDKs. Unfinished D1/D2
work does not block runtime development or recipe-schema fixtures. Conversely,
D3 acceptance must not substitute Git Bash for an actual CRT host environment.

Use small increments within each stage: contract, implementation, bounded test,
native replay, then documentation. Record host failures, unsupported
static/shared variants, and cross-compile-only results separately. Do not
assume every upstream port already works on all three OSes.

## 10. Decisions for the First Implementation

- Development-contract sidecar location/schema, public CMake targets, and
  runtime-closure query format.
- Bootstrap CLI HTTP/TLS stack, trust store, and exact minimal runtime files.
- Existing driver/rootfs adapter boundary separating Host Tools from target SDKs.
- Registry snapshot authentication/promotion, recipe-execution trust policy,
  project/lock schemas, and exit codes.
- Per-OS atomic publication, launcher/CLI self-update, usage references, and GC.

These are D0-D2 review deliverables, not an open-ended feature backlog.
Writing this design does not authorize repository creation, external
publication, implementation start, or public release.
