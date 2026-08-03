# 에이전트 개발 작업 기준

## 프로젝트 목표

이 프로젝트의 목표는 Linux, Windows, macOS, Android에서 사용할 수 있는
**Bionic-compatible OS Abstraction Runtime / PAL**을 만드는 것이다.

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
- Qt, GTK, Enlightenment, Chromium/Chrome 같은 대형 프로젝트는 직접 포팅
  대상이 아니라 portability gap을 드러내는 예시 또는 장기 benchmark로 취급.

이 프로젝트는 Docker/LXC, WSL, 전체 Android framework 실행, APK 무수정 실행,
기존 Linux/glibc 바이너리 무수정 실행을 목표로 하지 않는다. 기본 목표는
**rebuild-based source portability**이다.

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
- Primary language: C99.
- Architecture-specific code: x86_64/aarch64 assembly 허용.
- Primary compiler: LLVM Clang.
- Primary linker: LLD.
- Compiler runtime: compiler-rt.
- C++ runtime 방향: libunwind, libc++ 연동을 고려하되 minimal C libc/PAL 이후
  단계적으로 진행.
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
     `libc/src/arch/windows/common/syscall.c`).
4. Optional compatibility modules
   - Android log/properties, Binder client primitives, ashmem/memfd-style shared
     memory, Linux/BSD extension shims.
5. Later graphics/application runtime
   - libc/PAL 안정화 이후 별도 정의한다.

## 프로젝트 구조

다음 폴더들이 프로젝트 루트에 구성될 항목들이다. 폴더 하위에는 각각 세부
폴더 목록이 존재할 수 있다.

- `docs/`
  - 프로젝트 관련 문서 저장.
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
    라이브러리별 `src/arch/`에 둔다. libm/libdl/libstdc++도 arch-specific 코드가
    필요해지면 동일한 패턴을 따른다.
- `libm/`
  - 결과 파일: `libm.so`, `libm.a`
  - The math library. Traditionally Unix systems kept stuff like sin(3) and
    cos(3) in a separate library.
- `libdl/`
  - 결과 파일: `libdl.so`
  - The dynamic linker interface library. This is where stuff like dlopen(3)
    lives.
- `libstdc++/`
  - 결과 파일: `libc++.so`
  - The C++ ABI support functions. Stuff like __cxa_guard_acquire and
    __cxa_pure_virtual live here.
- `shell/`
  - 결과 파일: `/system/bin/sh`, `/system/bin/toybox`
  - Android-like shell and command applet environment. This is a core runtime
    artifact used by porting tests, not an ordinary third-party port recipe.
  - `tiny_sh/`: project-owned bootstrap shell runner (`crt_tiny_sh`).
  - `mksh/`: imported Android `external/mksh`. Repo metadata (`Android.bp`,
    `NOTICE`, `mkshrc`, ...) lives directly under `mksh/`; the imported C
    source lives under `mksh/src/`.
  - `toybox/`: imported Android `external/toybox` under `toybox/src/`, with
    project-owned config/build glue under `toybox/crt/`.
- `linker/`
  - 결과 파일: `/system/bin/linker`
  - The dynamic linker. It is responsible for loading the ELF executable into
    memory and resolving references to symbols.
- `tests/`
  - Unit, ABI, PAL, and integration tests.
- `third_party/`
  - Import provenance: upstream manifests, license notes, and source-family
    review docs for imported Bionic/OpenBSD code (`third_party/bionic/`).
- `porting/`
  - `porting/recipes/`: third-party library porting recipes (zlib, libpng,
    libffi, SQLite amalgamation, the `make` bootstrap tool, ...).
- `tools/`
  - CRT/porting toolchain wrappers and scripts: `crt-cc`, `crt-c++`,
    `crt-env.sh`/`.cmd`/`.ps1`, `crt-port-build.py`, `create_rootfs.py`,
    `fetch_ports.py`.

## 참고 문서

상세한 프로젝트 의미와 기술 스택 판단은 다음 문서를 우선 참고한다.

- `docs/project_meanings.md`
- `docs/project_stacks.md`
