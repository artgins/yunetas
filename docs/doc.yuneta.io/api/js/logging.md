# Logging & String Formatting

## Log functions

All log functions take a single `format` argument and use `printf`-style
substitution (`%s`, `%d`, `%i`, `%f`, `%o`/`%O`). There is **no** `gobj`
or error-code argument — that's the C API. The JS runtime is simpler.

```javascript
log_error   (format, ...args)   // red,   prefixed "ERROR"
log_warning (format, ...args)   // yellow, prefixed "WARNING"
log_info    (format, ...args)   // cyan,  prefixed "INFO"
log_debug   (format, ...args)   // silver, prefixed "DEBUG"

trace_msg   (format, ...args)   // cyan,  prefixed "MSG"
trace_json  (json, msg)         // dir-dump a JSON object
```

## Redirecting error / warning output

By default all logs go to the browser console. You can route `error`
and `warning` level messages to a single remote handler (info and debug
always stay in the console):

```javascript
set_remote_log_functions(remote_log_fn)   // fn(message)
```

## String formatting

```javascript
sprintf(format, ...args)   // printf-style
vsprintf(fmt, argv)        // variadic version
```

### Format specifiers

| Spec | Meaning |
|---|---|
| `%s` | string |
| `%d`, `%i` | integer |
| `%f`, `%e`, `%g` | real |
| `%o`, `%x`, `%X` | octal / hex |
| `%b` | binary |
| `%c` | character |
| `%j` | JSON-stringify |
| `%t` | boolean (`true`/`false`) |
| `%T` | type name |
| `%v` | value (auto-detected) |
| `%u` | unsigned integer |
