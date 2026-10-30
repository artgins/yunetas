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
| `DTP_INTEGER` / `DTP_REAL` / `DTP_BOOLEAN` | a number or a boolean, handed on as the EXACT type | a number of the JSON grammar with nothing before or after it, in its range (a JSON integer `-?(0\|[1-9][0-9]*)` for an integer or a boolean, a JSON number for a real: `2.0`, `0x10`, `.5`, `5.`, `+5`, `007` refused; an underflow is a number), read in the "C" locale; a boolean as true/false, yes/no, on/off or a JSON integer |
| `DTP_JSON` | anything | its json, or the text as a string; a text that starts as json and breaks is refused |

The exact type: a number given to a `DTP_BOOLEAN` is `true` unless 0, a
boolean given to a `DTP_INTEGER` or a `DTP_REAL` is 1 or 0, a real given to a
`DTP_INTEGER` is that integer -- and one with decimals (`2.7`), or past 64
bits, is refused, as its text is --, an integer given to a `DTP_REAL` is a
real. As it came, a `1` reached a `kw_get_bool()` that logged an ERROR.

A `null` is no value (but for a `DTP_JSON`, which keeps it), and so is an
empty text given in kw to a number: the parameter takes its default, or is
ABSENT from the handler's kw (the `""` used to reach it), and a
`SDF_REQUIRED` one answers that it is required. A value of kw that is refused
is overridden by the same parameter on the line, written in any case: the
handler gets the line's value under the parameter's DECLARED name (`N=5` is
`n`; up to 7.26.9 it arrived as `"N"`, and the handler took its default). A
text of kw with a NUL inside is refused, but for a `DTP_JSON`.

The cases, one each: a string given a number (by C, an ERROR; by a peer, a
WARNING); a dict given a list (in kw and on the line) and given its json as
text; a list given an object; an integer given an object, its text, other
text, an empty text on the line (`0`) and an empty text in kw (the default);
a real given its text and other text; a boolean given `yes`, `off`, `1` and
`maybe`; a `DTP_JSON` given a bare word (`j=UPDATE_PASSWORD`, a string), its
json as text, a number, and json written wrong (`j={"a":1`, refused); a
`null` for a string (the default) and for a boolean with no default (absent);
a required boolean given its text, a required one given `null`, a required
string given a list. The exact type: a boolean given `1`, `0` and `2.5`, an
integer given `true`, `2.0`, `2.7` (refused) and `1e30` (refused, from a
peer), a real given `3` and `true`. Text: an integer past 64 bits (in kw and
on the line), with a blank before and after, a real `nan`, `inf` and `1e999`
(all refused). An empty real and boolean in kw with no default (absent), and
a refused `s` the line gives as `S` (the line's value, under `s`). The
declared name: `N=5` reaches the handler as `n`, and beats a kw `n`. A NUL
inside a text of kw: refused for an integer, a boolean (from a peer) and a
string, kept as it came by a `DTP_JSON`. The text reader: a real `0x10` and
`0x1p3` refused, `1e-400` is `0.0`, an integer `2.0` and a boolean `1.0`
refused. The JSON grammar: a real `.5`, `5.`, `+5`,
`007` and `1e`, an integer `+5` and `007` (from a peer), a boolean `007` and
`+1`, all refused (`strtod()` and `strtoll()` took them); an integer `-0`, a
boolean `-1`, a real `1E5`, `-0.5e-1` and `0`, taken. And the locale: with the
`LC_NUMERIC` of `es_ES` (skipped when that locale is not installed), a real
`2.5` is `2.5` and `2,5` is refused (`strtod()` read `2` and took `2,5`).
`command_parameter_from_text()`: `yes`, `12abc` (refused, and why), `-3.5`,
an empty integer (0), a real `0x10` (refused), a json `word` (the text), a
dict `[1]` (refused), a real `.5` and an integer `007` (refused: no JSON
number), a real `1e999` (out of its range). And no leak.

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
