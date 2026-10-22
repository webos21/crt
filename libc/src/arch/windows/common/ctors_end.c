/* MinGW LLD sorts .ctors/.dtors before their .NNNNN priority subsections.
 * Clang encodes a priority as 65535-priority, so .99999 is past EVERY valid
 * priority, not just libc++'s .65435 (priority 100). An end marker in plain
 * .ctors/.dtors cut the priority entries out of our range entirely.
 * Keep the old symbol names and link placement for SDK compatibility; the
 * sorted end makes the complete range available to both EXE and DLL startup.
 * The MSVC $-sorted end markers remain independent and unchanged.
 */

__attribute__((section(".ctors.99999"), used)) void (*__crt_ctors_end)(void) = (void (*)(void))0;
__attribute__((section(".dtors.99999"), used)) void (*__crt_dtors_end)(void) = (void (*)(void))0;

__attribute__((section(".CRT$XCZ"), used)) void (*__crt_msvc_ctors_end)(void) = (void (*)(void))0;
__attribute__((section(".CRT$XTZ"), used)) void (*__crt_msvc_dtors_end)(void) = (void (*)(void))0;
