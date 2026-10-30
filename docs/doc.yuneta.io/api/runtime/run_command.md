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
(no such program, not executable) or waited for.

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
reaches it.

```C
int pty_sync_spawn(const char *command);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `command` | `const char *` | Shell command to execute via `/bin/sh -c`. |

**Returns**

`0` on success, `-1` on failure.
