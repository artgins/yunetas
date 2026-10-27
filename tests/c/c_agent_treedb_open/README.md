# c_agent_treedb_open test

Tests that the agent **ends, with exit 0, when its treedb does not open**, on
the agent's own source (`yunos/c/yuno_agent/src/agent_treedb.c`, compiled
into the test) and its own schema.

The agent opens its treedb (`treedb_yuneta_agent`) with its schema imposed,
and an agent without its treedb must not run. On an `open-treedb` answered -1
it ends with exit 0, which ydaemon does not relaunch: the main agent stays
down until it is started again, and `yuneta_agent22` (which opens no treedb)
stays up as the way in. Up to 7.25.4 a refused schema answered 0, and the
agent ran without its treedb.

## What it pins

The yuno creates a `C_TREEDB` service as the agent's `mt_play` does, in a
store of its own (`~/tests_yuneta/c_agent_treedb_open`), and calls
`open_agent_treedb()` twice:

1. the first opens the treedb, under the id of the agent's schema;
2. the second answers -1 (*"treedb 'treedb_yuneta_agent' is already open
   here"*), and the process must end there with **exit 0** and *"Cannot
   start agent treedb: ..."* on the console.

The yuno cannot check its own exit code, so ctest runs it through
`run_test.sh`, which fails on any other exit code, and on an exit 0 without
the message (a run that never reached the second open). If the second open
returns, the yuno logs *"TEST FAIL: the agent went on without its treedb"*
and ends with -1.

## Run

```bash
ctest -R test_c_agent_treedb_open --output-on-failure --test-dir build
```
