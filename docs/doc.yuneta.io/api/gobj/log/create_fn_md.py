#!/usr/bin/python

from string import Template
import os

template = Template("""

<!-- ============================================================== -->
($_name_())=
# `$_name_()`
<!-- ============================================================== -->

$_description_

<!------------------------------------------------------------>
<!--                    Prototypes                          -->
<!------------------------------------------------------------>

``````{tab-set}

`````{tab-item} C

<!--====================================================-->
<!--                    Tab C                           -->
<!--====================================================-->

**Prototype**

```C
$_prototype_

```

**Parameters**

$_parameters_

---

**Return Value**

$_return_value_


<!--====================================================-->
<!--                    End Tab C                       -->
<!--====================================================-->

`````

`````{tab-item} JS

<!--====================================================-->
<!--                    Tab JS                          -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                JS Prototype                    -->
<!---------------------------------------------------->

**Prototype**

````JS
// Not applicable in JS
````

<!--====================================================-->
<!--                    EndTab JS                       -->
<!--====================================================-->

`````

`````{tab-item} Python

<!--====================================================-->
<!--                    Tab Python                      -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                Python Prototype                -->
<!---------------------------------------------------->

**Prototype**

````Python
# Not applicable in Python
````

<!--====================================================-->
<!--                    End Tab Python                   -->
<!--====================================================-->

`````

``````

<!------------------------------------------------------------>
<!--                    Examples                            -->
<!------------------------------------------------------------>

```````{dropdown} Examples

``````{tab-set}

`````{tab-item} C

<!--====================================================-->
<!--                    Tab C                           -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                C examples                      -->
<!---------------------------------------------------->

````C
// TODO C examples
````

<!--====================================================-->
<!--                    End Tab C                       -->
<!--====================================================-->

`````

`````{tab-item} JS

<!--====================================================-->
<!--                    Tab JS                          -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                JS examples                     -->
<!---------------------------------------------------->

````JS
// TODO JS examples
````

<!--====================================================-->
<!--                    EndTab JS                       -->
<!--====================================================-->

`````

`````{tab-item} Python

<!--====================================================-->
<!--                    Tab Python                      -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                Python examples                 -->
<!---------------------------------------------------->

````python
# TODO Python examples
````

<!--====================================================-->
<!--                    End Tab Python                  -->
<!--====================================================-->

`````

``````

```````

""")

