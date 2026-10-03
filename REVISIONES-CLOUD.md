# Cloud review of main

Reviewed up to `cb3d6c70a` (2026-10-03): the answer to the review of
`860d63055`. What is resolved is removed from this file; what follows is
still open.

Read-only review: the suite is run by the fixing session before each
review request, so it is not repeated here.

## Resolved in this round

- Agent, the operator in the first 10 s wait: `kill-yuno` / `disable-yuno`
  hold the id (`restart_held`) and keep its pids waited for; both ends of the
  wait (map empty, 10 s handover) skip the held ids and then clear the set; a
  new restart clears it; `hold_restarted_yuno()` does nothing outside a
  restart, so no id is skipped forever. `enable-yuno` releases a disable
  hold, and the pids still cover it. `run-yuno` stays the operator's will.
  `forget_spared_yuno()` acts only in the window. No ownership or iteration
  issue (`restart_held` freed in `mt_destroy`, keys copied before deletion).
- fs_watcher: `push_subdirectories()` uses `fstatat(AT_SYMLINK_NOFOLLOW)`
  and logs a failure other than `ENOENT`; no path follows a symlink now.
- CHANGELOG line for `C_FS` / recursive watchers; `YUNO_LIFECYCLE.md`
  describes the window; `test_secret_attrs.c` comment.

## Open

### Medium

1. **Agent: the scan-alone ask at the window's end reopens the D-state double
   launch** (`c_agent.c:10725-10737`, `launch_spared_yuno(gobj, id, NULL)`).
   It was added for a reused pid, but at 5 minutes it drops the pid check for
   every yuno still waited for, and the likeliest reason a pid is alive at 5
   minutes is a long D-state hang (NFS, a dead disk), not a reused pid. A
   task past `exit_mm()` has an empty `cmdline`, which
   `find_living_yuno_pids()` skips (`continue; // no arguments`), so the scan
   reports it gone and the yuno gets a new instance beside the old process,
   which still holds its files. The function header (`:10760-10768`) says
   exactly this about the scan alone; the commit, the CHANGELOG and
   `YUNO_LIFECYCLE.md:899-901` only give the benefit. Before `cb3d6c70a` that
   yuno was left down with the 5-minute warning.
   *Fix:* tell the two cases apart per pid: record the `starttime` of
   `/proc/<pid>/stat` when the restart kills it and treat a pid whose
   starttime differs as reused (gone); or treat a live pid with a non-empty
   foreign `cmdline` as reused and an empty one outside `Z` as the stuck
   yuno.

### Low

- **Agent: a `run-yuno` in the first 10 s leaves the id waited for**
  (`run_yuno()` `:9301` → `forget_spared_yuno()`, window-only now). If the
  operator's launch gets through (the old process lost its `cmdline`, so
  `yuno_lives_unregistered()` misses it), the id is spared at 10 s although
  its new instance runs: no double launch (`running` / `is_launching`
  guard), but 5 minutes of `/proc` scans and a closing *"still alive after
  5 minutes: not launched, run-yuno once they are gone"* about a yuno that is
  running. *Fix:* in the first wait a launch holds the id
  (`restart_held`).
- **Agent: the handover warning names held ids** (`c_agent.c:10668-10681`):
  *"…each one launched when it is gone"* prints `restart_wait` before the
  held ids are removed, so it lists yunos the operator stopped. If every id
  left is held, the window opens on an empty map and says *"none left to
  wait for"* a second later. Remove the held ids first, and skip the window
  when nothing is left.
- **Docs and comments:**
  - `CHANGELOG.md:94-96` and the comment at `c_agent.c:10663-10664` say the
    skipped yunos are each told *"yuno alive but not connected to the agent:
    not launched again"*; `run_enabled_yunos()` skips ids in `restart_wait`
    (`:9914-9916`) before `yuno_lives_unregistered()` is reached, so that
    line is never logged for them.
  - `c_agent.c:10617-10619` says up to 7.25.22 such a yuno *"was never
    launched again"*; `:10665` and the docs say it was relaunched anyway.
  - `YUNO_LIFECYCLE.md:905`: *"`enable-yuno` gives it back to the
    restart"* holds only in the first 10 s; in the window `disable-yuno`
    forgets the id and a later `enable-yuno` gives nothing back (the
    CHANGELOG says it right).
- **Still without a test of their own**: the agent's spare window (no ctest
  compiles `c_agent.c`), the C_UDP client, the websocket default max, a
  frame of exactly `max-1`.

## Fix order

1, then the `run-yuno` in the first wait, the handover warning, and the
docs.
