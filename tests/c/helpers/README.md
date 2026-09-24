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
and the files outside the tree stay. An entry (a file, a directory) that
another process removes during the walk is not an error (the test binary is
linked with `--wrap=lstat` and removes the entry at its `lstat()`). And
`mkrdir()` over a file (error) and through a link to a directory (works).
And a deep tree: 300 levels are removed; paths longer than `PATH_MAX` and a
tree of 1100 levels are refused with a log, not walked into (the code before
crashed); `mkrdir()` of a path longer than `PATH_MAX` is refused with a log.

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
handle does nothing. And a clock set back across midnight, and forward again,
empties no file (the audit mask and the `W` mask; the file of last week of a
`W` mask is still emptied): the clock is faked with `--wrap=time`.
And a piece of 0 bytes stops nothing, a write that fails (a file size limit)
is followed by a record that opens the file again, the open of an old `W`
file empties it (a mask with the year never), and a new day on a full disk
calls the newfile callback (the retention) and writes again when it frees the
space.

`test_audit_record` compiles the audit record builder of `yuneta_agent`
(`yunos/c/yuno_agent/src/audit_record.c`) and checks it: a `content64` (in
the command text or as a kw key) is never written, only its size and the
sha256 of the decoded content; `__md_iev__` becomes a short `source`; a
read-only command is recorded with command, date and user only; the kw of the
caller is not modified. A secret (`password`, `pwd`, `secret`, `token`, `jwt`,
`private_key`, … in the text, in the kw at any depth, in the command carried by
`command-yuno`, in a json given as text, the `value` of a `write-attr` of a
secret attribute) is `<redacted>`. A `stats=__reset__` makes a stats command a
write. The carried command of `command-yuno` is the one of the text, as the
parser takes it. Blanks around the `=` of `content64`, many short values and a
quote that never ends leak nothing. Bad data from a peer logs no error. A
console write (`write-tty`) keeps only the fact, one record per burst, with no
hash. Texts made to be hard to scan (`list-yunos x` + 1 500 000 `=`, and
eleven other shapes) and 3000 random texts: every record is built, in linear
time, and a `password=` never survives; a text beyond the cap of a record
(128 MB) is written as its first word, size and sha256. The command word is
the one the parser takes (`WRITE-TTY`, `'write-tty'`, `EV_WRITE_TTY`,
`CLOSE-CONSOLE`, `1`, `ATTRIBUTE=… VALUE=…`), checked against
`command_get_cmd_desc()`. More secret names (`api_key`, `x-api-key`,
`http_cookie`, `__session_id__`, `auth_data`, `passphrase`, …), json keys with
`\u` escapes, `Bearer` tokens and JWTs. It prints the size of a record,
7.25.4's way and now, and what one record costs. The command carried by
`command-yuno` is the one its handler reads: the key exactly `command`
(`COMMAND=list-yunos` with a kw `command=delete-yuno` gets the full record, and
`NAME=decoy` does not move a `write-tty` to another console). `test_rotatory`
also covers `rotatory_keep_all_old_files()` (numbered `.OLD.<n>` pieces, none
removed, and the retention matches them), a newfile callback that runs for a
new file only (never for the same file opened again after a failed write or a
removal), a keep_all rename that fails (tried once, not at every record; the
test is linked with `--wrap=rename`), and the open of a fixed name, of a `MM`
name within its month, and of a `W` file with keep_all set right after the
open (none of them emptied).

## Run

```bash
ctest -R 'helpers/' --output-on-failure --test-dir build
```

`test_dir_array_nomem` checks a directory listing that cannot keep an entry
(no memory: the largest block is set to 4 KB, under the first array of
entries): `find_files_with_suffix_array()`, `walk_dir_array()` and
`get_ordered_filename_array()` answer `-1` with the listing empty, and log it.
Up to 7.25.4 the entry was dropped and the listing answered `0`, so
timeranger2 read a key without the md2 file the listing lost.
