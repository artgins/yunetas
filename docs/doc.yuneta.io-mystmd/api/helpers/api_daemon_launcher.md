# Daemon Functions

Source code in:

- [helpers.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [helpers.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(launch_daemon)=
## `launch_daemon()`

`launch_daemon()` creates a detached daemon process by performing a double fork and returns the PID of the first child process.

```C
pid_t launch_daemon(
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

['Uses a double fork technique to ensure the daemon process is fully detached from the terminal.', 'The parent process does not wait for the first child, allowing it to continue execution immediately.', 'If `execvp()` fails, an error is written to a pipe and the function returns `-1`.', 'The caller can use the returned PID to monitor or control the daemon process.']

---
