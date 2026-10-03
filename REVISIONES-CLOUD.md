# Cloud review of main

Reviewed up to `27b4b7293` (2026-10-03): the answer to the review of
`cb3d6c70a`. What is resolved is removed from this file; what follows is
still open.

Read-only review: the suite is run by the fixing session before each
review request, so it is not repeated here.

## Resolved in this round

- Agent: the scan-alone ask at the window's end is gone. Each pid the
  restart kills is recorded with its start time (`/proc/<pid>/stat` field
  22, parsed after the last `)`, no off-by-one, buffer large enough);
  `process_is_gone(pid, start_time)` reads a missing process, a zombie or a
  different start time as gone and a D-state task as alive. The
  `[[pid, start_time]]` shape is used by every reader and every merge.
- Agent: a launch in the first wait holds the id only while the wait still
  has it, so the relaunch's own launches hold nothing (`restart_sparing` is
  set before `run_enabled_yunos(TRUE)` at the handover, which skips the
  spared ids).
- Agent: the handover takes the held ids out before its warning, and opens
  no window when none is left.
- CHANGELOG, comments and `YUNO_LIFECYCLE.md` on the spared yunos, the start
  time, `run-yuno` and `enable-yuno` in the first 10 s.

## Open

### Low

- **Agent: a pid already gone when recorded is stored with `start_time = 0`,
  and 0 means "do not compare"** (`c_agent.c:10540-10548` writes it,
  `:10669` `if(start_time && ...)` skips it). The start time is read after
  the SIGKILL, so a killed process can already be reaped (and the
  `watcher_pid` from the treedb can be stale). Such a pid is then judged by
  existence alone: if it is reused during the wait or the window, it never
  reads gone, and the yuno is held 5 minutes and left down (*"still alive
  after 5 minutes: not launched"*) — the case this commit says it closed,
  now without the scan-alone rescue. No double launch through this path.
  The comment says "gone already" while the code treats it as "alive,
  start time unknown". *Fix:* do not append a pid whose stat read fails with
  `ENOENT`; log any other failure.
- **Agent: `read_process_stat()` fails silently, and `process_is_gone()`
  takes every failure for gone** (`:10619-10626`, `:10663`): `EMFILE` /
  `ENFILE` (the suite runs under `ulimit -Sn 1024`) or a `hidepid` mount
  read as gone, with nothing logged. In the first wait, a live D-state pid
  read as gone empties the map, and `run_enabled_yunos(gobj, FALSE)`
  (`:10714`, no scan) launches a second instance beside it. In the window
  the scan masks it. The old `kill(pid, 0)` / `ESRCH` test was dropped in
  the rewrite. *Fix:* gone only for `ENOENT` / `ESRCH`; anything else is
  logged and read as alive.
- **Docs: `enable-yuno` releases every hold, not only a disable one**
  (`cmd_enable_yuno` → `unhold_restarted_yuno()` for every id it touches,
  `:5933`). An `enable-yuno` of an enabled yuno killed (or run) in the first
  10 s makes the relaunch run it. `YUNO_LIFECYCLE.md` and the CHANGELOG say
  only "gives a disabled one back": either say it, or unhold only ids held by
  `disable-yuno`.
- **Comment**: the header of `launch_spared_yuno()` (`:10822-10827`) lost the
  verb *"are fooled"* that *"the scan alone"* hung on; the sentence no
  longer parses.
- **Still without a test of their own**: the agent's spare window (no ctest
  compiles `c_agent.c`), the C_UDP client, the websocket default max, a
  frame of exactly `max-1`.

## Fix order

The stat read's errors (the double-launch one), then `start_time = 0`, the
`enable-yuno` hold, the comment.
