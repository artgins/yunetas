---
title: 'JS: Logging and String Formatting'
description: >-
  The log and trace writers of @yuneta/gobj-js, the remote handler, and
  the printf-style formatting.
---

# Logging and String Formatting

**Source code:** [`src/helpers.js`](https://github.com/artgins/gobj-js/blob/7.25.8/src/helpers.js),
[`src/sprintf.js`](https://github.com/artgins/gobj-js/blob/7.25.8/src/sprintf.js)

Every writer takes a format and its arguments, in the style of `printf`. There
is no `gobj` parameter and no error code, which the C API has. The JS runtime is
simpler.

---

## Write a log

(js_log_error)=
### [`log_error(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/helpers.js#L240)

Writes an error. It goes to the remote handler too.

(js_log_warning)=
### [`log_warning(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/helpers.js#L260)

Writes a warning. It goes to the remote handler too.

(js_log_info)=
### [`log_info(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/helpers.js#L280)

Writes an information message. It stays in the console.

(js_log_debug)=
### [`log_debug(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/helpers.js#L293)

Writes a debug message. It stays in the console.

---

## Write a trace

(js_trace_msg)=
### [`trace_msg(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/helpers.js#L306)

Writes one line of trace.

(js_trace_json)=
### [`trace_json(json, msg)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/helpers.js#L319)

Writes a JSON value.

:::{warning}
A value with a cycle breaks this function. A gobj, a widget and a DOM node all
have cycles, so a `kw` that holds one of them breaks the `machine` trace. Put an
identity in the `kw`, and find the object inside the action.
:::

Turn the levels on and off with the functions in [Traces](traces.md).

(js_trace_json_masked)=
### [`trace_json_masked(json, msg)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/helpers.js#L563)

[`trace_json()`](#js_trace_json) of a value that can hold a credential: it
writes [`json_mask_secrets(json)`](#js_json_mask_secrets). The framework
uses it since 7.25.9 for the `commands` trace, the `machine` trace with
`ev_kw`, the kw a `kw_get_*()` error dumps and the ievents trace of
`C_IEVENT_CLI`. It never throws: a gobj, a widget or a DOM node in the value
is written as it is, not walked.

```js
trace_json_masked({username: "bob", password: "hunter2"}, "login");
// {username: "bob", password: "********"}
```

(js_json_mask_secrets)=
### [`json_mask_secrets(json)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/helpers.js#L550)

A JSON value as a log or a trace may show it, with the rule and the lists of
the C kernel's `json_mask_secrets()`. At any depth, the value of a key whose
name is a secret's ([`is_secret_name()`](#js_is_secret_name)) is
`"********"`, whatever its type (not `undefined`, `null` or `""`); so is the
`value` of an object whose `attribute` names a secret; and a string is masked
as [`mask_secrets_inline()`](#js_mask_secrets_inline). It answers a masked
copy (the objects and arrays on the way to a masked value are copied, the
rest is shared), or the value itself when there was nothing to mask; the
value is never changed.

Only JSON is walked: plain objects and arrays. A gobj, a DOM node, a class
instance, a typed array or a function is passed as it is. An object met twice
is masked once, the same everywhere; a cycle back to one is `"<cycle>"`; a
nesting deeper than 64 levels (as in C) is shown as `"<deeper not shown>"`;
and a failure answers
`"<not shown: the masking failed>"`: it never throws.

```js
json_mask_secrets({password: 1234, auth: {access_token: "eyJ..."}, window: gobj});
// {password: "********", auth: {access_token: "********"}, window: gobj}
```

(js_mask_secrets_inline)=
### [`mask_secrets_inline(text)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/helpers.js#L393)

A text (a command line) with the value of every `name=value` whose name is a
secret's written as `********`, quoted or not, and the `value=` of a
write-attr whose `attribute=` names a secret. The rest is kept. It answers
`null` when there was nothing to mask.

```js
mask_secrets_inline("set-user-pwd username=bob password=hunter2");
// "set-user-pwd username=bob password=********"
mask_secrets_inline("list-yunos");      // null
```

(js_is_secret_name)=
### [`is_secret_name(name)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/helpers.js#L361)

`true` if `name` is the name of a secret, by the C kernel's lists: it holds
`passw`, `pwd`, `passphrase`, `secret`, `token`, `jwt`, `bearer`,
`authorization`, `cookie`, `credential` or `salt`; or `apikey`, `sessionid`,
`sessionkey` or `authdata` once `_`, `-`, `.` and blanks are taken out; or
both `priv` and `key`. Any case. A name with a segment that names something
ABOUT a credential (`endpoint`, `url`, `domain`, `path`, `file`, `public`,
`count`, `type`, `name`, ... the C list) is not one: `token_endpoint`,
`cookie_domain`, `jwt_public_keys`.

```js
is_secret_name("smtp_password");    // true
is_secret_name("X-Api-Key");        // true
is_secret_name("username");         // false
is_secret_name("token_endpoint");   // false
```

---

## Where the logs go

(js_set_remote_log_functions)=
### [`set_remote_log_functions(remote_log_fn)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/helpers.js#L190)

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
### [`set_log_callback(callback)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/helpers.js#L85)

Sends every log to one function of the application.

(js_set_console_log_enabled)=
### [`set_console_log_enabled(enabled)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/helpers.js#L118)

Turns the write to the console on or off.

(js_set_console_log_filter)=
### [`set_console_log_filter(fn)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/helpers.js#L139)

Gives one function the say over each console line, on top of the switch above.
`fn(level, msg)` gives `true` to write the line. `null` writes them all, which
is the default.

The console write occurs before the log callback runs, so nothing after it can
remove a line. This is the only place to keep a class of lines off the console
— for example, the timer traffic of the `machine` trace.

The filter decides the console only. The callback still gets every line. A
filter that throws is ignored: a broken filter must not silence the log.

---

## Format a string

(js_sprintf)=
### [`sprintf(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/sprintf.js#L24)

Builds a string in the style of `printf`.

(js_vsprintf)=
### [`vsprintf(fmt, argv)`](https://github.com/artgins/gobj-js/blob/7.25.8/src/sprintf.js#L29)

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
