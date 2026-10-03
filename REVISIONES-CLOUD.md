# Cloud review of main

Reviewed up to `f41529d9c` (2026-10-03): the answer to the review of
`0db20a63d` (`bc7e036b0`, plus the report commits `945fe7711`,
`17ee14309`, `f41529d9c`). What is resolved is removed from this file; what
follows is still open.

Read-only review: the suite is run by the fixing session before each
review request, so it is not repeated here.

## Resolved in this round

- `--stop`: the list is growable (`gbmem_realloc`), so look-alikes cannot
  crowd the daemon out, root caller or not; zombies are not taken, an
  `EPERM` on one is not a failure, an unreadable `cmdline` fails. Every
  return path frees; `gbmem` works before `gbmem_setup` (static defaults).
- dbsimple: a `..` returns to the user the walk started with; the walk is
  bounded (a `..` restart is strictly shorter, links capped at 40). No path
  found that yields a non-root trusted user for a root-only chain.
- fs_watcher: `add_watch()` drops the old path of a wd watched again
  elsewhere (only while it still maps to that wd); the re-watch listing does
  not follow links; the doc no longer says "never blocks the loop".
- The agent's spare window keeps `{yuno_id: [pids]}` and launches only the
  spared ids; `launch_enabled_yuno()` is the old body of
  `run_enabled_yunos()` unchanged, so `run-yuno` behaves as before.
- logcenter: an empty `restart_yuneta_command` is an ERROR, not
  `system(NULL)`. `c_authz.c`'s inline comment of `list-jwk`.

## Open

### Medium

1. **Agent spare window: a spared yuno the operator stops is still
   relaunched** (`c_agent.c`). `cmd_kill_yuno`, `update-binary`, `run-yuno`
   and `disable-yuno` never touch `priv->restart_wait` (it is referenced only
   by `restart_nodes()`, the two ticks, `mt_stop` and `mt_destroy`). So:
   - `kill-yuno` of a spared yuno: its process dies, the next
     `restart_spare_tick` finds it gone and `launch_enabled_yuno()` launches
     it (kill-yuno does not disable);
   - `run-yuno` after the old process dies, then `kill-yuno` within the 5
     minutes: the id is still in the map (the scan matched the new
     instance), and the window relaunches it.

   The CHANGELOG (*"a yuno an operator stops meanwhile (kill-yuno,
   update-binary, run-yuno) is not launched by that window"*) and the header
   of `restart_spare_tick` hold only for ids that were not spared. The live
   check of `f41529d9c` stopped a yuno that was **not** spared (`5120`), so it
   does not cover this.
   *Fix:* drop the id from `restart_wait` in `kill-yuno`, `update-binary`,
   `run-yuno` (and `disable-yuno`).

2. **Agent spare window: "gone" no longer looks at the pids**
   (`launch_spared_yuno()` `c_agent.c:10704-10732`,
   `find_living_yuno_pids()` `:9097-9104`). "Gone" is decided only by the
   `/proc` scan, which skips a process whose `cmdline` cannot be read or has
   no arguments. A SIGKILLed task loses its `cmdline` at `exit_mm()`, before
   its files are released; a yuno stuck in D state while closing or flushing
   is exactly the one that outlives the 10 s. The scan reports it gone, and
   a second instance starts beside one still holding its queues — the
   *"opened as not master"* failure the window exists to prevent.
   `process_is_gone()` (reads `/proc/<pid>/stat`, sees `D` as alive) is no
   longer consulted after the 10 s.
   *Fix:* gone = the scan finds nothing **and** every recorded pid passes
   `process_is_gone()` (a reused pid then only delays to the 5-minute cap).

### Low

- **Agent: ids launched at the 10 s handover stay spared**
  (`c_agent.c:10646-10650`). `run_enabled_yunos(gobj, TRUE)` launches the
  ids whose recorded pid still looks alive but whose scan is empty; they are
  not dropped from the map, so for 5 minutes a `kill-yuno` of them is undone
  (item 1), and the closing warning says *"still alive after 5 minutes: not
  launched"* about yunos that are running.
- **Agent: cost of the spare tick.** Each second, per spared id, one
  `gobj_list_nodes` and one full `/proc` scan reading every `cmdline` —
  `/proc/<pid>/cmdline` of a task holding its mmap lock can block the loop,
  and these are aimed at exactly such tasks. One scan per tick, matched
  against all ids, would do. An unreadable `/proc` logs the same ERROR every
  second (up to 300 per id). *"each one launched"* (`:10679`) is said even
  when nothing was launched (disabled, deleted, already running).
- **fs_watcher: `readdir()` errors are not detected** in
  `queue_unwatched_subdirs()` (`fs_watcher.c:968`): no `errno = 0` before it,
  no check after the loop, so an EIO/ESTALE mid-listing ends it in silence
  and the subdirectories not read are never watched. The `walk_dir_tree()`
  it replaced has done that check since 7.25.4. Same place (`:975`): a failed
  `fstatat()` on a `DT_UNKNOWN` entry is "not a directory" for any errno
  (the old code logged all but ENOENT/EACCES).
- **fs_watcher: hidden subdirectories are now queued** by the re-watch (the
  old `walk_dir_tree(..., WD_MATCH_DIRECTORY)` skipped them, without
  `WD_HIDDENFILES`). `add_watch_recursive()` skips them,
  `push_subdirectories()` takes them: the three paths disagree. Harmless for
  timeranger2 keys; a decision and a line in the doc.
- **`secret_attrs`: the root skip is too broad** (`test_secret_attrs.c:1741`).
  Only the positive cases tell nothing as real root (the file really is
  root's). The three refusal cases fake the file as the intruder's, reach
  `trusted_dir_owner()`, and still tell the old code from the new — the `..`
  regression test among them. As root all of them are skipped with a
  `printf`, and ctest stays green (no `SKIP_RETURN_CODE`). *Fix:* skip only
  the positive checks, or report a real skip.
- **logcenter: exit 1 is still explained as "sudo refused or the unit's
  restart failed"**, which only `restart-yuneta` can mean; the non-unit
  default command (`yshutdown; yuneta_agent --start`) or a configured one
  exits 1 for other reasons. And `restart-yuneta` maps a child's failure to
  1, so "above 128" cannot come from it.
- **`c_authz.c:1798`**: the function header still says `list-jwk` answers
  "(empty)"; the inline comment (and the code) say it answers the config's
  `jwks`.
- **Docs**: `daemon_launcher.md` / `ENTRY_POINT.md` do not say that an
  `EPERM` on a zombie is not a failure.
- **Still without a test of their own**: the agent's spare window (no ctest
  compiles `c_agent.c`), the C_UDP client, the websocket default max, a
  frame of exactly `max-1`.

## Fix order

1, 2, then the handover ids, the `readdir` errno, the `secret_attrs` skip,
and the rest.
