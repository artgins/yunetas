# helpers test

Unit tests for the string/utility helpers in `kernel/c/gobj-c/src/helpers.c`.

Currently covers `split2()` / `split_free2()` (split a string by a delimiter
set, dropping empty tokens), including a **reentrancy regression**: `split2()`
must not clobber a caller's in-progress `strtok()` parse — it parses with
`strtok_r` internally.

Also `save_json_to_file()` with a missing directory: it answers `-1` and logs
an error (it was silent).

Also `rmrdir()` / `rmrcontentdir()` with symbolic links: a link to an outside
directory, a link to an outside file and a dangling link are removed as links,
and the files outside the tree stay. And `mkrdir()` over a file (error) and
through a link to a directory (works).

`test_rotatory` covers `rotatory_remove_old_files()`, the retention of the
agent's audit directory: only the old files of the mask (and their `.OLD`) go;
a recent file, other names, a symbolic link, a directory and the current file
stay. It also checks that a size rotation calls the newfile callback, where the
agent applies the retention. And the write path: across a size rotation the
`.OLD` and the new file hold whole records (up to 7.25.4 the `"\n"` of the
record that crossed the limit went to the new file), and a file removed by hand
is created again by the next record. A full disk stops only the handle on it,
and it writes again when the space is back (the free space is faked for one
directory: the test binary is linked with `--wrap=statvfs,--wrap=fstatvfs`).
And after `rotatory_end()`, a write, flush, truncate or close through an old
handle does nothing.

## Run

```bash
ctest -R 'helpers/' --output-on-failure --test-dir build
```
