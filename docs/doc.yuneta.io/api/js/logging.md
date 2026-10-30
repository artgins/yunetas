---
title: 'JS: Logging and String Formatting'
description: >-
  The log and trace writers of @yuneta/gobj-js, the remote handler, and
  the printf-style formatting.
---

# Logging and String Formatting

**Source code:** [`src/helpers.js`](https://github.com/artgins/gobj-js/blob/7.29.2/src/helpers.js),
[`src/sprintf.js`](https://github.com/artgins/gobj-js/blob/7.29.2/src/sprintf.js)

Every writer takes a format and its arguments, in the style of `printf`. There
is no `gobj` parameter and no error code, which the C API has. The JS runtime is
simpler.

```javascript
log_error("Command not found: %s", command);    // "Command not found: open"
log_error(`Command not found: ${command}`);     // the same line
log_warning("%d devices, %s", 3, "online");     // "3 devices, online"
```

A message without arguments is written as it is: a `%s` in its data is not
touched. Arguments left over are appended, a json one as its JSON. Up to gobj-js
7.25.9 the writers dropped their arguments, and the first line above printed
`Command not found: undefined`.

---

## Write a log

(js_log_error)=
### [`log_error(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/helpers.js#L341)

Writes an error. It goes to the remote handler too.

(js_log_warning)=
### [`log_warning(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/helpers.js#L361)

Writes a warning. It goes to the remote handler too.

(js_log_info)=
### [`log_info(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/helpers.js#L381)

Writes an information message. It stays in the console.

(js_log_debug)=
### [`log_debug(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/helpers.js#L394)

Writes a debug message. It stays in the console.

### The format of a log line

A message given alone is not a format: a `%s` in its text stays as it is. With
arguments, the log functions read these conversions; extra arguments go at the
end, separated by a space. No value makes a log line throw (since gobj-js
7.26.17): what has no string form is printed as json, or as `<type: not
printable>`, and a log function that throws (a remote one over a socket that
is gone) does not take the caller with it. The REMOTE one, if it logs in
turn, is not called again from inside itself (since 7.26.18; the console
functions are not held back by it since 7.26.19); that stops a synchronous
loop only, so a teardown unwires it first (`set_remote_log_functions(null)`).

| Conversion | Prints |
|---|---|
| `%s` | The value as text. |
| `%d`, `%i` | An integer. A BigInt keeps all its digits. |
| `%f` | A real number. |
| `%o`, `%O` | An object as JSON; any other value as text (`"abc"` prints `abc`). |
| `%j` | Any value as JSON (`"abc"` prints `"abc"`; a BigInt, its digits as a string). |
| `%%` | A `%`. It takes no argument. |

```js
log_error("node %s not found", id);              // node 42 not found
log_error("100%% of %d nodes", 3);               // 100% of 3 nodes
log_error("record %j", {id: "a"});               // record {"id":"a"}
log_error("name %o, json %j", "abc", "abc");     // name abc, json "abc"
log_error("rowid %d", 12345678901234567890n);    // rowid 12345678901234567890
log_error(`already composed: 50%s`);             // already composed: 50%s
```

---

## Write a trace

(js_trace_msg)=
### [`trace_msg(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/helpers.js#L407)

Writes one line of trace.

(js_trace_json)=
### [`trace_json(json, msg)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/helpers.js#L420)

Writes a JSON value.

:::{warning}
A value with a cycle breaks this function. A gobj, a widget and a DOM node all
have cycles, so a `kw` that holds one of them breaks the `machine` trace. Put an
identity in the `kw`, and find the object inside the action.
:::

Turn the levels on and off with the functions in [Traces](traces.md).

(js_trace_json_masked)=
### [`trace_json_masked(json, msg)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/helpers.js#L2042)

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
### [`json_mask_secrets(json)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/helpers.js#L2029)

A JSON value as a log or a trace may show it, with the rule and the lists of
the C kernel's `json_mask_secrets()`. At any depth, the value of a key whose
name is a secret's ([`is_secret_name()`](#js_is_secret_name)) is
`"********"`, whatever its type (not `undefined`, `null` or `""`); so is the
`value` of an object whose `attribute` names a secret; and every string, under
ANY key, is masked as [`mask_secrets_inline()`](#js_mask_secrets_inline): a
JWT, the credential after `Bearer` or `Basic`, a secret `name=value` or
`name: value` in it. A string with a NUL
inside is read as a blank, as C reads it (shown with blanks when something is
masked). Up to gobj-js 7.26.19 a JWT under a plain key showed. A key is judged
as a NAME, never as a value: a key that is itself a credential (an object
keyed by tokens) is shown as it is, as in C. It answers a masked
copy (the objects and arrays on the way to a masked value are copied, the
rest is shared), or the value itself when there was nothing to mask; the
value is never changed.

Only JSON is walked: plain objects and arrays. A gobj, a DOM node, a class
instance, a typed array or a function is passed as it is. An object met twice
is masked once, the same everywhere; a cycle back to one is `"<cycle>"`; an
object or array reached at 64 levels (as in C) is `"<deeper not shown>"` --
counted from the FIRST time it is met: one masked higher up shows whole
wherever else it appears; the walk has a budget of 4 MB units (every node
one, a key or a string its characters too), and once it is spent it stops: one
`"<not shown: too large to mask>"` stands for the rest of each object or
array being walked (it is linear and bounded, as in C, a string counted in
characters);
and a failure answers
`"<not shown: the masking failed>"`: it never throws.

```js
json_mask_secrets({password: 1234, auth: {access_token: "eyJ..."}, window: gobj});
// {password: "********", auth: {access_token: "********"}, window: gobj}
json_mask_secrets({note: "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJ4In0.c2ln", h: "Authorization: Bearer abc"});
// {note: "********", h: "Authorization: ********"}
json_mask_secrets({"eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJ4In0.c2ln": 1});
// the same: the key as it is
```

(js_mask_secrets_inline)=
### [`mask_secrets_inline(text)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/helpers.js#L1854)

A text (a command line) with the value of every `name=value` and every
`name: value` (its name quoted or not: `"password": x`, `bearer: tok`) whose
name is a secret's written as `********`, quoted or not, and the `value=` of a
write-attr whose `attribute=` names a secret. An unquoted value of a
`name=value` runs to the next `word=`: `password=correct horse battery` is
masked whole; a value that is a scheme and its credential is masked whole
(`token=Bearer abc`, `Authorization: Basic x`). And a credential by its
SHAPE, wherever it is and whatever names it, by the rules of the C kernel's
[`mask_secrets_inline()`](#mask_secrets_inline): a JWT or a JWE (`eyJ`,
base64url and a `.`: also one a cap cut; a `.` after it, the end of a
sentence, is not its own), and after `Bearer` or `Basic` (any case, a word of
its own) the word that can be a credential -- quoted, after the scheme
doubled, or not a word of prose (it holds a digit, a symbol of its own or a
character that is no ASCII, mixes its case, or has 24 characters or more; for
`Bearer` 8 at least, for `Basic` a run of base64 that decodes to a
`user:password`) --, the word to a blank, a quote or one of `,;}]`, masked
whole. The name of a `name=value` is also the run of name characters before
the `=`, as in a query string (gobj-js 7.26.22+):
`GET https://my-file-server.com/get?token=abc&x=1` is
`GET https://my-file-server.com/get?token=********&x=1`; and the name as a
form writes it, a part of it in brackets or quotes, its `%XX` decoded
(`user[password]=x`, `user%5Bpassword%5D=x`, `t%6Fken=x`; gobj-js 7.26.24+).
In the word before the `=`, `pass`, `pin` and `otp` count only after its last
`&`, `?` or `;` (gobj-js 7.26.25+; 7.26.22 to 7.26.24 gave
`GET /search?q=boarding+pass&lang=********`). A URL or a query `%`-encoded
inside a value is read decoded too, twice at most (gobj-js 7.26.25+):
`GET /cb?next=%2Flogin%3Fpassword%3Dhunter2` is
`GET /cb?next=%2Flogin%3Fpassword%3D********`, and
`url=https%3A%2F%2Fbob%3Ahunter2%40host` is
`url=https%3A%2F%2Fbob%3A********%40host`.
The password of the userinfo of a URL is masked too
(`https://bob:hunter2@host` is `https://bob:********@host`, 7.26.24+). A JWT
header whose only dots end it is no JWT (`eyJhIjoxfQ.`, 7.26.22+). Each rule judges the
text as it came, and a run of characters to mask is one `********`; the
readings of the parameters of gobj-js 7.26.19 and 7.26.20 are rules of their
own, whole: measured position by position on 438 008 generated texts, nothing
that 7.26.19 masked shows but where a name is judged as C judges it (its last
128 bytes, its case in ASCII), and nothing that 7.26.20 masked shows but the
word after a bare `Bearer` that cannot be a credential and, since 7.26.25, the
value of a parameter whose word holds `pass`, `pin` or `otp` only before its
last `&`, `?` or `;`. The rest is kept. It
answers `null` when there was nothing to mask. The C kernel and gobj-js
answer the same on the tables of both test suites, on 438 008 generated
texts and on 30 000 generated jsons (since gobj-js 7.26.21: up to 7.26.20 the
word after `bearer` was masked whatever it was, and `Basic`, a JWE, a cut JWT
and a `name: value` were not).

```js
mask_secrets_inline("set-user-pwd username=bob password=hunter2");
// "set-user-pwd username=bob password=********"
mask_secrets_inline("curl -H 'Authorization: Bearer abc' u");
// "curl -H 'Authorization: ********' u"
mask_secrets_inline("sent Bearer mF_9.B5f-4.1JqM");   // "sent Bearer ********"
mask_secrets_inline("the bearer of this letter");    // null: prose
mask_secrets_inline("note=eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJ4In0.c2ln x=1");
// "note=******** x=1"
mask_secrets_inline("x-auth-token: password=a,b c");    // "x-auth-token: ********"
mask_secrets_inline("GET https://my-file-server.com/get?token=abc&x=1");
// "GET https://my-file-server.com/get?token=********&x=1"
mask_secrets_inline("GET https://my-file-server.com/get?user%5Bpassword%5D=abc");
// "GET https://my-file-server.com/get?user%5Bpassword%5D=********"
mask_secrets_inline("clone https://bob:hunter2@git.example/x.git");
// "clone https://bob:********@git.example/x.git"
mask_secrets_inline("GET /cb?next=%2Flogin%3Fpassword%3Dhunter2");
// "GET /cb?next=%2Flogin%3Fpassword%3D********"
mask_secrets_inline("GET /search?q=boarding+pass&lang=es");    // null
mask_secrets_inline("the header eyJhIjoxfQ.");      // null: its only dot ends it
mask_secrets_inline("list-yunos");      // null
```

(js_is_secret_name)=
### [`is_secret_name(name)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/helpers.js#L627)

`true` if `name` is the name of a secret, by the C kernel's lists: it holds
`passw`, `pwd`, `passphrase`, `secret`, `token`, `jwt`, `bearer`,
`authorization`, `cookie`, `credential` or `salt`; or `apikey`, `sessionid`,
`sessionkey` or `authdata` once `_`, `-`, `.` and blanks are taken out; or
`pass`, `pin` or `otp` as a WHOLE segment, a run of letters and digits
(`db_pass`, `user-pin`, `sim_pin`; not `passive`, `ping`, `otpx`); or both
`priv` and `key`. Any case, folded in ASCII as C folds it (gobj-js 7.26.22+:
the Kelvin sign is no `k`), and judged by its last 128 bytes of UTF-8, as C
judges it, where a name is cut. A name with a segment that names something ABOUT a
credential (`endpoint`, `url`, `domain`, `path`, `file`, `public`, `count`,
`type`, `name`, ... the C list) is not one: `token_endpoint`, `cookie_domain`,
`jwt_public_keys`, `pin_count`. `key` and `cert` alone are not secrets' names:
a record key, a public certificate. Up to gobj-js 7.26.19 `pass`, `pin` and
`otp` were not in it. As in C, an ordinary name with one of those segments is
a secret's too (`gpio_pin`, `first_pass`): accepted, the safe side.

```js
is_secret_name("smtp_password");    // true
is_secret_name("X-Api-Key");        // true
is_secret_name("sim_pin");          // true
is_secret_name("mapping");          // false
is_secret_name("username");         // false
is_secret_name("token_endpoint");   // false
```

---

## Where the logs go

(js_set_remote_log_functions)=
### [`set_remote_log_functions(remote_log_fn)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/helpers.js#L187)

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
### [`set_log_callback(callback)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/helpers.js#L82)

Sends every log to one function of the application.

(js_set_console_log_enabled)=
### [`set_console_log_enabled(enabled)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/helpers.js#L115)

Turns the write to the console on or off.

(js_set_console_log_filter)=
### [`set_console_log_filter(fn)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/helpers.js#L136)

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
### [`sprintf(format, ...args)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/sprintf.js#L49)

Builds a string in the style of `printf`.

(js_vsprintf)=
### [`vsprintf(fmt, argv)`](https://github.com/artgins/gobj-js/blob/7.29.2/src/sprintf.js#L54)

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
