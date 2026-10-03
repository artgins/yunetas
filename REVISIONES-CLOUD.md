# Cloud review of main

Reviewed up to `860d63055` (2026-10-03): the answer to the review of
`f41529d9c`. What is resolved is removed from this file; this version is the
fixing session's answer to the review of `860d63055` (`07acd1533`), item by
item.

The wattyzer suite runs only right before a new SDK version is published
(the release checklist), not after a round of fixes: it is not pending here.
This answer: clean build (`yunetas clean/build --sdk-only`) with no
warning, suite 287/287 under `ulimit -Sn 1024` on the dev machine.

## Resolved

### Medium

| # | Item | Test |
|---|------|------|
| 1 | Agent: `forget_spared_yuno()` acts only in the window (`restart_sparing`). In the first 10 s, `kill-yuno` (both paths) and `disable-yuno` call `hold_restarted_yuno()`: the id's pids stay in the map (still waited for), and the id goes to `restart_held`, which neither the relaunch nor the handover runs (the handover also takes the held ids out of the window). `enable-yuno` releases it (`unhold_restarted_yuno()`); a new restart clears it. A launch (`run_yuno()`) forgets only in the window | Live, on the local agent: a root look-alike of a stopped `gate_central^2120`, `deactivate-snap`, `kill-yuno id=2120` 4 s later (inside the first wait). `2120` was not launched beside the look-alike (only it and `2020`'s processes ran, no `run_yuno` of `2120` in the log; the code before launched it at the end of the wait), and stayed down once the look-alike was killed. `run-yuno id=2120` put it back |

### Low

| Item | Test |
|------|------|
| Agent, a reused pid: at the window's end the yunos still waited for are asked once without their pids (`launch_spared_yuno(..., NULL)`, the scan alone); one with no process of its own is launched, the rest said. The comment says what a reused pid does | No test (a reused pid is not staged) |
| fs_watcher: `push_subdirectories()` uses `fstatat(AT_SYMLINK_NOFOLLOW)` for `DT_UNKNOWN` and says a failure other than `ENOENT`: the pass no longer follows a symlink to a directory (the first walk, `lstat`, and the re-watch never did) | No test (`DT_UNKNOWN` comes from filesystems that do not fill `d_type`) |
| fs_watcher: hidden subtrees at start, a CHANGELOG line for `C_FS` and the watch utilities with `recursive` (a `.git` or a cache now takes watches and events from the start) | -- |
| `YUNO_LIFECYCLE.md`: the restart's caveat describes the spare window, its exits (`kill-yuno`, `disable-yuno`, `enable-yuno`, a launch), its last ask and the 5-minute warning, with an example | -- |
| `test_secret_attrs.c`: the comment says the test runs as root only if root is in group `yuneta`, and what that means for its cases | -- |

## Not done, and why

- **Still without a test of their own**: the agent's spare window in ctest
  (no ctest compiles `c_agent.c`; checked live, each round), the C_UDP
  client, the websocket default max, a frame of exactly `max-1`.
