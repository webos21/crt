# 에이전트 개발 작업 기준

## 프로젝트 목표

이 프로젝트의 목표는 Linux, Windows, macOS에서 사용할 수 있는
**Bionic-compatible OS Abstraction Runtime / PAL**을 만드는 것이다.
Android는 대상 플랫폼이 아니다. Android가 쓰는 Bionic libc를 기반으로 도입했다는
점이 중요하며, Android는 그 libc의 출처이자 API/ABI 기준이다.

첫 산출물은 C Runtime Library처럼 보이지만, 실제 목표는 단순한 libc 포팅이
아니다. Android Bionic libc를 기반으로 libc/PAL 수준의 저수준 실행 환경을
통일하여, Linux/BSD/Android 계열 native library와 application source를 각
OS에서 더 쉽게 재빌드하고 이전할 수 있게 만드는 것이 핵심이다.

즉, 이 프로젝트는 다음을 지향한다.

- Bionic-compatible libc/API/ABI surface 제공.
- Linux, BSD, Android 스타일의 저수준 runtime facility를 공통화.
- files, sockets, threads, TLS, memory mapping, dynamic loading, errno,
  signals, clocks, process basics 등의 OS 차이를 PAL에서 흡수.
- 상위 계층이 graphics, window system, application lifecycle, packaging,
  host UX 같은 문제에 집중할 수 있도록 저수준 portability 기반 제공.
- 대형 프로젝트 중 WebKit은 `06-web` 단계의 직접 목표이다. WPE WebKit을 reference로
  삼은 **WebKit CRT Port**(`PlatformCRT`)를 만든다 (`docs/crtweb_acceptance.md`).
  Qt, GTK, Enlightenment, Chromium/Chrome 같은 나머지 대형 프로젝트는 직접 포팅
  대상이 아니라 portability gap을 드러내는 예시 또는 장기 benchmark로 취급.

이 프로젝트는 Docker/LXC, WSL, Android를 대상 OS로 삼는 것, 전체 Android
framework 실행, APK 무수정 실행, 기존 Linux/glibc 바이너리 무수정 실행을 목표로
하지 않는다. 기본 목표는 **rebuild-based source portability**이다.

## 포팅 테스트 방향성

외부 library/application porting test는 이 프로젝트의 핵심 개발 루프이다.
zlib, libpng, SQLite, libffi 같은 원본 upstream source를 CRT sysroot와 wrapper
toolchain으로 빌드해 보면서 부족한 Bionic-compatible libc/PAL 표면을 찾아
채운다.

이때 기본 원칙은 다음과 같다.

- porting test는 다음 순서로 반복한다.
  1. upstream `configure` 또는 `crt-cc` 직접 compile/link/run으로 구현 필요
     요소를 도출한다.
  2. 해당 API/type/symbol/behavior의 Android Bionic 구현과 public ABI 정책을
     확인한다.
  3. 우리 CRT/PAL/sysroot에서 어떤 방향으로 확장할지 결정하고 구현한다.
  4. 동일 porting test를 재실행하고, 실패하면 1번으로 돌아가 누락 표면을 다시
     도출한다.
- 원본 upstream source를 임의로 patch하지 않는다.
- 빌드 실패는 먼저 우리 CRT header, libc, libm, libdl, linker, C++ runtime,
  startup/sysroot, PAL 구현의 부족으로 간주하고 보강한다.
- porting test에서 새 header/type/macro/symbol/behavior가 필요해지면 반드시
  Android Bionic의 public header, source, ABI shape, errno/return-value 정책을
  먼저 참조한다. 우리 CRT/PAL/sysroot는 host OS의 native libc/SDK 모양이 아니라
  Bionic-compatible surface를 기준으로 확장한다.
- host OS 또는 특정 upstream package가 Darwin/glibc/MSVC 전용 surface를 요구하더라도
  그것을 그대로 public ABI로 채택하지 않는다. 필요한 경우 recipe compile option,
  내부 PAL adapter, 또는 명시적 compatibility shim을 사용하되 Bionic 기준과의 차이를
  문서화한다.
- configure/cache 변수는 CRT toolchain capability를 정확히 선언하기 위한 경우에만
  recipe에 기록한다.
