# Daemon Launcher

Turn a process into a well-behaved Unix daemon: detach, redirect stdio, write a pidfile, and optionally relaunch on crash.

Source code:

- [`helpers.h`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.h)
- [`helpers.c`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c)

(launch_daemon)=
### [`launch_daemon()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L9772)

`launch_daemon()` runs a program as a detached daemon (a double fork, the
daemon in a session of its own) and returns the PID of the daemon once its
`exec` is done.

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
| `program` | `const char *` | The program to run as a daemon, found in `PATH` as `execvp()` does. |
| `...` | `variadic` | Additional arguments to pass to the program, terminated by `NULL` (at most 255 in all). |

**Returns**

The PID of the daemon, or `-1`, said on stderr, when it could not be
launched: a `fork()`, `setsid()`, `/dev/null` or the `execvp()` failed, or
too many arguments.

**Notes**

- It waits for the `exec` of the daemon, not for its end: the children tell
  it the daemon's PID or their failure through a pipe that closes on the
  daemon's `exec` (`O_CLOEXEC`), read to its end. The first child is reaped.
- The PID answered is the daemon's: `kill(pid, SIGTERM)` ends it.
- Up to 7.29.3 it answered the PID of the first child, which had already
  exited (a `kill()` of it did nothing to the daemon) and was never reaped (a
  zombie per call); it read the pipe once without waiting, so an `exec`
  failure was seen only when it came first; and the daemon kept the write end
  of the pipe open.
