# Run Command

Functions for running external processes synchronously.

Every program run by them starts with **no signal blocked and every signal
at its default**, whatever the caller has. A yuno keeps SIGPIPE, SIGTERM,
SIGALRM, SIGQUIT, SIGINT, SIGUSR1 and SIGUSR2 blocked for its `signalfd`,
and a blocked or an ignored signal stays so across `exec`: up to 7.29.3
every program a yuno ran inherited them (`SigBlk: 0x7a06`), so a `timeout`
it ran could not stop its child with SIGTERM. Check it on a running
program:

```bash
grep -E 'SigIgn|SigBlk' /proc/<pid>/status   # both 0
```

Every program run by them also starts with **no fd of the caller but
stdin, stdout and stderr**. An fd without `O_CLOEXEC` stays open across
`exec`: up to 7.29.3 every program a yuno ran inherited each one (a
listening socket, a log file, the other end of a pipe), and kept it open
after the yuno closed it. On a glibc older than 2.34 (no
`posix_spawn_file_actions_addclosefrom_np()`) each open fd is closed one by
one, and one at or above the soft limit of open files, which
`posix_spawn_file_actions_addclose()` refuses, is given `O_CLOEXEC` instead;
before, such an fd failed the whole spawn. Check it on a running program:

```bash
ls -l /proc/<pid>/fd    # 0, 1 and 2 only (and what the program opened)
```

**Source:** `kernel/c/root-linux/src/run_command.h`

---

(run_command)=
## [`run_command()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/run_command.c#L29)

Runs a command with `/bin/sh -c` (`posix_spawn()`), waits for its end and
returns its output, stdout and stderr in one.

```C
gbuffer_t *run_command(const char *command);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `command` | `const char *` | Shell command string to execute. |

**Returns**

`gbuffer_t` containing the command output, or `NULL` (logged) when the
command could not be run. The exit status is not returned: use
[`run_process2()`](#run_process2) with `sh -c` when it matters.

**Notes**

- stdout and stderr of the whole command go to the output, every command
  of a list or a pipeline. Up to 7.29.3 it was `popen()` of the command
  with `2>&1` appended, which took the stderr of the last command only,
  in a buffer of 4 KB that cut a longer command with nothing said.
- An output larger than the largest gbuffer is cut, with an error logged.
- The command's stdin is `/dev/null`. Up to 7.29.3 it was the caller's.
- **It returns when the output ends, not when the command ends.** The
  output ends when no process has the pipe as its stdout or stderr. A
  process that the command leaves in the background keeps the pipe, and
  `run_command()` waits for it. Give such a process another stdout **and**
  another stderr:

  ```C
  run_command("sleep 9 >/dev/null &");           // returns after 9 s: stderr is the pipe
  run_command("sleep 9 >/dev/null 2>&1 &");      // returns at once
  ```

  Up to 7.29.3 (`popen()` with `2>&1` appended) the first form returned at
  once, because only the stdout of the last command was the pipe.
- A command reaped by somebody else (the caller's `SIGCHLD` at `SIG_IGN`,
  or a handler of it that reaps) is not an error: the output is returned.
- When it cannot be run, the ERROR says which call failed (`"failed"`:
  `posix_spawn()`, or the call that prepared it, such as
  `posix_spawnattr_setsigmask()`), with its `errno`.

**Example**

```C
gbuffer_t *gbuf = run_command("'/yuneta/agent/yunos/mqtt_broker' --print-role");
if(gbuf) {
    json_t *jn = gbuf2json(gbuf, 2);   // gbuf owned: the json the program wrote
    // ...
    JSON_DECREF(jn)
}
```

---

(run_process2)=
## [`run_process2()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/run_command.c#L83)

