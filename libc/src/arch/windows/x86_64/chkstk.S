.text

# __chkstk is the MSVC-triple (*-pc-windows-msvc) name clang emits calls to;
# ___chkstk_ms is the separate, differently-mangled name clang emits instead
# when targeting *-w64-mingw32 (the triple tools/crt-cc uses for third-party
# ports specifically so their configure/libtool probes see a GNU-like
# toolchain -- see tools/crt-cc's own comment). Both names exist purely for
# historical MSVC-vs-MinGW C-symbol-naming-convention reasons; LLVM's own
# compiler-rt (lib/builtins/x86_64/chkstk.S) implements them as the exact
# same probe-only routine (caller still does its own `sub rsp, rax`
# afterward; this routine only touches each intervening page so the OS's
# stack guard page grows safely instead of being skipped over by a single
# large stack-frame allocation). Discovered missing when GNU make's port
# build (tools/crt-port-build.py, recipe "make") -- the first third-party
# port with a stack frame large enough to trigger it -- failed to link with
# "undefined symbol: ___chkstk_ms". Not needed on aarch64: AArch64 COFF
# never had the leading-underscore C-name-mangling split that x86/x86_64
# did, so mingw-w64 uses the same __chkstk name there as MSVC (see the
# aarch64 chkstk.S, which provides no separate _ms-suffixed alias).
.globl __chkstk
.globl ___chkstk_ms
.def __chkstk; .scl 2; .type 32; .endef
.def ___chkstk_ms; .scl 2; .type 32; .endef
.p2align 4, 0x90
__chkstk:
___chkstk_ms:
  pushq %rcx
  pushq %rax
  leaq 24(%rsp), %rcx
  cmpq $4096, %rax
  jb 2f
1:
  subq $4096, %rcx
  testq %rax, (%rcx)
  subq $4096, %rax
  cmpq $4096, %rax
  ja 1b
2:
  subq %rax, %rcx
  testq %rax, (%rcx)
  popq %rax
  popq %rcx
  retq