- host SDK/header/library를 편의상 노출하여 통과시키지 않는다. 필요한 OS boundary는
  CRT/PAL이 통제하는 최소 compatibility surface로 제공한다.
- port-specific workaround가 정말 필요하면 먼저 문서에 이유와 장기 제거 조건을
  기록하고, upstream source 수정은 최후의 수단으로만 검토한다.
- porting 성공 여부는 단순 compile이 아니라 configure/make/install, 간단한 link/run
  smoke, 필요한 경우 runtime behavior test까지 확장해서 판단한다.

## 기반 스택

- Base runtime: Android Bionic libc.
- Primary language: C99 (core runtime: libc/libm/libdl와 PAL). 상위 계층
  (`libcrtgfx`의 Skia bridge, `libcrtui`의 Skia/MediaView companion, 해당 테스트,
  그리고 Skia/WebKit 같은 C++ upstream을 소비하는 코드)은 C++17이다.
- Architecture-specific code: x86_64/aarch64 assembly 허용.
- Primary compiler: LLVM Clang.
- Primary linker: LLD.
- Compiler runtime: compiler-rt.
- C++ runtime: imported LLVM libc++, libc++abi, libunwind (고정된 AOSP
  `toolchain/llvm-project` revision, `libstdc++/third_party/`의 recipe). `02-cxx`
  이상의 SDK가 이를 제공하고 GNU libstdc++는 채택하지 않는다. in-tree bootstrap
  C++ ABI 런타임(`libstdc++/src/`)은 `CRT_USE_IMPORTED_LIBCXX=OFF`일 때의
  기본이다 (`docs/cxx_runtime.md`).
- Build system: CMake.
- Default generator: Ninja.
- Test integration: CTest.
- Build configuration: `CMakePresets.json`와 target-specific CMake toolchain
  files를 사용.

Rust는 core runtime의 기본 언어로 사용하지 않는다. 단, build tool, code
generator, test/fuzz tool, 또는 명확한 `extern "C"` 경계 뒤의 선택적 내부
module에는 사용할 수 있다. core runtime은 Rust toolchain 없이도 빌드 가능해야
한다.

## 빌드 및 런타임 경계

이 프로젝트는 자체 libc/PAL을 제공하므로 core runtime build가 host libc,
host startup files, host default runtime libraries에 우발적으로 의존하지
않도록 해야 한다.

구현 시 다음 원칙을 따른다.

- Low-level runtime object에는 필요 시 `-ffreestanding` 사용.
- `memcpy`, `memmove`, `memset`, `strlen`, `malloc` 등 compiler builtin과
  충돌할 수 있는 symbol 구현 시 `-fno-builtin` 또는 targeted
  `-fno-builtin-<name>` 사용 검토.
- Final runtime/test link에서는 필요에 따라 `-nostdlib`, `-nostartfiles`,
  `-nodefaultlibs`를 사용하여 host runtime 의존을 차단.
- Hosted build tools와 freestanding runtime object를 명확히 분리.
- Target tuple별 explicit sysroot를 구성하고, headers/libraries/startup
  objects를 해당 sysroot에 설치.
- Bionic cleaned kernel headers와 Linux UAPI provenance/license metadata를
  보존.
- 내부 Linux kernel header를 임의로 복사하지 않는다.
- C++ 예외 처리도 이 경계 원칙의 대상이다. Windows(`*-w64-mingw32`) 타겟에서
  Clang 기본값은 OS 자체가 제공하는 native SEH 예외 테이블(`.pdata`/`.xdata`,
  `RtlUnwind`/`RtlVirtualUnwind` 기반)이지만, CRT는 `-fdwarf-exceptions`로
  강제 전환하여 Linux/macOS와 동일하게 Itanium DWARF CFI 예외 테이블을
  사용한다. CRT는 이미 자체 소스로 libunwind를 빌드해 3개 OS에서 동일한
  unwind 엔진/테이블 포맷을 쓰기로 결정했으므로 (host `libunwind-dev` 패키지
  사용을 명시적으로 거부한 것과 동일한 "toolchain을 직접 소유한다" 원칙),
  Windows에서만 native SEH(=OS 소유의 unwind 엔진)에 의존하면 이 일관성이
  깨진다. 자세한 배경은 `docs/cxx_runtime.md`의 "Exceptions, RTTI, And
  Unwind" 절과 `tools/crt-libcxx-build.py`의 관련 주석을 참고한다.