functions = [
    {
        "name": "glog_init",
        "description": """
Initialize the global logging system. This function is typically called at the start of the application
to ensure logging functionality is ready to use.
        """,
        "prototype": """
void glog_init(void);
        """,
        "parameters": """
No parameters.
        """,
        "return_value": """
This function does not return a value.
        """
    },
    {
        "name": "glog_end",
        "description": """
Terminate the global logging system. It frees resources associated with logging.
Better avoided since memory usage for logging is minimal, and logs are generally 
kept running for the application lifecycle.
        """,
        "prototype": """
void glog_end(void);
        """,
        "parameters": """
No parameters.
        """,
        "return_value": """
This function does not return a value.
        """
    },
    {
        "name": "gobj_log_register_handler",
        "description": """
Register a new log handler with the logging system.
        """,
        "prototype": """
int gobj_log_register_handler(
    const char *handler_type,
    loghandler_close_fn_t close_fn,
    loghandler_write_fn_t write_fn,
    loghandler_fwrite_fn_t fwrite_fn
);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `handler_type`
  - `const char *`
  - The type of handler being registered (e.g., "stdout").

* - `close_fn`
  - `loghandler_close_fn_t`
  - Function to close the handler.

* - `write_fn`
  - `loghandler_write_fn_t`
  - Function to handle standard log writes.

* - `fwrite_fn`
  - `loghandler_fwrite_fn_t`
  - Function to handle formatted log writes for backtrace and similar features.
:::
        """,
        "return_value": """
- Returns `0` if the handler was registered successfully.
- Returns `-1` on error (e.g., invalid parameters or internal issues).
        """
    },
    {
        "name": "gobj_log_exist_handler",
        "description": """
Check if a specific log handler exists.
        """,
        "prototype": """
BOOL gobj_log_exist_handler(const char *handler_name);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `handler_name`
  - `const char *`
  - The name of the log handler to check for existence.
:::
        """,
        "return_value": """
- Returns `TRUE` if the handler exists.
- Returns `FALSE` if the handler does not exist.
        """
    },
    {
        "name": "gobj_log_add_handler",
        "description": """
Add a new log handler to the logging system.
        """,
        "prototype": """
int gobj_log_add_handler(
    const char *handler_name,
    const char *handler_type,
    log_handler_opt_t handler_options,
    void *h
);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `handler_name`
  - `const char *`
  - Name of the handler being added.

* - `handler_type`
  - `const char *`
  - Type of the handler being added.

* - `handler_options`
  - `log_handler_opt_t`
  - Options for configuring the handler's behavior.

* - `h`
  - `void *`
  - A handle for the handler.
:::
        """,
        "return_value": """
- Returns `0` on success.
- Returns `-1` on failure (e.g., invalid parameters).
        """
    },
    {
        "name": "gobj_log_del_handler",
        "description": """
Delete an existing log handler by its name. If the name is empty, all handlers are deleted.
        """,
        "prototype": """
int gobj_log_del_handler(const char *handler_name);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `handler_name`
  - `const char *`
  - Name of the handler to be deleted. If empty, all handlers are removed.
:::
        """,
        "return_value": """
- Returns `0` on success.
- Returns `-1` on error (e.g., handler not found).
        """
    },
    {
        "name": "gobj_log_list_handlers",
        "description": """
List all registered log handlers and their properties.
        """,
        "prototype": """
json_t *gobj_log_list_handlers(void);
        """,
        "parameters": """
No parameters.
        """,
        "return_value": """
- Returns a JSON object with details about registered handlers.
- Returns `NULL` if no handlers are registered or in case of an error.
        """
    },
    {
        "name": "gobj_log_set_global_handler_option",
        "description": """
Set or reset a global option for all log handlers.
        """,
        "prototype": """
int gobj_log_set_global_handler_option(
    log_handler_opt_t log_handler_opt,
    BOOL set
);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `log_handler_opt`
  - `log_handler_opt_t`
  - The global option to configure.

* - `set`
  - `BOOL`
  - Whether to enable (`TRUE`) or disable (`FALSE`) the option.
:::
        """,
        "return_value": """
- Returns `0` on success.
- Returns `-1` on failure.
        """
    },
    {
        "name": "stdout_write",
        "description": """
Write a log message directly to the stdout handler.
        """,
        "prototype": """
int stdout_write(void *v, int priority, const char *bf, size_t len);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `v`
  - `void *`
  - Context or additional data (typically unused).

* - `priority`
  - `int`
  - Log priority level.

* - `bf`
  - `const char *`
  - Log message buffer.

* - `len`
  - `size_t`
  - Length of the log message.
:::
        """,
        "return_value": """
- Returns `0` on success.
- Returns `-1` on failure.
        """
    },
    {
        "name": "stdout_fwrite",
        "description": """
Write a formatted log message to the stdout handler.
        """,
        "prototype": """
int stdout_fwrite(void *v, int priority, const char *format, ...);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `v`
  - `void *`
  - Context or additional data (typically unused).

* - `priority`
  - `int`
  - Log priority level.

* - `format`
  - `const char *`
  - Log message format string (printf style).

* - `...`
  - ``
  - Additional arguments matching the format string.
:::
        """,
        "return_value": """
- Returns `0` on success.
- Returns `-1` on failure.
        """
    }
]


