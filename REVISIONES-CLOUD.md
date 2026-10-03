# Cloud review of main

Reviewed up to `860d63055` (2026-10-03): the answer to the review of
`f41529d9c`. What is resolved is removed from this file; what follows is
still open.

Read-only review: the suite is run by the fixing session before each
review request, so it is not repeated here.

## Resolved in this round

- Agent spare window: a spared yuno leaves it on `kill-yuno` (both the
  running loop and `signal_unconnected_yunos()`, every filter), on
  `disable-yuno` and on any launch (`run_yuno()`). "Gone" = every recorded
  pid gone (`process_is_gone()`, D state alive) and then the scan finds none.
  The handover no longer launches spared ids. No use-after-free in
  `restart_spare_tick` (the key is copied before the launch, and a launch
  only forgets the current key). The sparing state is reset at an empty
  map, at the 5-minute expiry and by a new restart.
- fs_watcher: `readdir()` errors checked through `errno` in both loops;
  `fstatat()` failures logged in `queue_unwatched_subdirs()`; hidden
  subdirectories watched by the first walk too (documented; the new test is
  red on the old code).
- logcenter text, `c_authz.c` header, `daemon_launcher.md` / `ENTRY_POINT.md`
  (zombies, `EPERM` on one).

## Open

### Medium

1. **Agent: `forget_spared_yuno()` acts during the first 10 s wait too, and
   undoes it** (`c_agent.c:10769`, called from `:5394`, `:6002`, `:9237`,
   `:9296`; the wait at `:10613-10630`). Before the 10 s mark `restart_wait`
   is the "killed pids not gone yet" map, not the spare map, and the forget
   does not look at `restart_sparing`. Scenario: `deactivate-snap`, yuno X
   stuck in D state; within the 10 s the operator sends `kill-yuno id=X` (X
   is not running, so it goes through `signal_unconnected_yunos()`) and X's
   id leaves the map. Once the other ids are gone the map is empty, and
   `restart_wait_tick()` calls `run_enabled_yunos(gobj, FALSE)`:
   `launch_enabled_yuno()` without `yuno_lives_unregistered()`, so X gets a
   second instance beside its living process, and the operator's
   `kill-yuno` is overridden. The same with `disable-yuno` + `enable-yuno`
   inside the 10 s. Before this commit X stayed in the map and was spared at
   the handover.
   *Fix:* forget only while `restart_sparing`; in the wait phase keep the
   pids, and if the operator must be honoured there, record the id apart and
   leave it out of the handover.

### Low

- **Agent: a reused pid keeps its yuno down for good, not "only a delay"**
  (comment at `c_agent.c:10721`, expiry `:10700-10711`). A recorded pid
  reused within the window never reads gone, so the id is dropped at the
  5-minute expiry with *"still alive after 5 minutes"* and never launched.
  Rare with a 4M `pid_max`, real with 32768. The comment should say it, or
  the expiry should run the scan once for ids whose only living pids do not
  match the yuno.
- **fs_watcher: the `fstatat` half is done in one loop only.**
  `push_subdirectories()` (`fs_watcher.c:1927-1929`) still uses
  `is_directory(child)` for `DT_UNKNOWN`: `stat()` follows symlinks (unlike
  the re-watch's `AT_SYMLINK_NOFOLLOW`) and fails silently. The report of
  `860d63055` says both are logged.
- **fs_watcher: hidden subtrees at start** are a behaviour change for
  `C_FS` / watchfs with `recursive`: a tree that already holds `.git` or a
  cache now takes inotify watches (and events) for all of it. Documented;
  worth a CHANGELOG line for those consumers.
- **`YUNO_LIFECYCLE.md:883-891`** still describes the restart as *"still
  alive after 10 s: those are not launched again"*. The spare window, its
  exits (`kill-yuno`, `disable-yuno`, a launch) and the 5-minute
  *"run-yuno"* warning are not there.
- **`test_secret_attrs.c:1738`**: the comment says the test never runs as
  root, but `is_yuneta_user()` (`helpers.c`) accepts any user in group
  `yuneta`, root included. Say "unless root is in group yuneta" rather than
  restore the skip.
- **Still without a test of their own**: the agent's spare window (no ctest
  compiles `c_agent.c`), the C_UDP client, the websocket default max, a
  frame of exactly `max-1`.

## Fix order

1, then `push_subdirectories()`'s `fstatat`, `YUNO_LIFECYCLE.md`, and the
rest.