Runs a program and waits for its end: `posix_spawn()` and `waitpid()`, with no
shell, so `argv` reaches the program as it is. The program starts with no
signal blocked and every signal at its default (up to 7.29.3 it inherited the
caller's mask and ignores). It is how the agent launches a yuno (`run-yuno`),
and how a yuno runs a shell command when it needs its status:
`argv = {"sh", "-c", command, NULL}` on `/bin/sh`, in place of `system()`,
which gives the command the caller's mask and sets SIGQUIT to `SIG_IGN`
while it waits.

```C
int run_process2(
    const char *path,
    char *const argv[]
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | Full path to the executable. |
| `argv` | `char *const []` | NULL-terminated argument vector. |

**Returns**

The status of `waitpid()`: `0` when the program exited with 0 (read the rest
with `WIFEXITED()` / `WEXITSTATUS()`). `-1`, logged, when it could not be run
(no such program, not executable) or waited for. The ERROR names the call
that failed (`"failed"`). A caller whose `SIGCHLD` is `SIG_IGN` gets `-1`:
the kernel reaps the program, and its status is lost.

**Notes**

Unlike `system()`, the caller's `SIGINT` and `SIGQUIT` are not ignored while it
waits: a yuno takes them from a `signalfd`, and setting them to `SIG_IGN`
would throw away a stop that arrives meanwhile. Up to 7.28.4 this was a port
of the `system()` of *The Linux Programming Interface* (GPL-3.0), which did;
it also answered `127` (in the status) for a program that could not be run.

**Example**

```C
char *const argv[] = {"mqtt_broker", "--config-file", cfg, "--start", NULL};
int status = run_process2("/yuneta/agent/yunos/mqtt_broker", argv);
if(status != 0) {
    // -1: not run (logged); else the program failed: WEXITSTATUS(status)
}

char *const sh[] = {"sh", "-c", "/yuneta/store/certs/copy-certs.sh", NULL};
status = run_process2("/bin/sh", sh);   // a shell command, with its status
```

---

(pty_sync_spawn)=
## [`pty_sync_spawn()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/run_command.c#L134)

Spawns a command synchronously in a pseudo-terminal (via `forkpty()`),
providing interactive terminal I/O between parent and child. The command
starts with no signal blocked and every signal at its default, so a ctrl+c
reaches it, and with no fd of the caller but its terminal.

When the caller's stdin ends (a pipe, `/dev/null`), the command gets the
end of its terminal's input, as a person types it: the end-of-file
character of the terminal (`VEOF`, ctrl+d), twice after a line not ended
(the first gives the line). So `cat >/dev/null; echo DONE` ends. Nothing is
sent when the command put its terminal in raw mode, where that character is
a byte like any other. Up to 7.29.3 the end of stdin was read again at once,
for ever, and each time one byte that was never read went to the command:
about 500 000 bytes a second, and never the end.

When the caller's stdin is a terminal, each ctrl+d typed there is passed
on, and what is typed after it goes on reaching the command. Any other
stdin ends once, and so the command gets one end of input: a terminal has
no end that stays, as a pipe has. Only the first reader of the terminal
sees it, and a second one waits. With `abc` and a newline on a pipe,
`cat >/dev/null; echo A; cat >/dev/null; echo B` prints `A` and waits;
typed on a terminal, a second ctrl+d gets to `B`.

The call ends with the command, not with a process the command left in the
background: the exit is seen by a pidfd (`pidfd_open()`), so
`(trap '' HUP; exec sleep 3) & sleep 0.2` takes 0.2 s, not 3 s. Without a
pidfd the exit is looked for every 100 ms. A kernel older than 5.3, where
`pidfd_open()` fails, gets a WARNING at each call (*"pidfd_open() FAILED:
the end of the command is looked for every 100 ms"*, with its `errno`). A
build whose kernel headers are older than 5.3, on an architecture whose
syscall number the source does not carry, makes no call: one WARNING per
process says so (*"built without pidfd_open(): the end of a command is
looked for every 100 ms (said once)"*). Then what the command left in the terminal is passed on
without waiting for such a process, the terminal is closed, and the
command is reaped. Up to 7.29.3 the terminal was never closed (one fd per
call), and a command whose terminal was read to its end before its exit
was seen was not reaped (a zombie) and answered `-1`.

With `SIGCHLD` at `SIG_IGN` in the caller the kernel reaps the command and
its exit status is lost: the call answers `0` and logs nothing. That holds
for a command that ends before its pidfd is opened too (`true`): the
kernel has no process with its pid then, and the call takes that as the
end of the command. The call cannot tell how the command ended, so a
command killed by a signal (`kill -9`) also answers `0`.

```C
int pty_sync_spawn(const char *command);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `command` | `const char *` | Shell command to execute via `/bin/sh -c`. |

**Returns**

`0` when the command exited, whatever its exit code (also when the kernel
reaped it, with `SIGCHLD` ignored: then a command killed by a signal
answers `0` too); `-1` when it could not be run, was killed by a signal, or
could not be followed (logged).
