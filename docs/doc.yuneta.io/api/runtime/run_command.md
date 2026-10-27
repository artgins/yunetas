# Run Command

Functions for running external processes synchronously.

**Source:** `kernel/c/root-linux/src/run_command.h`

---

(run_command)=
## [`run_command()`](https://github.com/artgins/yunetas/blob/7.28.10/kernel/c/root-linux/src/run_command.c#L29)

Runs a shell command synchronously via `popen()` and captures its
combined stdout/stderr output.

```C
gbuffer_t *run_command(const char *command);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `command` | `const char *` | Shell command string to execute. |

**Returns**

`gbuffer_t` containing the command output, or `NULL` on error.

**Notes**

- `2>&1` is appended automatically to capture stderr.

---

(run_process2)=
## [`run_process2()`](https://github.com/artgins/yunetas/blob/7.28.10/kernel/c/root-linux/src/run_command.c#L83)

Runs a program and waits for its end: `posix_spawn()` and `waitpid()`, with no
shell, so `argv` reaches the program as it is. The program inherits the signal
mask and the ignored signals of the caller. It is how the agent launches a
yuno (`run-yuno`).

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
```

---

(pty_sync_spawn)=
## [`pty_sync_spawn()`](https://github.com/artgins/yunetas/blob/7.28.10/kernel/c/root-linux/src/run_command.c#L134)

Spawns a command synchronously in a pseudo-terminal (via `forkpty()`),
providing interactive terminal I/O between parent and child.

```C
int pty_sync_spawn(const char *command);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `command` | `const char *` | Shell command to execute via `/bin/sh -c`. |

**Returns**

`0` on success, `-1` on failure.
