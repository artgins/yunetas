# Run Command

Functions for running external processes synchronously.

**Source:** `kernel/c/root-linux/src/run_command.h`

---

(run_command)=
## [`run_command()`](https://github.com/artgins/yunetas/blob/7.7.1/kernel/c/root-linux/src/run_command.c#L28)

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
## [`run_process2()`](https://github.com/artgins/yunetas/blob/7.7.1/kernel/c/root-linux/src/run_command.c#L81)

Runs a process synchronously using `fork()` with robust signal handling.

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

Process exit status, or `127` if `exec` failed.

---

(pty_sync_spawn)=
## [`pty_sync_spawn()`](https://github.com/artgins/yunetas/blob/7.7.1/kernel/c/root-linux/src/run_command.c#L163)

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
