# Cloud review of main

Reviewed up to `f41529d9c` (2026-10-03): the answer to the review of
`0db20a63d`. What is resolved is removed from this file; this version is the
fixing session's answer to the review of `f41529d9c` (`374ab424b`), item by
item. Every fix has a test that fails on the code before it, except where it
says otherwise.

The wattyzer suite runs only right before a new SDK version is published
(the release checklist), not after a round of fixes: it is not pending here.
This answer: clean build (`yunetas clean/build --sdk-only`) with no
warning, suite 287/287 under `ulimit -Sn 1024` on the dev machine.

## Resolved

### Medium

| # | Item | Test |
|---|------|------|
| 1 | Agent: a spared yuno leaves the window when the operator stops it (`kill-yuno`), disables it, or it is launched by anyone (`run_yuno()` takes its id out: `run-yuno`, the window itself). `forget_spared_yuno()`; the tick copies the id before the launch, which frees the key | Live, see below |
| 2 | Agent: gone = every pid the restart killed gone (`process_is_gone()`, D state alive) AND then the scan finds no process of the yuno. The scan (every `cmdline`) runs only once the pids are gone, so the per-second cost of the review's low item goes with it. And the 10 s handover no longer launches the spared ids (`run_enabled_yunos()` skips them while sparing): only the window does, when they are gone -- the handover issue of the lows | Live, see below |

Live check of 1 and 2 on the local agent: as in `f41529d9c`, a root
look-alike of a stopped `gate_central^2120` (the restart cannot kill it),
then `deactivate-snap`. At the 10 s handover every yuno was launched except
`2120` (spared, `{'2120':[863291]}`); `kill-yuno id=2120` inside the window
closed it at the next tick (*"none left to wait for"*); the look-alike
killed: `2120` stayed down. `run-yuno id=2120` put it back.
The first try of it found a gap of the fix itself: a spared yuno is never
"running" for the agent, so its `kill-yuno` goes through
`signal_unconnected_yunos()`, not the loop of the running ones where the
forget was -- the window relaunched it. The forget is in both now.

### Low

| Item | Test |
|------|------|
| fs_watcher: `readdir()` checked through `errno` in `queue_unwatched_subdirs()` and in the pass's `push_subdirectories()` (which had the same gap): a listing that fails half way is an ERROR, not the end. A failed `fstatat()` on a `DT_UNKNOWN` entry is an ERROR unless `ENOENT` | No test (an `EIO` mid-listing is not staged) |
| fs_watcher: hidden subdirectories. A `.name` made later (`IN_CREATE`) and the ones the pass meets were always watched; only the first walk left them out. The decision: watched whenever they appear; the first walk takes them too (`WD_HIDDENFILES`), the doc says so with an example | `test_fs_watcher_overflow` `do_test_hidden_at_start`: a file in a `.h` there before the watch is heard (red on `f41529d9c`: heard 0 times) |
| `secret_attrs` under real root: it cannot happen. The entry point refuses a user that is not `yuneta` or of its group (*"To run yunos the user must be 'yuneta'..."*, checked running it with `sudo`: exit 255 before any case). The SKIP branch of `bc7e036b0` was dead code: removed, with a comment saying why | -- |
| logcenter: the text names only what it can know (126/127 from the shell; `restart-yuneta` answers 1 and says why on its stderr); the "above 128" and the sudo cause are gone | No test (no ctest runs logcenter) |
| `c_authz.c`: the function header of the jwk question says `list-jwk` answers the config's `jwks` | -- |
| Docs: `daemon_launcher.md`, `ENTRY_POINT.md`: a zombie of the name is not taken, and an `EPERM` on one is no failure | -- |
| Agent: an unreadable `/proc` logs an ERROR each second while a spared yuno's pids are gone and the scan cannot run: kept (each one is a real failure, and it lasts at most the window); the scan no longer runs before the pids are gone | -- |
| *"each one launched"* said when nothing was launched: the closing line is now *"yunos spared by the restart: none left to wait for"* | -- |

## Not done, and why

- **Still without a test of their own**: the agent's spare window in ctest
  (no ctest compiles `c_agent.c`; checked live), the C_UDP client, the
  websocket default max, a frame of exactly `max-1`.
