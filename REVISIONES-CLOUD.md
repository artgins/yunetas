# Cloud review of main

Reviewed up to `cb3d6c70a` (2026-10-03): the answer to the review of
`860d63055`. What is resolved is removed from this file; this version is the
fixing session's answer to the review of `cb3d6c70a` (`c171fc621`), item by
item.

The wattyzer suite runs only right before a new SDK version is published
(the release checklist), not after a round of fixes: it is not pending here.
This answer: clean build (`yunetas clean/build --sdk-only`) with no
warning. Suite under `ulimit -Sn 1024`: the first run 286/287 --
`test_c_treedb_literal_wins` failed on *"child: io_uring_queue_init(4096)
FAILED: Cannot allocate memory"* (kernel memory under the parallel suite,
memlock unlimited; it passed alone at once, 221 s, and in the last nine
runs); the second run 287/287.

## Resolved

### Medium

| # | Item | Test |
|---|------|------|
| 1 | Agent: the scan-alone ask at the window's end is gone. Each pid the restart kills is recorded with its start time (`/proc/<pid>/stat`, field 22, read by `read_process_stat()`, which parses after the last `)`): `{yuno_id: [[pid, start_time]]}`. `process_is_gone(pid, start_time)`: gone when it does not exist, is a zombie, or runs with another start time (a reused pid); a task in D state is alive, with or without its `cmdline`. So a reused pid reads gone at once (no 5-minute hold), and a stuck task is never taken for gone | Live, on the local agent: a root look-alike of a stopped `gate_central^2120`, `deactivate-snap`: the warning lists `{'2120':[[971809,6713152]]}` (its pid and its start time); the look-alike killed: `2120` launched within the second. (A reused pid and a D-state task without `cmdline` are not staged.) |

### Low

| Item | Test |
|------|------|
| Agent, a launch in the first wait: `run_yuno()` holds the id there (`hold_restarted_yuno(..., only_waited=TRUE)`: only an id the wait still has, so the relaunch's own launches, after the wait, hold nothing); in the window it leaves it, as before | Not staged (it needs the old process to have lost its `cmdline`, or `run-yuno` refuses it) |
| Agent, the handover: the held ids are taken out before the warning, which names only the spared; with none left, no window opens (relaunch, done) | Live: `2120` stuck, `deactivate-snap`, `kill-yuno id=2120` 4 s later: at 10 s no *"still alive after 10 s"* and no *"none left to wait for"*; nothing launched beside the look-alike; `2120` down after it died; `run-yuno` put it back |
| Docs and comments: the CHANGELOG no longer says the spared get *"not launched again"* (they are skipped before that check) and describes the start time; the comment of the handover says the same; the header of `restart_wait_tick()` says 7.25.22 relaunched them anyway and the sparing's first form left them down; `YUNO_LIFECYCLE.md`: the start time, `run-yuno` among the operator's acts, `enable-yuno` gives back only in the first 10 s, the map shape | -- |

## Not done, and why

- **Still without a test of their own**: the agent's spare window in ctest
  (no ctest compiles `c_agent.c`; checked live each round), the C_UDP
  client, the websocket default max, a frame of exactly `max-1`.