- The daemon starts with no signal blocked and every signal at its default
  but SIGHUP, which it ignores, and with no fd of the caller but stdin,
  stdout and stderr ([`clean_child_before_exec()`](#clean_child_before_exec)).
  Up to 7.29.3 it inherited the caller's mask and ignores (a yuno's:
  SIGTERM, SIGINT, SIGQUIT and the rest of its `signalfd` set blocked) and
  every fd of the caller without `O_CLOEXEC`, and the first child left with
  `exit()`, which wrote out a second time what the caller had in its stdio
  buffers.
- A caller whose `SIGCHLD` is `SIG_IGN` (or whose handler of it reaps)
  gets the PID too, and no error is said: the first child reaped by
  somebody else is not a failure.
- A caller with stdin or stdout closed is no different: the ends of the
  pipe are put above stderr. Before, they took fds 0 and 1, the daemon's
  `/dev/null` took the place of the write end, and an `exec` failure went to
  `/dev/null`: the PID of a program never run was answered.

**Example**

```C
pid_t pid = launch_daemon(TRUE, "telnet", "localhost", "3303", NULL);
if(pid < 0) {
    return -1;  // not launched, already said
}
// ... the telnet connects to a listener of the test ...
kill(pid, SIGKILL);     // the daemon itself
```

---

(clean_child_before_exec)=
### [`clean_child_before_exec()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L10042)

`clean_child_before_exec()` prepares a forked child for its `exec`: the
program starts with no signal blocked, every signal at its default, and no
fd open but stdin, stdout, stderr and `keep_fd`. It is what
[`launch_daemon()`](#launch_daemon), [`pty_sync_spawn()`](#pty_sync_spawn)
and the command of `C_PTY` do before their `exec`
([`run_process2()`](#run_process2) and [`run_command()`](#run_command) get
the same from `posix_spawn()`).

```C
void clean_child_before_exec(int keep_fd);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `keep_fd` | `int` | An fd above stderr to keep open (a pipe that closes on the `exec` with `O_CLOEXEC`), or `-1` for none. |

**Notes**

- A yuno keeps its signals blocked for its `signalfd`, and a blocked or an
  ignored signal stays so across `exec`; an fd without `O_CLOEXEC` stays
  open across `exec`. Without this, the program inherits both.
- A signal that cannot be set (SIGKILL, SIGSTOP, those the C library keeps
  for itself) is left. The fds are closed with `close_range()`; on a kernel
  without it (before 5.9), the ones open, as `/proc/self/fd` lists them, and
  only without `/proc` one by one up to the limit of open files. Before, that
  loop was the only way, and under `LimitNOFILE=infinity` it was millions of
  `close()` per program run.
- Call it only in the child, after `fork()`: it changes the process it runs
  in. Nothing is logged: the child is about to `exec`.

**Example**

```C
pid_t pid = fork();
if(pid == 0) {
    clean_child_before_exec(-1);
    execl("/bin/sh", "sh", "-c", command, (char *)NULL);
    _exit(127);
}
```

---

(close_fd_range)=
### [`close_fd_range()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/gobj-c/src/helpers.c#L10042)

`close_fd_range()` closes every fd of `[first, last]` that is open: with
`close_range()`, or on a kernel without it (before 5.9) each one that
`/proc/self/fd` lists. It is what
[`clean_child_before_exec()`](#clean_child_before_exec) uses, and what a
yuno started with `--start` uses to close everything it inherited.

```C
void close_fd_range(unsigned int first, unsigned int last);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `first` | `unsigned int` | The first fd to close. |
| `last` | `unsigned int` | The last fd to close, `~0U` for all of them. |

**Notes**

- Only without `/proc` does it close one by one up to the limit of open
  files. Up to 7.29.3 that loop was the only way on a kernel without
  `close_range()`: under `LimitNOFILE=infinity` (`fs.nr_open`, 4000000 with
  the package's sysctl) it was four million `close()` for each program a
  yuno ran, and at each `--start`.
- Nothing is logged: a child about to `exec`, or a daemon at its start, has
  nobody to tell.

**Example**

```C
close_fd_range(STDERR_FILENO + 1, ~0U);     // keep stdin, stdout and stderr
```

---

## Linux daemon supervisor

Declared in
[`ydaemon.h`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ydaemon.h).
These entry points run a yuno under a parent "watcher" process that
relaunches the child on crash. They are only compiled on Linux
(`#ifdef __linux__`).

(daemon_run)=
### [`daemon_run()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ydaemon.c#L331)

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
  abnormally: 2 s after its end the first time; when each new child ends
  within 60 s of its start the wait doubles, up to 60 s, and a child that
  lived longer sets it back to 2 s ([Entry point §4.3](#entry-point-watcher)).
  See [`get_relaunch_times()`](#get_relaunch_times) to
  inspect how many relaunches have happened so far.
- Use [`daemon_shutdown()`](#daemon_shutdown) from another process (or
  the child itself) to terminate both watcher and child cleanly.
- A SIGQUIT to the watcher is the stop: its child is not relaunched, and
  the watcher passes the SIGQUIT to the child it waits for. So a stop that
  comes while the watcher forks a child, which the sender did not know,
  stops that child too. Up to 7.29.3 only a stop that came before the
  watcher looked at it after the fork reached the child; a later one
  reached only the watcher, and `--stop` killed the child with SIGKILL
  10 s later.
- `yuneta_entry_point()` blocks the yuno's signals
  ([`get_yuno_signal_set()`](#get_yuno_signal_set)) before it reads the
  arguments. With `--start` they stay blocked, so a SIGQUIT that comes before
  `daemon_run()` sets the watcher's signals is the stop: nothing is
  launched. Up to 7.29.3 a yuno started by hand or by a systemd unit ended
  then with a core dump. Without `--start` the signals are given back once
  the arguments are read.

---

(daemon_shutdown)=
### [`daemon_shutdown()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ydaemon.c#L584)

`daemon_shutdown()` requests an orderly shutdown of a running daemon
by process name. Every process of that name started as it (the base name
of `argv[0]` in `/proc/<pid>/cmdline`: a script of the same name has its
interpreter there and is not one; a daemon whose binary was renamed still
is, and so is another user's, which then fails with `EPERM`; a zombie of the
name is dead and is not counted, nor is an `EPERM` on one a failure; the list has no fixed size, so look-alikes
of the name, which anybody can start, cannot push the real ones out of it)
gets SIGQUIT, the
watchers first: a watcher notes it and does not relaunch its child, whatever its end;
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
not be signalled (another user's process: `EPERM`), when the `cmdline` of
one could not be read for a reason other than its end (so it is not known
whether it is the daemon), or when the list could not grow (no memory) --
each one said on stderr, and not waited for.

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
### [`get_watcher_pid()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ydaemon.c#L812)

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
### [`daemon_set_pid_file()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ydaemon.c#L804)

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
### [`get_relaunch_times()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ydaemon.c#L779)

`get_relaunch_times()` returns the number of times the watcher has
relaunched its child process since the daemon was started. Useful for
diagnostics and stats endpoints.

```C
int get_relaunch_times(void);
```

**Returns**

The relaunch counter (0 on the first run, incremented on each restart).

---

(get_yuno_signal_set)=
### [`get_yuno_signal_set()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ydaemon.c#L1043)

`get_yuno_signal_set()` fills `set` with the signals a yuno reads from its
`signalfd` (`C_YUNO`): SIGPIPE, SIGTERM, SIGALRM, SIGQUIT, SIGINT, SIGUSR1
and SIGUSR2. It is the one definition of that set: `C_YUNO` blocks it for
its `signalfd`, the watcher blocks it around each `fork()` of a child, and
`yuneta_entry_point()` blocks it from the start of a yuno.

```C
void get_yuno_signal_set(sigset_t *set);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `set` | `sigset_t *` | The set to fill. What it had is replaced. |

**Example**

```C
sigset_t yuno_signals;
get_yuno_signal_set(&yuno_signals);
sigprocmask(SIG_BLOCK, &yuno_signals, NULL);    // pending until the signalfd reads them
int sfd = signalfd(-1, &yuno_signals, SFD_NONBLOCK|SFD_CLOEXEC);
```

---

(search_process)=
### [`search_process()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ydaemon.c#L768)

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
### [`daemon_set_debug_mode()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ydaemon.c#L787)

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
### [`daemon_get_debug_mode()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ydaemon.c#L796)

`daemon_get_debug_mode()` returns whether the daemon is currently
running in debug mode.

```C
BOOL daemon_get_debug_mode(void);
```

**Returns**

`TRUE` if debug mode is enabled, `FALSE` otherwise.

---