functions.extend([
    {
        "name": "gobj_log_alert",
        "description": "Logs an alert-level message.",
        "prototype": '''
PUBLIC void gobj_log_alert(hgobj gobj, log_opt_t opt, ...);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The gobj instance initiating the log.

* - `opt`
  - `log_opt_t`
  - Options for log customization, such as stack tracing or exit behavior.

* - `...`
  - `variadic`
  - The message and optional variables for formatting.
:::
        ''',
        "return_value": '''
- **None**
        '''
    },
    {
        "name": "gobj_log_critical",
        "description": "Logs a critical-level message.",
        "prototype": '''
PUBLIC void gobj_log_critical(hgobj gobj, log_opt_t opt, ...);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The gobj instance initiating the log.

* - `opt`
  - `log_opt_t`
  - Options for log customization, such as stack tracing or exit behavior.

* - `...`
  - `variadic`
  - The message and optional variables for formatting.
:::
        ''',
        "return_value": '''
- **None**
        '''
    },
    {
        "name": "gobj_log_error",
        "description": "Logs an error-level message.",
        "prototype": '''
PUBLIC void gobj_log_error(hgobj gobj, log_opt_t opt, ...);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The gobj instance initiating the log.

* - `opt`
  - `log_opt_t`
  - Options for log customization, such as stack tracing or exit behavior.

* - `...`
  - `variadic`
  - The message and optional variables for formatting.
:::
        ''',
        "return_value": '''
- **None**
        '''
    },
    {
        "name": "gobj_log_warning",
        "description": "Logs a warning-level message.",
        "prototype": '''
PUBLIC void gobj_log_warning(hgobj gobj, log_opt_t opt, ...);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The gobj instance initiating the log.

* - `opt`
  - `log_opt_t`
  - Options for log customization, such as stack tracing or exit behavior.

* - `...`
  - `variadic`
  - The message and optional variables for formatting.
:::
        ''',
        "return_value": '''
- **None**
        '''
    },
    {
        "name": "gobj_log_info",
        "description": "Logs an info-level message.",
        "prototype": '''
PUBLIC void gobj_log_info(hgobj gobj, log_opt_t opt, ...);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The gobj instance initiating the log.

* - `opt`
  - `log_opt_t`
  - Options for log customization, such as stack tracing or exit behavior.

* - `...`
  - `variadic`
  - The message and optional variables for formatting.
:::
        ''',
        "return_value": '''
- **None**
        '''
    }
])

functions.extend([
    {
        "name": "gobj_log_set_last_message",
        "description": "Set the last log message for later retrieval.",
        "prototype": """
PUBLIC void gobj_log_set_last_message(const char *msg, ...);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `msg`
  - `const char *`
  - The message to set as the last log message.

* - `...`
  - `variadic`
  - Additional arguments for formatting the message.
:::
        """,
        "return_value": """
- This function does not return a value.
        """
    },
    {
        "name": "set_show_backtrace_fn",
        "description": "Set a function to display the backtrace during error handling.",
        "prototype": """
PUBLIC void set_show_backtrace_fn(show_backtrace_fn_t show_backtrace_fn);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `show_backtrace_fn`
  - `show_backtrace_fn_t`
  - Function pointer to the backtrace display function.
:::
        """,
        "return_value": """
- This function does not return a value.
        """
    },
    {
        "name": "gobj_trace_msg",
        "description": "Log a trace message with formatting.",
        "prototype": """
PUBLIC void gobj_trace_msg(
    hgobj gobj,
    const char *fmt,
    ...
) JANSSON_ATTRS((format(printf, 2, 3)));
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - The GObj instance generating the log.

* - `fmt`
  - `const char *`
  - Format string for the message.

* - `...`
  - `variadic`
  - Additional arguments for formatting the message.
:::
        """,
        "return_value": """
- This function does not return a value.
        """
    }
])

functions.extend([
    {
        "name": "gobj_trace_json",
        "description": "Logs a JSON object with a trace message.",
        "prototype": """
        PUBLIC void gobj_trace_json(
            hgobj gobj,
            json_t *jn,
            const char *fmt,
            ...
        );
        """,
        "parameters": """
        :::{list-table}
        :widths: 10 5 40
        :header-rows: 1
        * - Key
          - Type
          - Description

        * - `gobj`
          - [`hgobj`](hgobj)
          - Handle to the GObj instance.
        * - `jn`
          - `json_t *`
          - JSON object to log.
        * - `fmt`
          - `const char *`
          - Format string for the log message.
        * - `...`
          - `variadic`
          - Additional arguments for formatting the message.
        :::
        """,
        "return_value": "This function does not return a value."
    },
    {
        "name": "print_error",
        "description": "Logs an error message and optionally exits or aborts the program.",
        "prototype": """
        PUBLIC void print_error(
            pe_flag_t quit,
            const char *fmt,
            ...
        );
        """,
        "parameters": """
        :::{list-table}
        :widths: 10 5 40
        :header-rows: 1
        * - Key
          - Type
          - Description

        * - `quit`
          - [`pe_flag_t`](pe_flag_t)
          - Indicates the action to take after logging the error (e.g., continue, exit, or abort).
        * - `fmt`
          - `const char *`
          - Format string for the error message.
        * - `...`
          - `variadic`
          - Additional arguments for formatting the message.
        :::
        """,
        "return_value": "This function does not return a value."
    }
])

