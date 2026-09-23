# tr_msg2db test

The first test of `tr_msg2db.c`. `tests/c/tr_msg` covers `tr_msg.c`, which is a
different module, so msg2db had no test at all before this one.

`test_pkey2_empty` pins the rule that a store must not take what it cannot
give back. `msg2db_append_message()` refuses a record whose `pkey2` value is
empty, says so where the caller can still hear it, and writes nothing. The
reload then finds only what it can read.

`test_msg2db_load_failed` pins what msg2db serves when a key's history does
not load whole. msg2db loads forward and keeps the LAST message per `id` and
`pkey2`, so a load that stops half way leaves an OLD message in the place of
the current one. Such a key is loaded again backward, keeping the NEWEST
message of each `pkey2`:

1. a file of `dev1` whose only append was never acknowledged (a md2 of 0
   rows, its content not empty), between its old and its new message: the
   file is ignored with a warning, and the NEW message is served;
2. the md2 of the new message damaged (it cannot be read, mode 000):
   `dev1` is reloaded backward, the load stops at once at its newest file, and
   `dev1` is not served (absent, with an ERROR naming it); `dev2`, whole, is.
   The damaged file is the file of the current period, where the next message
   goes: a second ERROR says so, and the next message of `dev1` is refused
   (tranger refuses an append into a flagged file), so it is not served
   either;
3. the damage in the MIDDLE of `dev1`'s history, with two alarms: `X`, whose
   newest message is in the last file, is served (its current message); `Y`,
   whose newest message is in the damaged file, is absent (its old message is
   not served as current); `msg2db_id_incomplete()` is TRUE for `dev1`, FALSE
   for `dev2`, and stays TRUE after `Y`'s next message, which is served;
4. the store of 3 with a record of `dev1` whose `pkey2` is empty on each side
   of the damage: one read by the forward load, one read only by the backward
   reload. Both are dropped, and "Records NOT loaded, 'pkey2' empty" says
   `dropped: 2`. It said 1: the count of the forward load was put back after
   the reload;
5. the md2 of the new message ends in a part of a row (a power cut during
   the write of a row). That is an append that was never acknowledged, not
   damage: the md2 is cut back to its whole rows with one warning, `dev1` is
   whole (`msg2db_id_incomplete()` is FALSE), and its next message is stored
   and served, also after a restart. Before the fix, the file was flagged
   and every new message of `dev1` was refused until the period changed.

Cases 2, 3 and 4 use a md2 of mode 000. They are skipped as root, because
root can read such a file.

1 and 2 served the OLD message before the fix of the fourth fix round. 3 was
red after it: the whole of `dev1` was dropped, `X` too (fifth fix round).

## Why it was kept out of the suite, and why it is in now

It was left unregistered because msg2db seemed to "leak" 8 tracked blocks per
`open_db` + `close_db`, and the memory check at the end of every test is
global. The leak was the test's. A master tranger watches its `/disks`
directory with inotify, and `tranger2_shutdown()` cancels that watcher
asynchronously: the io_uring completions that free each `fs_event` arrive on
later turns of the loop. The test never gave the loop those turns, so the
memory was still allocated at the check.

`drain_loop()` runs the loop ten times after each shutdown, as
`tests/c/timeranger2/test_rt_disk_multi_feed.c` does, and the test passes
clean. Any test that shuts down a master tranger with a `yev_loop` needs the
same turns before its leak check:

```c
tranger2_shutdown(tranger);
for(int i = 0; i < 10; i++) {
    yev_loop_run_once(yev_loop);
}
```

## Run

```bash
ctest -R tr_msg2db --output-on-failure --test-dir build
```
