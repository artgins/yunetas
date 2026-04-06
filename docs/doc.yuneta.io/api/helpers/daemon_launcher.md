# Daemon Launcher

Turn a process into a well-behaved Unix daemon: detach, redirect stdio, write a pidfile, and optionally relaunch on crash.

Source code:

- [`helpers.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [`helpers.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(launch_daemon)=
### `launch_daemon()`

`launch_daemon()` creates a detached daemon process by performing a double fork and returns the PID of the first child process.

```C
int launch_daemon(
    BOOL redirect_stdio_to_null,
    const char *program,
    ...
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `redirect_stdio_to_null` | `BOOL` | If `TRUE`, redirects standard input, output, and error to `/dev/null`. |
| `program` | `const char *` | The name of the program to execute as a daemon. |
| `...` | `variadic` | Additional arguments to pass to the program, terminated by `NULL`. |

**Returns**

Returns the PID of the first child process if successful, or `-1` if an error occurs.

**Notes**

- Uses a double-fork technique to ensure the daemon process is fully
  detached from the terminal.
- The parent process does not wait for the first child, allowing it to
  continue execution immediately.
- If `execvp()` fails, an error is written to a pipe and the function
  returns `-1`.
- The caller can use the returned PID to monitor or control the daemon
  process.

---

## Linux daemon supervisor

Declared in
[`ydaemon.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/root-linux/src/ydaemon.h).
These entry points run a yuno under a parent "watcher" process that
relaunches the child on crash. They are only compiled on Linux
(`#ifdef __linux__`).

(daemon_run)=
### `daemon_run()`

`daemon_run()` starts the watcher / child supervision loop. The parent
process keeps running as a watcher that relaunches the child if it
dies; the child calls `process(process_name, work_dir, domain_dir, cleaning_fn)`
to do the actual work.

```C
int daemon_run(
    void (*process)(
        const char *process_name,
        const char *work_dir,
        const char *domain_dir,
        void (*cleaning_fn)(void)
    ),
    const char *process_name,
    const char *work_dir,
    const char *domain_dir,
    void (*cleaning_fn)(void)
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `process` | `function pointer` | Entry point invoked in the child. Receives the process name, work dir, domain dir and the cleanup callback. |
| `process_name` | `const char *` | Name of the process (used in `/proc` lookups and logs). |
| `work_dir` | `const char *` | Working directory to `chdir` into before running. |
| `domain_dir` | `const char *` | Domain-specific directory passed through to the child. |
| `cleaning_fn` | `function pointer` | Cleanup callback invoked on shutdown. May be `NULL`. |

**Returns**

Returns `0` on normal shutdown, or a non-zero value on error.

**Notes**

- The watcher process relaunches the child each time it exits
  abnormally. See [`get_relaunch_times()`](#get_relaunch_times) to
  inspect how many relaunches have happened so far.
- Use [`daemon_shutdown()`](#daemon_shutdown) from another process (or
  the child itself) to terminate both watcher and child cleanly.

---

(daemon_shutdown)=
### `daemon_shutdown()`

`daemon_shutdown()` requests an orderly shutdown of a running daemon
by process name. The function locates the watcher process and signals
it; the watcher in turn signals its child and terminates.

```C
void daemon_shutdown(const char *process_name);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `process_name` | `const char *` | Name of the running daemon process to shut down. |

**Returns**

This function does not return a value.

**Notes**

No-op if no process with the given name is found.

---

(get_watcher_pid)=
### `get_watcher_pid()`

`get_watcher_pid()` returns the PID of the watcher (parent) process
that is supervising the current child, or `0` if the caller is not
running under a watcher.

```C
int get_watcher_pid(void);
```

**Returns**

The watcher process PID, or `0` if the current process has no watcher.

---

(get_relaunch_times)=
### `get_relaunch_times()`

`get_relaunch_times()` returns the number of times the watcher has
relaunched its child process since the daemon was started. Useful for
diagnostics and stats endpoints.

```C
int get_relaunch_times(void);
```

**Returns**

The relaunch counter (0 on the first run, incremented on each restart).

---

(search_process)=
### `search_process()`

`search_process()` walks `/proc` looking for running processes whose
name matches `process_name` and invokes a callback for each match.

```C
int search_process(
    const char *process_name,
    void (*cb)(void *self, const char *name, pid_t pid),
    void *self
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `process_name` | `const char *` | Target process name to match against `/proc/*/comm` (or `/proc/*/cmdline`). |
| `cb` | `function pointer` | Callback invoked for each matching process. Receives the opaque `self`, the resolved process name, and the PID. |
| `self` | `void *` | Opaque pointer forwarded to the callback. |

**Returns**

Returns the number of matches found, or a negative value on error.

**Notes**

Used internally by [`daemon_shutdown()`](#daemon_shutdown) to locate
the watcher process to signal.

---

(daemon_set_debug_mode)=
### `daemon_set_debug_mode()`

`daemon_set_debug_mode()` enables or disables daemon debug mode for
the current process. When debug mode is on the supervisor emits extra
tracing and may skip behaviours that would make debugging harder (for
example, preventing auto-relaunch on crash).

```C
int daemon_set_debug_mode(BOOL set);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `set` | `BOOL` | `TRUE` to enable debug mode, `FALSE` to disable it. |

**Returns**

Returns `0` on success.

---

(daemon_get_debug_mode)=
### `daemon_get_debug_mode()`

`daemon_get_debug_mode()` returns whether the daemon is currently
running in debug mode.

```C
BOOL daemon_get_debug_mode(void);
```

**Returns**

`TRUE` if debug mode is enabled, `FALSE` otherwise.

---
