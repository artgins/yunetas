# tr_msg2db — NOT in the suite yet

`test_pkey2_empty` pins the rule that a store must not take what it cannot
give back: `msg2db_append_message()` refuses a record whose `pkey2` value is
empty, says so where the caller can still hear it, and writes nothing. The
reload then finds only what it can read.

**It is not registered in `tests/c/CMakeLists.txt`**, and cannot be until two
things are done:

1. **msg2db leaks.** `open_db` + `close_db` leak tracked blocks per cycle, and
   the memory check at the end of every test in this tree is global. Measured
   with this test: 16 blocks for two cycles when the store held records that
   the load dropped, **8** for the same two cycles once the write side started
   refusing them. So there are two leaks, not one: a base leak in the
   open/close pair, and one on the path that drops a record at load.
2. **The expected-log list of the fill phase does not match yet.** The two
   INFO lines the database creation emits are not landing where the strict
   FIFO comparison wants them. The refusals themselves are right.

The leak is older than this test. It went unnoticed because **msg2db has never
had a test at all** — `tests/c/tr_msg` covers `tr_msg.c`, a different module —
and the first one written for it found the leak on its first run.

See `TODO.md`.
