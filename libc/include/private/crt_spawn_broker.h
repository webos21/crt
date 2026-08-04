#ifndef CRT_PRIVATE_CRT_SPAWN_BROKER_H
#define CRT_PRIVATE_CRT_SPAWN_BROKER_H

#include <stdint.h>

/*
 * Windows-only. A RtlCloneUserProcess clone never goes through
 * CreateProcess's CSRSS registration handshake, so CreateProcessA (and
 * anything else needing a working CSR connection) crashes if called
 * directly from inside one. __crt_sys_posix_spawn() uses
 * __crt_windows_is_unregistered_clone() to detect that case and routes
 * the spawn request through a broker process instead of calling
 * CreateProcessA itself, via __crt_windows_spawn_broker_request().
 *
 * The broker is a second instance of the same executable, launched once
 * (lazily, from a still-registered process, before any clone exists) by
 * __crt_windows_ensure_spawn_broker(). It is dispatched into
 * __crt_windows_spawn_broker_main() very early in mainCRTStartup(), before
 * the normal fd/rootfs bootstrap, and never returns.
 *
 * See docs/windows_fork_emulation.md, "Chosen Direction: Spawn Broker".
 */
int __crt_windows_is_unregistered_clone(void);

#define CRT_SPAWN_BROKER_MODE_ENV "CRT_SPAWN_BROKER_MODE"
#define CRT_SPAWN_BROKER_PIPE_ENV "CRT_SPAWN_BROKER_PIPE"

#define CRT_SPAWN_BROKER_MAGIC 0x43535042U /* "CSPB" */
#define CRT_SPAWN_BROKER_VERSION 1U

#define CRT_SPAWN_BROKER_PATH_MAX 4096U
#define CRT_SPAWN_BROKER_CMDLINE_MAX 8192U
#define CRT_SPAWN_BROKER_CWD_MAX 4096U
#define CRT_SPAWN_BROKER_ENV_MAX 65536U
#define CRT_SPAWN_BROKER_PIPE_NAME_MAX 128U

/* Fixed-size request header, written first; the four variable-length
 * fields (application path, command line, current directory, environment
 * block) follow back-to-back on the wire, each exactly as many bytes as
 * its *_length field says, with no extra framing. */
struct crt_spawn_broker_request_header {
  uint32_t magic;
  uint32_t version;
  uint32_t client_pid;
  uint32_t creation_flags;
  /* Raw HANDLE values, meaningful only in the *client's* process. The
   * broker opens the client process (it always can -- it is always
   * CSRSS-registered) and duplicates these into itself before use. 0
   * means "no explicit std handle for this slot". */
  uint64_t std_input;
  uint64_t std_output;
  uint64_t std_error;
  /* CreatePipe() itself has been observed to fail with
   * ERROR_INVALID_HANDLE when called from inside an unregistered clone
   * (not just CreateProcessA), so the client cannot create the
   * fd-snapshot bootstrap pipe locally and merely hand a handle to the
   * broker the way it does for std_input/output/error. Instead, when
   * this is nonzero, the broker creates the pipe itself (a normal
   * CreateProcessA-spawned process, so CreatePipe works fine there),
   * uses the read end directly -- inheritable, in its own process -- as
   * part of the same CreateProcessA call that spawns the real target
   * (patching its numeric value into the CRT_FD_SNAPSHOT_PIPE_ENV entry
   * already present in the environment block below), and returns the
   * write end duplicated into the client via
   * crt_spawn_broker_response.fd_snapshot_pipe_write. This is the one
   * place the broker protocol is not fully generic: it knows about this
   * one specific env var so the existing fd-snapshot bootstrap machinery
   * keeps working unmodified on the far side of the extra broker hop. */
  uint32_t want_fd_snapshot_pipe;
  /* Set to request a plain, unattached pipe instead of a spawn: the
   * broker creates it locally (the same "CreatePipe works fine in a
   * normal CreateProcessA-spawned process" reasoning as
   * want_fd_snapshot_pipe above) and duplicates *both* ends back into the
   * client, then responds immediately without touching any of the
   * spawn-only fields below or calling CreateProcessA at all. This is
   * what __crt_sys_pipe() uses when called from inside an unregistered
   * clone -- ordinary pipe() has exactly the same CreateProcessA-adjacent
   * failure mode as the fd-snapshot bootstrap pipe did, just reached via
   * mksh forking a subshell and then needing another pipe for a nested
   * command substitution or `|` pipeline, rather than via posix_spawn().
   * Mutually exclusive with want_fd_snapshot_pipe and every other field
   * except client_pid; a client requesting a plain pipe still fills in
   * magic/version/client_pid and leaves the rest zeroed. */
  uint32_t want_plain_pipe;
  uint32_t application_path_length;
  uint32_t command_line_length;
  uint32_t current_directory_length;
  uint32_t environment_length;
};

