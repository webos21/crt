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
3. Platform Adaptation Layer
   - `platform/linux`, `platform/windows`, `platform/macos`,
     `platform/android`.
4. Architecture layer
   - `arch/x86_64`, `arch/aarch64`, atomics, TLS, startup code,
     calling-convention-sensitive code.
5. Optional compatibility modules
   - Android log/properties, Binder client primitives, ashmem/memfd-style shared
     memory, Linux/BSD extension shims.
6. Later graphics/application runtime
   - libc/PAL 안정화 이후 별도 정의한다.

## 프로젝트 구조

다음 폴더들이 프로젝트 루트에 구성될 항목들이다. 폴더 하위에는 각각 세부
폴더 목록이 존재할 수 있다.

- `docs/`
  - 프로젝트 관련 문서 저장.
- `include/`
  - public headers and exported ABI surface.
- `platform/`
  - OS별 PAL 구현.
- `arch/`
  - x86_64/aarch64 architecture-specific code.
- `cmake/`
  - CMake modules and toolchain files.
- `libc/`
  - 결과 파일: `libc.so`, `libc.a`
  - The C library. Stuff like fopen(3) and kill(2).
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
- `linker/`
  - 결과 파일: `/system/bin/linker`
  - The dynamic linker. It is responsible for loading the ELF executable into
    memory and resolving references to symbols.
- `tests/`
  - Unit, ABI, PAL, and integration tests.

## 참고 문서

상세한 프로젝트 의미와 기술 스택 판단은 다음 문서를 우선 참고한다.

- `docs/project_meanings.md`
- `docs/project_stacks.md`
