# tr_msg2db test

The first test of `tr_msg2db.c`. `tests/c/tr_msg` covers `tr_msg.c`, which is a
different module, so msg2db had no test at all before this one.

`test_pkey2_empty` pins the rule that a store must not take what it cannot
give back. `msg2db_append_message()` refuses a record whose `pkey2` value is
empty, says so where the caller can still hear it, and writes nothing. The
reload then finds only what it can read.

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
