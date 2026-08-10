# tr_msg2db — NOT in the suite yet

`test_pkey2_empty` pins what a msg2db does with a record whose `pkey2` value is
empty: the write side accepts it (`kw_has_key`), the load side refuses it
(`!empty_string`), so the record is stored and then dropped at every load for
the life of the store. The test asserts the reporting -- the first dropped
record logged whole, and one tally when the topic finishes loading.

**It is not registered in `tests/c/CMakeLists.txt`**, because it cannot pass
yet: `msg2db_open_db()` + `msg2db_close_db()` leak **8 tracked blocks per
cycle** (~1 KB), and the memory check at the end of every test in this tree is
global. Measured with this very test: 8 blocks for one cycle, 16 for two,
identical with 0 or 5 bad records, so the leak is in the open/close pair and
has nothing to do with the records.

That leak is older than this test. It went unnoticed because **msg2db has never
had a test at all** -- `tests/c/tr_msg` covers `tr_msg.c`, a different module.

To finish this: find the 8 blocks, then register the directory. Two smaller
things are also open in the test itself, both marked in the source: the
expected-log list of the fill phase, and the `msg2db_list_messages()` call,
which returns 0 records where 3 are expected.

See `TODO.md`.
