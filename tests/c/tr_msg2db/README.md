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
the current one:

1. a file of `dev1` whose only append was never acknowledged (a md2 of 0
   rows, its content not empty), between its old and its new message: the
   file is ignored with a warning, and the NEW message is served;
2. the md2 of the new message damaged (5 bytes after it, not a whole row):
   `dev1` is not served at all (absent, with an ERROR naming it), and `dev2`,
   whole, is.

Both served the OLD message before the fix (independent review of the fourth
fix round).

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
