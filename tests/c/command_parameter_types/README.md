# command_parameter_types test

What a command parameter takes, whatever gives it: the command line (text),
the caller's kw (json) or its declared default (text). A value of another
shape is refused by `command_parser()` before the handler runs, with the
answer `-1` that names the parameter: an ERROR with a stack when C code calls
the command, a WARNING when a peer does (its kw carries `__username__`).

| Type | Takes | Text is read as |
|---|---|---|
| `DTP_STRING` | a string | the text |
| `DTP_DICT` / `DTP_LIST` | a json object / array | its json |
| `DTP_INTEGER` / `DTP_REAL` / `DTP_BOOLEAN` | a number or a boolean | a number with nothing after it; a boolean as true/false, yes/no, on/off or a number |
| `DTP_JSON` | anything | its json, or the text as a string; a text that starts as json and breaks is refused |

A `null` is no value (but for a `DTP_JSON`, which keeps it): the parameter
takes its default, or is missing, and a `SDF_REQUIRED` one answers that it is
required. A value of kw that is refused is overridden by the same parameter
on the line.

The cases, one each: a string given a number (by C, an ERROR; by a peer, a
WARNING); a dict given a list (in kw and on the line) and given its json as
text; a list given an object; an integer given an object, its text, other
text, an empty text on the line (`0`) and an empty text in kw (the default);
a real given its text and other text; a boolean given `yes`, `off`, `1` and
`maybe`; a `DTP_JSON` given a bare word (`j=UPDATE_PASSWORD`, a string), its
json as text, a number, and json written wrong (`j={"a":1`, refused); a
`null` for a string (the default) and for a boolean with no default (absent);
a required boolean given its text, a required one given `null`, a required
string given a list. And no leak.

```C
/*  a peer's kw: one WARNING, -1, and the handler never runs  */
json_t *resp = gobj_command(yuno, "typed",
    json_pack("{s:i, s:s}", "s", 5, "__username__", "peer@test"), yuno);
/*  "<gobj>: command 'typed', parameter 's' must be a string, not an integer"  */
```

Up to 7.26.9 a value of another shape went to the handler as it came: its
`kw_get_*()` logged the caller's mistake as an ERROR with a stack, or read it
as something else (`"treedb_name": 5` saved every treedb in C_TREEDB's
`save-schema`, `dry_run=yes` was `atoi()`'d to FALSE).

Run it:

```bash
ctest -R test_command_parameter_types --output-on-failure --test-dir build
```
