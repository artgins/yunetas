---
title: 'JS: Logging and String Formatting'
description: >-
  The log and trace writers of @yuneta/gobj-js, the remote handler, and
  the printf-style formatting.
---

# Logging and String Formatting

**Source code:** [`src/helpers.js`](https://github.com/artgins/gobj-js/blob/7.13.5/src/helpers.js),
[`src/sprintf.js`](https://github.com/artgins/gobj-js/blob/7.13.5/src/sprintf.js)

Every writer takes a format and its arguments, in the style of `printf`. There
is no `gobj` parameter and no error code, which the C API has. The JS runtime is
simpler.

---

## Write a log

(js_log_error)=
### [`log_error(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/helpers.js#L150)

Writes an error. It goes to the remote handler too.

(js_log_warning)=
### [`log_warning(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/helpers.js#L170)

Writes a warning. It goes to the remote handler too.

(js_log_info)=
### [`log_info(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/helpers.js#L190)

Writes an information message. It stays in the console.

(js_log_debug)=
### [`log_debug(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/helpers.js#L203)

Writes a debug message. It stays in the console.

---

## Write a trace

(js_trace_msg)=
### [`trace_msg(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/helpers.js#L216)

Writes one line of trace.

(js_trace_json)=
### [`trace_json(json, msg)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/helpers.js#L229)

Writes a JSON value.

:::{warning}
A value with a cycle breaks this function. A gobj, a widget and a DOM node all
have cycles, so a `kw` that holds one of them breaks the `machine` trace. Put an
identity in the `kw`, and find the object inside the action.
:::

Turn the levels on and off with the functions in [Traces](traces.md).

---

## Where the logs go

(js_set_remote_log_functions)=
### [`set_remote_log_functions(remote_log_fn)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/helpers.js#L100)

Sends the errors and the warnings to one handler, such as a websocket that
carries them to a log centre. The information and the debug messages stay in the
console.

:::{important}
In every path that takes an application down, call
`set_remote_log_functions(null)` **first**, before the websocket goes and before
the shell goes. A log of the shutdown that goes through a dead socket writes a
log about the failure, which goes through the same dead socket. The recursion
ends in *"too much recursion"*, and it hides the first fault.
:::

(js_set_log_callback)=
### [`set_log_callback(callback)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/helpers.js#L60)

Sends every log to one function of the application.

(js_set_console_log_enabled)=
### [`set_console_log_enabled(enabled)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/helpers.js#L75)

Turns the write to the console on or off.

---

## Format a string

(js_sprintf)=
### [`sprintf(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/sprintf.js#L24)

Builds a string in the style of `printf`.

(js_vsprintf)=
### [`vsprintf(fmt, argv)`](https://github.com/artgins/gobj-js/blob/7.13.5/src/sprintf.js#L29)

The same, and it takes the arguments in an array.

### The conversions

| Conversion | Meaning |
|---|---|
| `%s` | A string. |
| `%d`, `%i` | An integer. |
| `%f`, `%e`, `%g` | A real number. |
| `%o`, `%x`, `%X` | Octal and hexadecimal. |
| `%b` | Binary. |
| `%c` | One character. |
| `%j` | The JSON form. |
| `%t` | A boolean, as `true` or `false`. |
| `%T` | The name of the type. |
| `%v` | The value, with the type found automatically. |
| `%u` | An integer with no sign. |
