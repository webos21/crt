# Header And Sysroot ABI Policy

The project exposes a Bionic/Linux-shaped public header surface while compiling
on Linux, Windows, and macOS hosts. Public scalar types must not silently follow
the host libc data model when that would break the project ABI.

## Type Policy

The first public ABI tranche fixes the common scalar types through
`include/bits/crt_types.h`.

| Type | Project ABI |
| --- | --- |
| `ssize_t` | pointer-sized signed integer |
| `off_t` | signed 64-bit integer |
| `time_t` | signed 64-bit integer |
| `dev_t` | unsigned 64-bit integer |
| `ino_t` | unsigned 64-bit integer |
| `nlink_t` | unsigned 64-bit integer |
| `blksize_t` | signed 64-bit integer |
| `blkcnt_t` | signed 64-bit integer |
| `pid_t` | signed 32-bit integer |
| `socklen_t` | unsigned 32-bit integer |
| `sa_family_t` | unsigned 16-bit integer |
| `in_port_t` | unsigned 16-bit integer |
| `in_addr_t` | unsigned 32-bit integer |
| `nfds_t` | unsigned long |

Windows LLP64 differences are absorbed at this layer. In particular, `long` is
allowed to remain 32-bit on Windows, but `off_t`, `time_t`, inode/device types,
and pointer-sized signed types keep the project ABI.

## `bits/` Policy

The project now has a minimal `bits/` layer, starting with
`include/bits/crt_types.h`.

This is intentionally not a full Bionic `bits/` import yet. The rule is:

- use `bits/` for shared public ABI definitions that must be consistent across
  multiple public headers;
- keep feature macros, internal implementation details, and host adapter details
  out of public `bits/` headers;
- add narrower `bits/*.h` files only when a real public ABI boundary needs them.

Public code should include standard headers such as `<sys/types.h>`,
`<sys/socket.h>`, or `<time.h>`, not `<bits/crt_types.h>` directly.

## Header Compile Tests

`tests/header_abi_test.c` includes the current public header set in one
translation unit and checks the core type sizes and selected structure layouts.
This catches include-order conflicts, accidental host-header leakage, and
Windows LLP64 regressions early.

The data-model test remains responsible for target-wide C model expectations,
including pointer size, `long` size, `wchar_t`, and target-native `long double`
policy.