functions.extend([
    {
        "name": "gobj_log_debug",
        "description": "Logs a debug-level message with formatting options.",
        "prototype": """
PUBLIC void gobj_log_debug(hgobj gobj, log_opt_t opt, ...);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj instance.

* - `opt`
  - `log_opt_t`
  - Log options (e.g., stack trace or additional metadata).

* - `...`
  - `variadic`
  - Additional arguments for formatting the message.
:::
        """,
        "return_value": "This function does not return a value."
    },
    {
        "name": "gobj_get_log_priority_name",
        "description": "Retrieves the name of a log priority level.",
        "prototype": """
PUBLIC const char *gobj_get_log_priority_name(int priority);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `priority`
  - `int`
  - Numeric value representing the log priority level.
:::
        """,
        "return_value": """
- Returns the name of the log priority as a string.
- Returns `NULL` if the priority level is invalid.
        """
    },
    {
        "name": "gobj_get_log_data",
        "description": "Retrieves internal log data as a JSON object.",
        "prototype": """
PUBLIC json_t *gobj_get_log_data(void);
        """,
        "parameters": "This function does not take any parameters.",
        "return_value": """
- Returns a JSON object containing the internal log data.
- Returns `NULL` on error.
        """
    },
    {
        "name": "gobj_log_clear_counters",
        "description": "Resets all counters associated with logging.",
        "prototype": """
PUBLIC void gobj_log_clear_counters(void);
        """,
        "parameters": "This function does not take any parameters.",
        "return_value": "This function does not return a value."
    },
    {
        "name": "gobj_log_last_message",
        "description": "Retrieves the most recent log message.",
        "prototype": """
PUBLIC const char *gobj_log_last_message(void);
        """,
        "parameters": "This function does not take any parameters.",
        "return_value": """
- Returns the most recent log message as a string.
- Returns `NULL` if no message is available.
        """
    },
    {
        "name": "gobj_info_msg",
        "description": "Logs an informational message with formatting options.",
        "prototype": """
PUBLIC void gobj_info_msg(
    hgobj gobj,
    const char *fmt,
    ...
);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj instance.

* - `fmt`
  - `const char *`
  - Format string for the informational message.

* - `...`
  - `variadic`
  - Additional arguments for formatting.
:::
        """,
        "return_value": "This function does not return a value."
    },
    {
        "name": "gobj_trace_buffer",
        "description": "Logs the content of a buffer for debugging.",
        "prototype": """
PUBLIC void gobj_trace_buffer(
    hgobj gobj,
    int priority,
    const char *bf,
    int len,
    const char *fmt,
    ...
);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj instance.

* - `priority`
  - `int`
  - Priority level for the log message.

* - `bf`
  - `const char *`
  - Pointer to the buffer content.

* - `len`
  - `int`
  - Length of the buffer.

* - `fmt`
  - `const char *`
  - Format string for additional message details.

* - `...`
  - `variadic`
  - Additional arguments for formatting.
:::
        """,
        "return_value": "This function does not return a value."
    },
    {
        "name": "gobj_trace_dump",
        "description": "Logs the content of a buffer in a detailed dump format.",
        "prototype": """
PUBLIC void gobj_trace_dump(
    hgobj       gobj,
    int         priority,
    const char  *bf,
    int         len,
    const char  *fmt,
    ...
);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj instance.

* - `priority`
  - `int`
  - Priority level for the log message.

* - `bf`
  - `const char *`
  - Pointer to the buffer content.

* - `len`
  - `int`
  - Length of the buffer.

* - `fmt`
  - `const char *`
  - Format string for additional message details.

* - `...`
  - `variadic`
  - Additional arguments for formatting.
:::
        """,
        "return_value": "This function does not return a value."
    },
    {
        "name": "print_backtrace",
        "description": "Logs a backtrace to the configured logging output.",
        "prototype": """
PUBLIC void print_backtrace(void);
    """,
        "parameters": """
No parameters.
    """,
        "return_value": "This function does not return a value."
    }
])

# Loop through the list of names and create a file for each
for fn in functions:
    # Substitute the variable in the template

    formatted_text = template.substitute(
        _name_          = fn['name'],
        _description_   = fn['description'],
        _prototype_     = fn['prototype'],
        _parameters_    = fn['parameters'],
        _return_value_  = fn['return_value']
    )
    # Create a unique file name for each name
    file_name = f"{fn['name'].lower()}.md"

    # Check if the file already exists
    if os.path.exists(file_name):
        print(f"File {file_name} already exists. =============================> Skipping...")
        continue  # Skip to the next name

    # Write the formatted text to the file
    with open(file_name, "w") as file:
        file.write(formatted_text)

    print(f"File created: {file_name}")
