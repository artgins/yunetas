# Log

Structured logging API. Every log call emits a JSON record with severity, gobj context, and user-supplied key/value pairs. Handlers (UDP, rotatory file, stdout…) are registered separately and receive a copy of each record.

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)

(glog_end)=
## `glog_end()`

The `glog_end()` function deinitializes the logging system, freeing allocated resources and unregistering log handlers.

```C
void glog_end(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

**Notes**

This function should generally not be called, as the logging system consumes minimal memory and should remain available throughout the program's execution.

---

(glog_init)=
## `glog_init()`

`glog_init()` initializes the global logging system, setting up default log handlers and registering built-in log handler types.

```C
void glog_init(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

**Notes**

This function ensures that the logging system is initialized only once. It registers built-in log handlers such as `stdout`, `file`, and `udp`.

---

(gobj_get_log_data)=
## `gobj_get_log_data()`

Retrieves a JSON object containing log statistics, including counts of different log levels such as debug, info, warning, error, critical, and alert.

```C
json_t *gobj_get_log_data(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A JSON object containing log statistics with keys: 'debug', 'info', 'warning', 'error', 'critical', and 'alert', each holding the respective count.

**Notes**

The returned JSON object must be managed by the caller, ensuring proper memory deallocation when no longer needed.

---

(gobj_get_log_priority_name)=
## `gobj_get_log_priority_name()`

Retrieves the string representation of a log priority level based on its integer value.

```C
const char *gobj_get_log_priority_name(int priority);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `priority` | `int` | The log priority level, expected to be within predefined system log levels. |

**Returns**

A string representing the name of the log priority level. If the priority is out of range, an empty string is returned.

**Notes**

The function maps integer priority levels to predefined log level names such as `LOG_ERR`, `LOG_WARNING`, and `LOG_DEBUG`.

---

(gobj_info_msg)=
## `gobj_info_msg()`

`gobj_info_msg()` logs an informational message with a formatted string and optional arguments.

```C
void gobj_info_msg(
    hgobj gobj,
    const char *fmt,
    ... ) JANSSON_ATTRS((format(printf, 2, 3))
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The object generating the log message. |
| `fmt` | `const char *` | The format string for the log message. |
| `...` | `variadic` | Additional arguments for the format string. |

**Returns**

This function does not return a value.

**Notes**

The log message is formatted using `vsnprintf()` and logged with priority `LOG_INFO`.

---

(gobj_log_add_handler)=
## `gobj_log_add_handler()`

Registers a new log handler with the specified name, type, options, and handler object. The function ensures that the handler name is unique and associates it with a registered handler type.

```C
int gobj_log_add_handler(
    const char *handler_name,
    const char *handler_type,
    log_handler_opt_t handler_options,
    void *h
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `handler_name` | `const char *` | The unique name of the log handler to be added. |
| `handler_type` | `const char *` | The type of the log handler, which must be previously registered. |
| `handler_options` | `log_handler_opt_t` | Bitmask of options that configure the behavior of the log handler. |
| `h` | `void *` | A pointer to the handler-specific data structure. |

**Returns**

Returns 0 on success, or -1 if the handler name already exists or the handler type is not found.

**Notes**

The function checks if the handler name is already registered and ensures that the handler type exists before adding the handler. If the handler type is not found, the function returns an error.

---

(gobj_log_alert)=
## `gobj_log_alert()`

Logs an alert message with priority `LOG_ALERT`. The message is formatted using a variable argument list and processed by the logging system.

```C
void gobj_log_alert(
    hgobj gobj,
    log_opt_t opt,
    ...
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The object generating the log message. |
| `opt` | `log_opt_t` | Logging options that control behavior such as stack tracing or process termination. |
| `...` | `variadic` | Format string followed by arguments for message formatting. |

**Returns**

This function does not return a value.

**Notes**

The function increments the global alert log counter and processes the message through registered log handlers. If `LOG_OPT_TRACE_STACK` is set, a stack trace is included in the log output.

---

(gobj_log_clear_counters)=
## `gobj_log_clear_counters()`

Resets all internal log counters, including debug, info, warning, error, critical, and alert counts.

```C
void gobj_log_clear_counters(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

**Notes**

This function is useful for resetting log statistics before starting a new monitoring session.

---

(gobj_log_critical)=
## `gobj_log_critical()`

Logs a critical message with the specified format and arguments. The message is processed by registered log handlers.

```C
void gobj_log_critical(
    hgobj gobj,
    log_opt_t opt,
    ...
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The object generating the log message. Can be NULL. |
| `opt` | `log_opt_t` | Logging options that control behavior such as stack tracing or process termination. |
| `...` | `variadic` | Format string followed by arguments, similar to printf. |

**Returns**

None.

**Notes**

Critical logs indicate severe conditions that require immediate attention. The function internally calls `_log_jnbf()` to process the log message.

---

(gobj_log_debug)=
## `gobj_log_debug()`

Logs a debug message with a specified priority and options. `gobj_log_debug()` formats the message using variadic arguments and sends it to registered log handlers.

```C
void gobj_log_debug(
    hgobj gobj,
    log_opt_t opt,
    ...
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance associated with the log message. |
| `opt` | `log_opt_t` | Logging options that control behavior such as stack tracing or exit conditions. |
| `...` | `variadic` | Format string followed by arguments for message formatting. |

**Returns**

This function does not return a value.

**Notes**

The function internally calls `_log_jnbf()` to process and dispatch the log message. It increments the global debug log counter.

---

(gobj_log_del_handler)=
## `gobj_log_del_handler()`

Removes a registered log handler by its name. If the handler name is empty, all handlers are removed.

```C
int gobj_log_del_handler(
    const char *handler_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `handler_name` | `const char *` | The name of the log handler to remove. If empty, all handlers are removed. |

**Returns**

Returns 0 if the handler was successfully removed, or -1 if the handler was not found.

**Notes**

If a handler is removed, its associated resources are freed, and if it has a close function, it is called.

---

(gobj_log_error)=
## `gobj_log_error()`

The function `gobj_log_error()` logs an error message with a specified priority level and optional formatting arguments.

```C
void gobj_log_error(
    hgobj gobj,
    log_opt_t opt,
    ...
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance associated with the log entry. |
| `opt` | `log_opt_t` | Logging options that control behavior such as stack tracing or process termination. |
| `...` | `variadic` | A variadic list of arguments for formatted message output. |

**Returns**

This function does not return a value.

**Notes**

The function increments the global error count and formats the log message before passing it to the logging system. It supports optional stack tracing and process termination based on the `opt` parameter.

---

(gobj_log_exist_handler)=
## `gobj_log_exist_handler()`

Checks if a log handler with the specified name exists in the system.

```C
BOOL gobj_log_exist_handler(
    const char *handler_name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `handler_name` | `const char *` | The name of the log handler to check. |

**Returns**

Returns `TRUE` if the log handler exists, otherwise returns `FALSE`.

**Notes**

If `handler_name` is empty or `NULL`, the function returns `FALSE`.
This function ensures that the logging system is initialized before performing the check.

---

(gobj_log_info)=
## `gobj_log_info()`

Logs an informational message with optional formatting and arguments. The message is processed and sent to registered log handlers.

```C
void gobj_log_info(
    hgobj gobj,
    log_opt_t opt,
    ...
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance associated with the log message. |
| `opt` | `log_opt_t` | Logging options that control additional behavior such as stack tracing or exit conditions. |
| `...` | `variadic` | Format string followed by optional arguments, similar to printf. |

**Returns**

This function does not return a value.

**Notes**

The function formats the message and sends it to all registered log handlers that accept informational messages. It uses `_log_jnbf()` internally to process the log entry.

---

(gobj_log_last_message)=
## `gobj_log_last_message()`

Retrieves the last logged message recorded by the logging system.

```C
const char *gobj_log_last_message(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A pointer to a string containing the last logged message. The returned string is managed internally and should not be modified or freed by the caller.

**Notes**

This function provides access to the most recent log message, which can be useful for debugging or monitoring purposes.

---

(gobj_log_list_handlers)=
## `gobj_log_list_handlers()`

Retrieves a list of registered log handlers, returning a JSON array with details about each handler.

```C
json_t *gobj_log_list_handlers(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A JSON array containing dictionaries with details about each registered log handler. Each dictionary includes 'handler_name', 'handler_type', and 'handler_options'.

**Notes**

The returned JSON array must be managed by the caller to avoid memory leaks.

---

(gobj_log_register_handler)=
## `gobj_log_register_handler()`

Registers a new log handler by specifying its type and associated functions for logging and formatting messages.

```C
int gobj_log_register_handler(
    const char *handler_type,
    loghandler_close_fn_t close_fn,
    loghandler_write_fn_t write_fn,
    loghandler_fwrite_fn_t fwrite_fn
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `handler_type` | `const char *` | The name of the log handler type to be registered. |
| `close_fn` | `loghandler_close_fn_t` | Function pointer for closing the log handler, or NULL if not applicable. |
| `write_fn` | `loghandler_write_fn_t` | Function pointer for writing log messages. |
| `fwrite_fn` | `loghandler_fwrite_fn_t` | Function pointer for formatted writing of log messages. |

**Returns**

Returns 0 on success, or -1 if the maximum number of log handler types has been reached.

**Notes**

This function allows the registration of custom log handlers, which can be later used with [`gobj_log_add_handler()`](#gobj_log_add_handler).

---

(gobj_log_set_global_handler_option)=
## `gobj_log_set_global_handler_option()`

Sets or clears a global log handler option in the logging system.

```C
int gobj_log_set_global_handler_option(
    log_handler_opt_t log_handler_opt,
    BOOL              set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `log_handler_opt` | `log_handler_opt_t` | The log handler option to be modified. |
| `set` | `BOOL` | If `TRUE`, the option is enabled; if `FALSE`, the option is disabled. |

**Returns**

Returns `0` on success.

**Notes**

This function modifies global logging behavior by enabling or disabling specific log handler options.

---

(gobj_log_set_last_message)=
## `gobj_log_set_last_message()`

Sets the last log message for retrieval using [`gobj_log_last_message()`](#gobj_log_last_message).

```C
void gobj_log_set_last_message(
    const char *msg,
    ...) JANSSON_ATTRS((format(printf, 1, 2))
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `const char *` | The format string for the log message. |
| `...` | `variadic` | Additional arguments for the format string. |

**Returns**

This function does not return a value.

**Notes**

The stored message can be retrieved using [`gobj_log_last_message()`](#gobj_log_last_message).

---

(gobj_log_warning)=
## `gobj_log_warning()`

Logs a warning message with the specified format and arguments. The function increments the internal warning counter and processes the log message through registered handlers.

```C
void gobj_log_warning(
    hgobj gobj,
    log_opt_t opt,
    ...
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The object generating the log message. Can be NULL if not associated with a specific object. |
| `opt` | `log_opt_t` | Logging options that control additional behavior such as stack tracing or process termination. |
| `...` | `variadic` | Format string followed by arguments, similar to printf, specifying the log message. |

**Returns**

This function does not return a value.

**Notes**

The function checks if logging is initialized before proceeding. If logging is already in progress, it prevents recursive calls. The log message is processed by registered handlers, and if configured, a backtrace may be printed.

---

(gobj_trace_dump)=
## `gobj_trace_dump()`

The `gobj_trace_dump()` function logs a hex dump of a given buffer with an associated formatted message.

```C
void gobj_trace_dump(
    hgobj gobj,
    const char *bf,
    size_t len,
    const char *fmt,
    ... ) JANSSON_ATTRS((format(printf, 4, 5))
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance associated with the trace log. |
| `bf` | `const char *` | Pointer to the buffer containing the data to be dumped. |
| `len` | `size_t` | The length of the buffer in bytes. |
| `fmt` | `const char *` | A format string for the log message. |
| `...` | `variadic` | Additional arguments for the format string. |

**Returns**

This function does not return a value.

**Notes**

The `gobj_trace_dump()` function is useful for debugging by providing a hex dump of a buffer. It formats the message using the provided format string and arguments before logging the output.

---

(gobj_trace_json)=
## `gobj_trace_json()`

Logs a JSON object with an associated message for debugging purposes. The function formats the log entry with metadata such as timestamp, gobj attributes, and system memory usage.

```C
void gobj_trace_json(
    hgobj gobj,
    json_t *jn,
    const char *fmt,
    ... ) JANSSON_ATTRS((format(printf, 3, 4))
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The gobj instance associated with the log entry. |
| `jn` | `json_t *` | The JSON object to be logged. This parameter is not owned by the function. |
| `fmt` | `const char *` | A format string for the log message. |
| `...` | `variadic` | Additional arguments for formatting the log message. |

**Returns**

This function does not return a value.

**Notes**

If the gobj has the `TRACE_GBUFFERS` trace level enabled, the function will also log the contents of the associated [`gbuffer_t`](#gbuffer_t).

---

(gobj_trace_msg)=
## `gobj_trace_msg()`

Logs a debug-level message with formatted text. The function formats the message using `printf`-style arguments and logs it with priority `LOG_DEBUG`.

```C
void gobj_trace_msg(
    hgobj gobj,
    const char *fmt,
    ... ) JANSSON_ATTRS((format(printf, 2, 3))
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance associated with the log message. |
| `fmt` | `const char *` | The format string, similar to `printf`. |
| `...` | `variadic` | Additional arguments for the format string. |

**Returns**

This function does not return a value.

**Notes**

Internally, [`trace_vjson()`](#trace_vjson) is used to format and log the message.

---

(print_backtrace)=
## `print_backtrace()`

The `print_backtrace()` function prints a backtrace of the current execution stack using the configured backtrace function.

```C
void print_backtrace(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

**Notes**

The function relies on the `show_backtrace_fn` callback function to perform the actual backtrace printing. If `show_backtrace_fn` is not set, no backtrace will be printed.

---

(print_error)=
## `print_error()`

The `print_error()` function prints an error message to stdout and syslog, and optionally exits or aborts the program based on the specified flag.

```C
void print_error(
    pe_flag_t quit,
    const char *fmt,
    ... ) JANSSON_ATTRS((format(printf, 2, 3))
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `quit` | `pe_flag_t` | Determines the behavior after printing the error message. Possible values: `PEF_CONTINUE` (continue execution), `PEF_EXIT` (exit with -1), `PEF_ABORT` (abort execution), or `PEF_SYSLOG` (log to syslog). |
| `fmt` | `const char *` | The format string for the error message, similar to `printf()`. |
| `...` | `variadic arguments` | Additional arguments corresponding to the format specifiers in `fmt`. |

**Returns**

This function does not return a value.

**Notes**

If `quit` is set to `PEF_ABORT`, the function will call `abort()`. If `quit` is set to `PEF_EXIT`, the function will call `exit(-1)`. The function also logs the error message to syslog on Linux systems.

---

(set_show_backtrace_fn)=
## `set_show_backtrace_fn()`

Sets the function pointer for handling backtrace display. The provided function will be used to print backtrace information when logging errors or critical messages.

```C
void set_show_backtrace_fn(
    show_backtrace_fn_t show_backtrace_fn
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `show_backtrace_fn` | `show_backtrace_fn_t` | Function pointer to the backtrace display handler. |

**Returns**

This function does not return a value.

**Notes**

If `show_backtrace_fn` is set to `NULL`, backtrace logging will be disabled.

---

(stdout_fwrite)=
## `stdout_fwrite()`

`stdout_fwrite()` writes a formatted message to the standard output stream using a specified priority level.

```C
int stdout_fwrite(
    void* v,
    int priority,
    const char* format,
    ...
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `v` | `void *` | Unused parameter, included for compatibility with log handler function signatures. |
| `priority` | `int` | The priority level of the log message, determining its severity. |
| `format` | `const char *` | The format string specifying how subsequent arguments are formatted. |
| `...` | `variadic` | Additional arguments corresponding to the format specifiers in `format`. |

**Returns**

Returns `0` on success.

**Notes**

This function formats the message using `vsnprintf()` and writes it to `stdout`. It applies ANSI color codes based on the priority level.

---

(stdout_write)=
## `stdout_write()`

`stdout_write()` writes a log message to the standard output with a priority label.

```C
int stdout_write(
    void   *v,
    int     priority,
    const char *bf,
    size_t  len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `v` | `void *` | Unused parameter, included for compatibility with log handler function signatures. |
| `priority` | `int` | Log priority level, determining the severity of the message. |
| `bf` | `const char *` | Pointer to the log message string. |
| `len` | `size_t` | Length of the log message string. |

**Returns**

Returns 0 on success, or -1 if the input parameters are invalid.

**Notes**

The function applies ANSI color codes to differentiate log levels visually.

---

(trace_msg0)=
## `trace_msg0()`

The `trace_msg0()` function logs a debug message with a formatted string.

```C
int trace_msg0(
    const char *fmt,
    ...) JANSSON_ATTRS((format(printf, 1, 2))
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fmt` | `const char *` | The format string, similar to `printf()`. |
| `...` | `variadic` | Additional arguments corresponding to the format specifiers in `fmt`. |

**Returns**

Returns 0 after logging the message.

**Notes**

This function formats the message using `vsnprintf()` and logs it with a debug priority.

---

(trace_vjson)=
## `trace_vjson()`

Logs a JSON-formatted message with optional structured data, using a specified priority level.

```C
void trace_vjson(
    hgobj gobj,
    int priority,
    json_t *jn_data,
    const char *msgset,
    const char *fmt,
    va_list ap
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance associated with the log message. |
| `priority` | `int` | The priority level of the log message, typically using syslog priority values. |
| `jn_data` | `json_t *` | Optional JSON data to include in the log message. Not owned by the function. |
| `msgset` | `const char *` | A string identifier categorizing the log message. |
| `fmt` | `const char *` | A format string for the log message, similar to printf. |
| `ap` | `va_list` | A variable argument list containing values to format into the log message. |

**Returns**

This function does not return a value.

**Notes**

The function formats the log message as JSON, including metadata such as timestamps and system information. It is used internally by logging functions like [`gobj_trace_json()`](#gobj_trace_json) and [`gobj_trace_msg()`](#gobj_trace_msg).

---

(_log_bf)=
## `_log_bf()`

*Description pending — signature extracted from header.*

```C
void _log_bf(
    int priority,
    log_opt_t opt,
    const char *bf,
    size_t len
);
```

---

(gobj_log_clear_log_file)=
## `gobj_log_clear_log_file()`

*Description pending — signature extracted from header.*

```C
void gobj_log_clear_log_file(void);
```

---

(set_trace_with_full_name)=
## `set_trace_with_full_name()`

*Description pending — signature extracted from header.*

```C
BOOL set_trace_with_full_name(
    BOOL trace_with_full_name
);
```

---

(set_trace_with_short_name)=
## `set_trace_with_short_name()`

*Description pending — signature extracted from header.*

```C
BOOL set_trace_with_short_name(
    BOOL trace_with_short_name
);
```

---

