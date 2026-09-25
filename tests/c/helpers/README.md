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
And a walk whose ROOT cannot be opened (mode 0) answers `-1` too; up to 7.25.4
`walk_dir_array()` answered `0` with an empty listing.

`test_dir_listing` compiles the answer of the agent's `dir-*` commands
(`yunos/c/yuno_agent/src/dir_listing.c`) and checks it: a tree that can be
listed answers `0` with its entries sorted; a directory of mode 0 (SKIPPED as
root), one that does not exist, and a `match` that is not a regular expression
answer `-1` with a comment that names the directory, and no list. Up to 7.25.4
each command answered an empty list with `0`. The comment sends to the log for
the cause (*"see the log"*): it does not read the process-global
`gobj_log_last_message()`.

`test_dir_read_error` fails the `readdir()` of one directory with `EIO`
(`--wrap=opendir,--wrap=readdir`): `find_files_with_suffix_array()`,
`walk_dir_array()` (its root, or a subdirectory) and `walk_dir_tree()` answer
`-1`, empty, logged. Up to 7.25.4 the failure was taken as the end of the
directory, and a SHORT listing answered `0`. It also checks `re` / `pattern`
`NULL` (every entry; up to 7.25.4 a crash in `regcomp()`), and that
`walk_dir_tree()` of a root of mode 0 logs its `-1` (SKIPPED as root).
It also checks the rules of the walks (`--wrap=opendir` fails the `opendir()`
of one directory, and the `readdir()` wrap can hide `d_type`): a SUBdirectory
that cannot be opened for a transient cause (`EMFILE`) fails the walk, `-1`,
empty (up to 7.25.4 it was skipped and the listing answered `0`, short); one
with `EACCES` is skipped with a warning; a callback that returns `FALSE` in a
subdirectory stops the WHOLE walk (up to 7.25.4 only that directory); paths
longer than `PATH_MAX` fail `walk_dir_tree()` / `walk_dir_array()` with
*"Path too long"* (up to 7.25.4 the walk went into the same directory again,
forever: a crash; run last), and `find_files_with_suffix_array()` without
`d_type` (up to 7.25.4 the directory was stat'ed instead of the file, and the
file dropped); a tree of 1100 levels fails the walk (*"Tree too deep"*).
And with `--wrap=lstat`: an entry whose `lstat()` fails with `EIO` fails the
walk and `find_files_with_suffix_array()` without `d_type`, `-1`, logged; a
subdirectory whose `opendir()` fails with `ENOTDIR` or `ELOOP` is skipped with
a warning, as with `EACCES`. And `rmrcontentdir()` / `rmrdir()` of a directory
whose `readdir()` fails answer `-1` and log *"readdir() FAILED"*, with what
was not read left in place (up to this fix `rmrcontentdir()` answered `0` with
nothing removed and nothing logged, and `rmrdir()` blamed the `rmdir()`).