## 아키텍처 원칙

runtime은 다음 계층으로 나누어 설계한다.

1. Bionic-compatible public surface
   - headers, libc/libm/libdl/libstdc++ symbols, errno, pthreads, C/C++ ABI.
2. Runtime core
   - allocator, stdio, time, locale policy, path handling, process abstractions,
     dynamic loading policy, shared internal helpers.
3. Platform Adaptation Layer / Architecture layer
   - OS별·아키텍처별 저수준 구현: atomics, TLS, startup code, syscall wrapper,
     calling-convention-sensitive code.
   - 현재는 별도 루트 `platform/`, `arch/` 대신 라이브러리별
     `<lib>/src/arch/{linux,macos,windows}/{x86_64,aarch64,common}` 아래에 있다
     (예: `libc/src/arch/macos/aarch64/crt1.S`,
     `libc/src/arch/windows/common/syscall.c`). `libcrtgfx`, `libcrtmedia`의
     호스트별 백엔드도 같은 `src/arch/` 패턴을 따른다.
4. Optional compatibility modules
   - Linux 확장 API 일부(epoll, inotify, eventfd, timerfd 등)는 libc에 구현되어
     있다. Android log/properties, Binder client primitives, ashmem/memfd-style
     shared memory는 **구현되어 있지 않다**: 현재는 `include/android/log.h`
     header와 API-level shim(`libc/src/android_api.c`)뿐이며, 실제 consumer가
     요구할 때 정의한다.
5. Graphics/application runtime
   - libc/PAL 위의 누적(cumulative) SDK stage로 정의되어 있다 (`docs/runtime_roadmap.md`).
     `03-gfx-simple`(`libcrtgfx` window/input), `04-gfx-media`(Skia/GPU,
     `libcrtmedia`), `05-ui`(`libcrtui`), `06-web`(`libcrtweb`, WebKit CRT Port,
     진행 중). 각 단계는 이전 단계 SDK만으로 빌드·검증된다 (`docs/distribution.md`).

## 프로젝트 구조

다음은 프로젝트 루트의 현재 구성이다 (2026-10-03 기준). 폴더 하위에는 각각 세부
폴더 목록이 존재할 수 있다.

라이브러리 폴더(`libc/`, `libcrtgfx/` 등)는 공통으로 `include/`(public header),
`src/`(구현. OS/아키텍처별 코드는 `src/arch/{linux,macos,windows}/...`),
`tests/`(해당 라이브러리가 소유하는 테스트), `third_party/`(해당 라이브러리 전용
외부 의존성의 pin/provenance), `tools/`(데모)를 가질 수 있다. 큰 upstream source는
저장소에 vendor하지 않고, 고정(pin)된 버전을 빌드 시 fetch하여 크기/SHA-256을
검증한다.

### Core runtime (`01-c`, `02-cxx`)

- `include/`
  - public headers and exported ABI surface.
- `libc/`
  - 결과 파일: `libc.so`, `libc.a`
  - The C library. Stuff like fopen(3) and kill(2).
  - `include/`: public headers.
  - `src/`: implementation. `src/arch/{linux,macos,windows}/{x86_64,aarch64,common}`
    에 architecture/OS별 startup, syscall, setjmp 코드가 있고, `src/gdtoa/`와
    `src/string/`에는 각각 imported OpenBSD gdtoa 계열과 Bionic string/memory
    계열 소스가 있다. architecture-specific code는 공용 루트 `arch/`가 아니라
    라이브러리별 `src/arch/`에 둔다. `libdl/`은 이미 이 패턴을 따르고 있고
    (아래 참고), libm/libstdc++도 arch-specific 코드가 필요해지면 동일한
    패턴을 따른다.
  - `tests/`: libc/libdl/pthread/shell-level unit, ABI, PAL, and integration
    tests -- the former top-level `tests/` (dist-stage policy: each library
    owns its own tests, matching `libcrtgfx/tests/`/`libcrtmedia/tests/`'s
    already-established shape). `add_crt_test()`/`add_crt_cxx_test()`
    themselves stay defined at the top-level `CMakeLists.txt` (shared by
    this directory and `libstdc++/tests/`, which are separate
    `add_subdirectory()` trees and cannot see a function defined in a
    sibling tree).
