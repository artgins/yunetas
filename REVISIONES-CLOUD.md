# Cloud review of main

Reviewed up to `27b4b7293` (2026-10-03): the answer to the review of
`cb3d6c70a`. What is resolved is removed from this file; this version is the
fixing session's answer to the review of `27b4b7293` (`c46f05171`), item by
item.

The wattyzer suite runs only right before a new SDK version is published
(the release checklist), not after a round of fixes: it is not pending here.
This answer: clean build (`yunetas clean/build --sdk-only`) with no warning
or error, and the checks of each fix (live, on the local agent: no ctest
compiles `c_agent.c`). The full suite is not run after a round of fixes
(user rule of 2026-10-03): it runs for a release.

## Resolved

### Low

| Item | Test |
|------|------|
| Agent, the stat read's errors: `read_process_stat()` answers 0 (read), -1 (`ENOENT`/`ESRCH`: no such process) or -2 (any other errno -- `EMFILE`, a `hidepid` mount -- or a content that does not parse), and says -2 at the transition only (an ERROR, once until a read works: the restart asks every 100 ms). `process_is_gone()` takes -2 for ALIVE, so a stat that cannot be read no longer empties the first wait and gives a living yuno a second instance | Not staged (an `EMFILE` at the right moment) |
| Agent, a pid gone before it is recorded: not kept (-1); one whose stat cannot be read is kept with start time 0 and judged by its existence (logged). The comment says it | Not staged |
| Agent, `enable-yuno` and the hold: `restart_held` keeps the acts of each id (`kill`, `disable`, `run`); `enable-yuno` takes back `disable` only, and the id is the restart's again when no act is left. (`enable-yuno` of an enabled yuno answers *"Yuno not found or already enabled"* and touches nothing: the reachable case is a stop, a disable and an enable.) CHANGELOG and `YUNO_LIFECYCLE.md` say so | Live, on the local agent: `2120` stuck behind a root look-alike, `deactivate-snap`, then `kill-yuno`, `disable-yuno`, `enable-yuno` of `2120` inside the first 10 s: no window, no `run_yuno` of `2120`, down after the look-alike died (`27b4b7293` released the stop with the enable, and the relaunch ran it). Then the plain spare again: `{'2120':[[1055247,7148899]]}`, launched when the look-alike died |
| Comment: the header of `launch_spared_yuno()` parses again (*"The scan alone is fooled by a task that lost its cmdline..."*) | -- |

## Not done, and why

- **Still without a test of their own**: the agent's spare window in ctest
  (no ctest compiles `c_agent.c`; checked live each round), the C_UDP
  client, the websocket default max, a frame of exactly `max-1`.
