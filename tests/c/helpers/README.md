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

`test_audit_record` also builds a record with the gbmem allocators failing
(swapped with `gbmem_set_allocators()`): a peer field or a kw string whose
redaction cannot be allocated is written as `<not written: no memory to
redact it>`, never as it came (up to 7.25.20 it was written raw, and one over
the cap had never been scanned). A peer field that is not UTF-8 is a warning,
not an error, written as `""`.

`test_watch_request` compiles what the agent takes from a `watch-yuno-stats`
request (`yunos/c/yuno_agent/src/watch_request.c`): the watch is named from
the hop the agent can trust (its own input channel's when direct, the one the
control center stamped, with `cc_connection`, when relayed), so a forged
extra hop names no other client's watch; a requester without `__relays__`
naming `EV_YUNO_STATS` is refused, a direct one too; more ids than the cap
are refused, and a cap under 1 (a bad `max_watch_ids`) refuses every watch
naming the cap, logged.

`test_yuno_config_file` compiles the writer of the configuration files the
agent materialises for a yuno (`yunos/c/yuno_agent/src/yuno_config_file.c`):
a new file is `0640`, and one that existed as `0664` is narrowed to `0640`
(up to 7.25.20 they were `0664`, with the yuno's secrets in them). The file
is written to a temporary file and renamed over the old one: a symbolic link
is replaced, not followed; a file whose mode the agent cannot change (another
owner, written through the group; a link to `/dev/null` in the test) is
replaced and the yuno runs; a write that fails (`RLIMIT_FSIZE`) leaves the old
file whole and no temporary file behind. The files
of an earlier launch that wrote more of them (`4-role^name.json` when three
are written now) are narrowed to `0640`: never widened, never removed, and a
symbolic link, its target and the files of another yuno are not touched; a
yuno name too long to build their names from is refused with a log. The
temporary files a write left when the agent died before its rename
(`.<n>-role^name.json.XXXXXX`) are removed; a link with that name, another
yuno's temporary file, or a name that only looks like one, is kept.

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
walk and `find_files_with_suffix_array()` without `d_type`, `-1`, logged;
one whose `lstat()` fails with `EACCES` is LISTED by
`find_files_with_suffix_array()` without `d_type`, as with it (up to
7.25.4 it was skipped with no log); a
subdirectory whose `opendir()` fails with `ENOTDIR` or `ELOOP` is skipped with
a warning, as with `EACCES`. And `rmrcontentdir()` / `rmrdir()` of a directory
whose `readdir()` fails answer `-1` and log *"readdir() FAILED"*, with what
was not read left in place (up to this fix `rmrcontentdir()` answered `0` with
nothing removed and nothing logged, and `rmrdir()` blamed the `rmdir()`).
And a failed `mkrdir()` leaves the cause in `errno` after its own log
(`ENOTDIR` under a file, `ENAMETOOLONG`): up to this fix the log changed it.

`test_switchs` covers the string switch of `helpers.h` (`SWITCHS` / `CASES` /
`ICASES` / `CASES_RE` / `DEFAULTS` / `SWITCHS_END`) and `str_match_regex()`:
what it matches (exact, ignoring case, a regex with and without `REG_ICASE`, a
case that falls through, `break`, `DEFAULTS`), and that leaving the switch with
`return` from inside a case costs nothing. Up to 7.25.6 `SWITCHS` compiled a
regex on entry that only `SWITCHS_END` freed, so each such `return` lost it:
the leak is outside gbmem, so the test measures the libc heap (`mallinfo2()`)
over 100000 x 3 returns (+1.06 GB with the old macro, +1 KB with the new one).
A pattern that does not compile answers `FALSE` and is logged.

`test_ip_literals` compiles webstats' `bracket_ip_literals()`
(`yunos/c/webstats/src/ip_literals.c`), which writes every IPv4 address of the
mail body as `[a.b.c.d]` so that a mail relay does not read it as a phone
number. What stands on its own is bracketed, what is glued to a word, a slash
or another number is a version and stays as it is (`Chrome/142.0.0.0`,
`1.2.3.4.5`, `v1.2.3.4`, anything inside a tag). And the three forms that
7.25.20 left bare because `.` and `:` counted as glue: the dot of a sentence
end (`[a.b.c.d].`), a port (`[a.b.c.d]:443`), and the IPv4-mapped IPv6 form
(`[::ffff:a.b.c.d]`).

`test_local_day` compiles webstats' `yesterday_of()`
(`yunos/c/webstats/src/local_day.c`), the day a report run at a given moment
is about, and runs it in the time zone of Madrid: an ordinary day, the turn of
a month and of a year, and both changes of hour. Up to 7.25.20 the day was the
one of `now - 86400`: the day BEFORE yesterday from 00:00 to 01:00 after the
23-hour day of spring, and the same day from 23:00 on the 25-hour day of
autumn.