- `libm/`
  - 결과 파일: `libm.so`, `libm.a`
  - The math library. Traditionally Unix systems kept stuff like sin(3) and
    cos(3) in a separate library.
  - `src/freebsd/`: imported FreeBSD msun 계열 소스. 프로젝트 소유 코드
    (`basic.c`, `fenv.c`, `long_double.c`)와 분리되어 있다. 의존성/출처는
    `docs/libm_dependency_map.md`.
- `libdl/`
  - 결과 파일: `libdl.so`
  - The dynamic linker interface library. This is where stuff like dlopen(3)
    lives.
  - `src/dl.c`: host-independent dispatcher (`dlerror()` state, shared handle
    validation), calling into a per-host backend under
    `src/arch/{linux,macos,windows}/dl_*.c` via `src/dl_internal.h`. Linux's
    backend is currently a documented stub (no CRT-owned ELF loader yet); see
    `docs/dynamic_loading.md`.
- `libstdc++/`
  - 결과 파일: `libc++.a`, `libc++.so` (in-tree bootstrap 런타임의 `OUTPUT_NAME`).
  - 디렉터리 이름은 Android Bionic의 작은 C++ ABI 지원 라이브러리 이름에서
    왔다. GNU libstdc++는 채택하지 않는다 (`docs/cxx_runtime.md`).
  - `src/`: in-tree bootstrap C++ ABI/allocation runtime (`cxxabi.c`,
    `msvcabi.c`, `new_delete.cc`). `CRT_USE_IMPORTED_LIBCXX=OFF`일 때의 기본
    런타임이며, `__cxa_guard_acquire`, `__cxa_pure_virtual` 같은 symbol이 있다.
  - `third_party/{libcxx,libcxxabi,libunwind}`: imported LLVM 런타임 recipe
    (고정된 AOSP `toolchain/llvm-project` revision). `tools/crt-libcxx-build.py`가
    fetch/build하며, `02-cxx` 이상의 SDK와 Skia 등 상위 계층이 이 imported
    런타임을 쓴다. `third_party/win32_shim`은 Windows 타깃에서 Skia/Media
    Foundation을 컴파일할 때 쓰는 호환 header이다.
  - `tests/`: C/C++-ABI-boundary and imported-libc++ tests (`cxx_runtime_
    test`, `cxx_frontend_test`, `cxx_allocation_test`; `imported_libcxx_
    test.cc` and friends, compiled/run directly by `tools/test_libcxx_
    runtime.py` rather than through this directory's own CMakeLists.txt).
- `shell/`
  - 결과 파일: `/system/bin/sh`, `/system/bin/toybox`, `/system/bin/awk`
  - Android-like shell and command applet environment. This is a core runtime
    artifact used by porting tests, not an ordinary third-party port recipe.
  - `tiny_sh/`: project-owned bootstrap shell runner (`crt_tiny_sh`).
  - `mksh/`: imported Android `external/mksh`. Repo metadata (`Android.bp`,
    `NOTICE`, `mkshrc`, ...) lives directly under `mksh/`; the imported C
    source lives under `mksh/src/`.
  - `toybox/`: imported Android `external/toybox` under `toybox/src/`, with
    project-owned config/build glue under `toybox/crt/`.
  - `awk/`: imported onetrue-awk (`/system/bin/awk`), with its own
    `import_manifest.json`.
- `linker/`
  - **예약된 폴더이며 아직 빌드 타깃이 없다** (`README.md`만 있음). 결과 파일은
    `/system/bin/linker`로 계획되어 있다.
  - 장기적으로 ELF executable을 메모리에 올리고 symbol을 해석하는 project-owned
    dynamic linker/loader가 들어갈 자리이다. 정책은 `docs/linker_loader.md`에
    있고, 구체적인 ELF loader milestone이 선택되기 전까지 공개 API는 `libdl/`이다.

### Upper runtime (`03-gfx-simple` ~ `06-web`)

각 단계는 이전 단계 SDK를 포함하는 누적(cumulative) stage이다.
단계 정의와 순서는 `docs/runtime_roadmap.md`, 배포 계약은 `docs/distribution.md`.

- `libcrtgfx/`
  - 결과 파일: `libcrtgfx` (window/input/software framebuffer, `03-gfx-simple`),
    `libcrtgfx_gpu` (GPU device/surface: Vulkan/D3D12/Metal), `libcrtgfx_skia`
    (Skia bridge) -- 각각 static/shared (`04-gfx-media`).
  - `src/arch/{linux,macos,windows}/`: 호스트별 window/GPU 백엔드.
  - `third_party/{skia,wayland,xkbcommon}`: pin/recipe. `assets/fonts`: 번들 폰트.
  - `cmake/`: in-tree 빌드와 isolated stage 프로젝트가 공유하는 source/target 목록.
  - `tools/`: window/GPU/Skia 데모. 정책은 `docs/libcrtgfx_api_policy.md`.
- `libcrtmedia/`
  - 결과 파일: `libcrtmedia.a`, `libcrtmedia.so` (`04-gfx-media`).
  - FFmpeg 기반 media: frame/format/extractor/codec/muxer/player, host audio sink,
    capture, hardware decode와 GPU frame 전달, HTTP/HTTPS transport (libcurl은
    CRT 소유 계약 아래에 있다). 정책은 `docs/libcrtmedia_api_policy.md`.
  - `third_party/ffmpeg`, `assets/`(테스트용 오디오/비디오 클립), `cmake/`, `tools/`.
- `libcrtui/`
  - 결과 파일: `libcrtui.a`, `libcrtui.so`, 선택적 `libcrtui_skia.a`,
    `libcrtui_skia_media.a` (`05-ui`).
  - CRT 소유 application UI API. LVGL은 비공개 구현 의존성이며 빌드 시 fetch되고
    (`third_party/lvgl`에는 pin만 있다) public header나 SDK에 노출되지 않는다.
  - `cmake/crtui_sources.cmake`: in-tree 빌드와 isolated stage가 공유하는 source
    목록. 상세는 `libcrtui/README.md`, `docs/crtui_acceptance.md`.
- `libcrtweb/`
  - `06-web`(WebKit CRT Port)의 자리. **현재는 `third_party/webkit/`의 WPE WebKit
    2.54.0 pin과 provenance(`recipe.json`, `license-inventory.json`)뿐이며**
    `crtweb` API, `PlatformCRT`, 빌드 타깃은 아직 없고 루트 CMake에 연결되어 있지
    않다. 계약은 `docs/crtweb_acceptance.md`, upstream 매핑은
    `docs/crtweb_porting.md`.

### Provenance and porting

- `third_party/`
  - Import provenance: upstream manifests, license notes, and source-family
    review docs for imported Bionic/OpenBSD code (`third_party/bionic/`).
  - 라이브러리 전용 외부 의존성의 pin/provenance는 이곳이 아니라 해당 라이브러리의
    `third_party/<name>/`에 둔다 (예: `libcrtgfx/third_party/skia`,
    `libcrtui/third_party/lvgl`, `libcrtweb/third_party/webkit`).
- `porting/`
  - `porting/recipes/`: third-party library porting recipes (zlib, libpng,
    libffi, SQLite amalgamation, bzip2, xz, PCRE2, expat, mbedTLS, curl, FreeType,
    FFmpeg, the `make` bootstrap tool, ...).
  - `porting/tests/`: recipe별 link/run smoke 소스. `porting/shims/{macos,win32}`:
    host별 compatibility shim. `DISTRIBUTION_README.md`: SDK에 함께 설치되는 porting
    안내.

### Build, packaging, and docs

- `tools/`
  - CRT/porting toolchain wrappers and scripts: `crt-cc`, `crt-c++`, `crt-ar`,
    `crt-env.sh`/`.cmd`/`.ps1`, `crt-port-build.py`, `create_rootfs.py`,
    `fetch_ports.py`.
  - 배포/stage: `create_dist.py`, `create_stage_source.py`, `crt-stage-build.py`,
    `build_stage_0N_*.py`(isolated stage 진입점), `verify_dist.py`,
    `prepare_release_assets.py`, 바이너리/텍스트 검사(`crt_elf.py`, `crt_macho.py`,
    `crt_pe.py`, `crt_text_relocate.py`).
  - upstream pin fetch: `fetch_skia.py`, `fetch_lvgl.py`, `fetch_wayland.py`,
    `fetch_xkbcommon.py`, `fetch_webkit.py`, `fetch_mingw_w64_headers.py`;
    `crt-libcxx-build.py`(imported libc++ 빌드), allocator baseline 도구.
  - `test_*.py`: 위 도구들의 단위/통합 테스트 (`tools/` 안에서 직접 실행).
- `distribution/`
  - isolated source-stage 프로젝트: `stages/{03-gfx-simple,04-gfx-media,05-ui}/CMakeLists.txt`는
    이전 단계 SDK만으로 다음 단계를 빌드하는 독립 CMake 프로젝트이고,
    `stage_recipe.schema.json`은 stage recipe 형식이다. 진입점은 `tools/build_stage_0N_*.py`.
- `examples/`
  - SDK에 설치되는 예제(`gfx-simple`, `gfx-gpu`, `gfx-skia`, `media-player`,
    `media-stream`, `ui-basic`). 각각 패키지된 `crt-toolchain.cmake`로 다시 빌드할 수
    있는 독립 CMake 프로젝트이다.
- `cmake/`
  - `run_core_tests.cmake`: 기본 workflow가 C stage CTest만 돌리도록 상위 계층 테스트
    (`crtgfx_`, `crtmedia_`, `crtui_`, `*cxx`)를 제외하는 필터.
- `benchmark/`
  - allocator baseline/contention 결과(`.jsonl`). 호스트별 결과를 한곳에 모으기 위해
    일부러 git에 커밋하며, 손으로 편집하지 않고 driver script로 재생성한다.
- `docs/`
  - 프로젝트 문서 저장: 정책/설계, 단계별 acceptance 기록(`crtui_`/`crtweb_`/
    `crtmedia_*`), roadmap, 배포 계약, porting 상태. `bringup/`, `study/`,
    `refine/`, `marketing/`은 참고/작업 메모이다.
- `.github/workflows/ci.yml`
  - CI matrix: macOS aarch64, Linux amd64/arm64, Windows x64/arm64.
- 루트 문서: `README.md`(공개 개요), `STATUS.md`(근거 기반 현재 상태 스냅샷),
  `TODO.md`(진행 중/예정 작업), `HISTORY.md`(완료 작업의 상세 기록), `LICENSE.md`.
  빌드 입구는 `CMakeLists.txt`와 `CMakePresets.json`이다.
- `out/`
  - 빌드 산출물과 SDK(`out/<preset>/dist/`). 생성물이므로 git에 포함하지 않는다.

## 참고 문서

상세한 프로젝트 의미와 기술 스택 판단은 다음 문서를 우선 참고한다.

- `docs/project_meanings.md`
- `docs/project_stacks.md` (core runtime 스택. 상위 계층은 아래 roadmap/acceptance 문서)

단계 정의, 계약, 배포는 다음 문서를 참고한다.

- `docs/runtime_roadmap.md`: 단계 순서와 경계.
- `docs/distribution.md`: 누적 SDK stage와 isolated stage 계약.
- `docs/crtui_acceptance.md`, `docs/crtweb_acceptance.md`,
  `docs/crtmedia_*_acceptance.md`: 단계별 계약과 증거.
- `docs/crtweb_porting.md`: WebKit upstream과 CRT의 매핑.

## 문서 관리

- `HISTORY.md`는 완료된 작업의 상세 기록, `TODO.md`는 진행 상황을 한두 줄로
  적는 곳이며, 끝난 항목의 상세는 `HISTORY.md`로 옮기고 `TODO.md`에서 지운다.
- `STATUS.md`는 명시적으로 요청받았을 때만 갱신한다. 상세 규칙은 `TODO.md`의
  Notice를 따른다.
- 같은 상태를 `README.md`, `STATUS.md`, `TODO.md`에 반복해 적지 않는다.