struct crt_spawn_broker_response {
  uint32_t magic;
  uint32_t version;
  int32_t result;         /* 0 on success, negative errno-shaped value on failure */
  uint32_t windows_error;  /* GetLastError() at the point of failure, diagnostics only */
  uint32_t process_id;
  /* Raw HANDLE values, already duplicated by the broker so they are
   * meaningful in the *client's* own process -- the client uses them
   * exactly as if it had called CreateProcessA itself. */
  uint64_t process_handle;
  uint64_t thread_handle;
  /* Valid (nonzero) only when the request had want_fd_snapshot_pipe set
   * and result is 0: the fd-snapshot bootstrap pipe's write end,
   * already duplicated into the client's own process. The client writes
   * the encoded fd snapshot to this handle and closes it, same as it
   * would have with a pipe it created itself. */
  uint64_t fd_snapshot_pipe_write;
  /* Valid (nonzero) only when the request had want_plain_pipe set and
   * result is 0: both ends of a fresh pipe, already duplicated into the
   * client's own process, exactly as if the client had called
   * CreatePipe() itself. */
  uint64_t plain_pipe_read;
  uint64_t plain_pipe_write;
};

/* Called from __crt_sys_fork(), before RtlCloneUserProcess runs, always
 * from a still-registered process (see reasoning in the header comment
 * above). No-op if a broker was already started earlier in this process
 * lineage -- the broker's pipe name lives in a process-wide static that
 * RtlCloneUserProcess's copy-on-write clone carries into every
 * descendant automatically, so this only does real work once per
 * lineage. Returns 0 on success (or if already started), a negative
 * errno-shaped value on failure. */
long __crt_windows_ensure_spawn_broker(void);

/* Client side of the protocol: send a spawn request to the broker and
 * wait for the response. Only meaningful (and only called) when
 * __crt_windows_is_unregistered_clone() is true. std_input/output/error
 * are HANDLE values (cast to void*) already valid in the caller's own
 * process; 0 means "no explicit std handle for this slot". On success,
 * out_pid, out_process_handle, and out_thread_handle are filled in
 * exactly as CreateProcessA would have filled a PROCESS_INFORMATION, with
 * the handles already valid in the caller's own process. Returns 0 on
 * success, a negative errno-shaped value on failure. */
long __crt_windows_spawn_broker_request(
    const char* application_path,
    const char* command_line,
    const char* current_directory,
    const char* environment_block,
    unsigned int environment_length,
    unsigned long creation_flags,
    void* std_input,
    void* std_output,
    void* std_error,
    int want_fd_snapshot_pipe,
    unsigned long* out_pid,
    void** out_process_handle,
    void** out_thread_handle,
    void** out_fd_snapshot_pipe_write);

/* Client side of the protocol: ask the broker for a plain pipe (see
 * want_plain_pipe above). Only meaningful (and only called) when
 * __crt_windows_is_unregistered_clone() is true. On success, out_read and
 * out_write are filled in with handles already valid in the caller's own
 * process, exactly as CreatePipe() would have. Returns 0 on success, a
 * negative errno-shaped value on failure. */
long __crt_windows_broker_create_pipe(void** out_read, void** out_write);

/* Entry point for a process launched as a broker (CRT_SPAWN_BROKER_MODE_ENV
 * set). Services spawn requests on the CRT_SPAWN_BROKER_PIPE_ENV named
 * pipe forever; never returns (calls ExitProcess directly on fatal
 * setup failure). */
void __crt_windows_spawn_broker_main(void);

#endif
