# Daemon Launcher

Turn a process into a well-behaved Unix daemon: detach, redirect stdio, write a pidfile, and optionally relaunch on crash.

Source code:

- [`helpers.h`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/gobj-c/src/helpers.h)
- [`helpers.c`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/gobj-c/src/helpers.c)

(launch_daemon)=
### [`launch_daemon()`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/gobj-c/src/helpers.c#L7372)

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

- Uses a double-fork technique to make sure that the daemon process is fully
  detached from the terminal.
- The parent process does not wait for the first child. This allows it to
  continue execution immediately.
- If `execvp()` fails, an error is written to a pipe and the function
  returns `-1`.
- The caller can use the returned PID to monitor or control the daemon
  process.

---

## Linux daemon supervisor

Declared in
[`ydaemon.h`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/root-linux/src/ydaemon.h).
These entry points run a yuno under a parent "watcher" process that
relaunches the child on crash. They are only compiled on Linux
(`#ifdef __linux__`).

(daemon_run)=
### [`daemon_run()`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/root-linux/src/ydaemon.c#L326)

`daemon_run()` starts the watcher / child supervision loop. The parent
process keeps running as a watcher that relaunches the child if it
dies. The child calls `process(process_name, work_dir, domain_dir, cleaning_fn)`
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
| `cleaning_fn` | `function pointer` | Cleanup callback invoked on shutdown. It can be `NULL`. |

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
### [`daemon_shutdown()`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/root-linux/src/ydaemon.c#L424)

`daemon_shutdown()` requests an orderly shutdown of a running daemon
by process name. Every process of that name gets SIGQUIT, the watchers
first: a watcher notes it and does not relaunch its child, whatever its end;
the child shuts down in order and exits 0, and its watcher exits with it.
They are given 10 s to be gone; then the name is scanned again and what is
left is killed with SIGKILL, said on stderr. Each `kill()` is checked.

```C
int daemon_shutdown(const char *process_name);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `process_name` | `const char *` | Name of the running daemon process to stop. |

**Returns**

`0` when every process of the name is gone or killed; `-1` when one could
not be signalled (another user's process: `EPERM`), said on stderr and not
waited for.

**Notes**

No-op if no process with the given name is found. The calling process is
never signaled. It returns once every process is gone (a zombie counts as
gone), so a start that follows does not meet the old one. Up to 7.25.21 each
process was killed 1 s after its own SIGQUIT, one after the other: an agent
had one second for its orderly shutdown. Up to 7.25.22 it returned nothing:
an `EPERM` waited 10 s, said *"killed (SIGKILL)"* and the `--stop` exited 0;
and a child that crashed in its shutdown was relaunched by its watcher and
left alive once the watcher was killed.

**Example**

```C
if(arguments.stop) {
    return daemon_shutdown(APP_NAME) < 0? 1 : 0;  // returns when the daemon is gone
}
```

---

(get_watcher_pid)=
### [`get_watcher_pid()`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/root-linux/src/ydaemon.c#L588)

`get_watcher_pid()` returns the PID of the watcher (parent) process
that is supervising the current child, or `0` if the caller is not
running under a watcher.

```C
int get_watcher_pid(void);
```

**Returns**

The watcher process PID, or `0` if the current process has no watcher.

---

(daemon_set_pid_file)=
### [`daemon_set_pid_file()`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/root-linux/src/ydaemon.c#L580)

`daemon_set_pid_file()` names the file where, under `--start`, the pid of
the WATCHER is written -- by the process that was started, before it exits,
which is when a systemd unit of `Type=forking` reads its `PIDFile=`. The
entry point calls it with the value of `--pid-file`; a yuno has nothing else
to do for it.

```C
void daemon_set_pid_file(const char *path);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | The file. It must live until `daemon_run()` (an argv string does). NULL or empty: no file. |

**Example**

```bash
/yuneta/agent/yuneta_agent --config-file=/yuneta/agent/yuneta_agent.json \
    --start --pid-file=/run/yuneta_agent/yuneta_agent.pid
cat /run/yuneta_agent/yuneta_agent.pid      # the watcher, the unit's main pid
```

**Notes**

The file is written aside (`<file>.tmp`) and renamed, so systemd reads it
whole or not at all. A failure to write it is printed and sent to syslog; the
daemon goes on. The unit that uses it is described in
[the entry point chapter](#entry-point-watcher).

---

(get_relaunch_times)=
### [`get_relaunch_times()`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/root-linux/src/ydaemon.c#L555)

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
### [`search_process()`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/root-linux/src/ydaemon.c#L544)

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
### [`daemon_set_debug_mode()`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/root-linux/src/ydaemon.c#L563)

`daemon_set_debug_mode()` enables or disables daemon debug mode for
the current process. When debug mode is on the supervisor emits extra
tracing and can skip behaviors that will make debugging harder (for
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
### [`daemon_get_debug_mode()`](https://github.com/artgins/yunetas/blob/7.25.22/kernel/c/root-linux/src/ydaemon.c#L572)

`daemon_get_debug_mode()` returns whether the daemon is currently
running in debug mode.

```C
BOOL daemon_get_debug_mode(void);
```

**Returns**

`TRUE` if debug mode is enabled, `FALSE` otherwise.

---
