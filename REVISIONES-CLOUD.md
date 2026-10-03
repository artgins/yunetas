# Cloud review of main

Reviewed up to `0db20a63d` (2026-10-03): the answer to the review of
`a8e6dd30d` (`bbff25b3b`, `0db20a63d`). What is resolved is removed from
this file; what follows is still open.

The review's own check, on `0db20a63d`: clean build (`yunetas clean/build
--sdk-only`) with no compiler warning, suite 287/287 as user `yuneta`
under `ulimit -Sn 1024` (240 and 241 first failed with the container's default `fs.inotify.max_user_instances=128`, *"INOTIFY INSTANCES"* reached; passed with the packaged 4096: environment, not code).

## Resolved in this round

All four medium items and every low item of the previous report are fixed
as described in `bbff25b3b`/`0db20a63d`: the C_TCP flush (`ret < -1000`),
`--stop` by `argv[0]` (SysV script spared, EPERM said, renamed binary still
stopped, overflow fails), the fs_watcher re-watch (bounded, ENOSPC ends it,
looked up by path), the dbsimple symlinked parent, the C_PROT_MQTT header
bound, OpenSSL `want_retries`, the logcenter restart failure, the spared
yunos of `restart_nodes()`, ExecStopPost, `close_range` and the `list-jwk`
doc. The findings below are new, most of them in the fixes themselves.

## Open

### Medium

1. **Agent: the 5-minute spare window relaunches every enabled yuno that is
   not running, not only the spared ones** (`c_agent.c:10592-10597`). Each
   tick where a spared pid is gone calls `run_enabled_yunos(gobj, TRUE)`,
   which launches any enabled, not running, not launching yuno.
   `kill-yuno` does not disable a yuno, so a yuno an operator stops in those
   5 minutes (the hot-patch flow: `kill-yuno` → `update-binary` →
   `run-yuno`) is started again as soon as an unrelated stuck yuno dies —
   possibly between the kill and the binary upload. Before, the window was
   10 s; nothing in the CHANGELOG says it is now 5 minutes of
   auto-relaunch. And the end of the window (`:10592`) calls
   `run_enabled_yunos(gobj, FALSE)`, which skips `yuno_lives_unregistered()`:
   minutes after the restart, a yuno launched by the window itself or by
   `run-yuno` may be alive and not yet registered (its `launching_yunos`
   entry expired), and gets a second instance.
   *Fix:* keep `{yuno_id: [pids]}` and launch only the ids whose pids are
   gone; pass `TRUE` at the end of the window.

2. **`--stop` run as root: "own processes first" does not protect the real
   agent** (`ydaemon.c:478-482`, `daemon_shutdown()`). The first pass takes
   `st_uid == geteuid()`; the agent runs as `yuneta`, so for a root caller
   (operator, the RPM scriptlet's first try) it lands in the second pass with
   every local user's look-alikes. `/proc` is globbed in lexical order
   (`/proc/1000` before `/proc/999`), so 64 look-alikes still push it out of
   `MAX_STOP_PIDS`: the agent stays up. What `0db20a63d` fixed is the
   silence (exit 1 now), not the crowding; `daemon_launcher.md` and
   `ENTRY_POINT.md` claim the real ones cannot be crowded out, which holds
   only when the caller has the daemon's uid.
   *Fix:* a growable list (`gbmem_*`) instead of the fixed 64, or a
   priority by the pid file's owner / the run-as user.

### Low

- **dbsimple: `..` carries the trusted user of a directory the resolved path
  does not pass through** (`dbsimple.c:169-174` with `trusted` kept across
  restarts in `trusted_dir_owner()`). `/a/Xdir/../W/f` (`/a` root's, `Xdir`
  X's and closed, `W` root's 02775): the walk enters `Xdir` (trusted = X),
  the `..` restarts on `/a/W/f` with X still trusted, `W` breaks the chain,
  the answer is X; for `/a/W/f` it is root only. If X is in `W`'s group, X
  can plant a file a root yuno loads and then hands back to X with the next
  save. Carrying the user is right for a symlink (its owner chose the
  target), not for `..`. Needs a configured path with such a `..`.
  *Fix:* restart the trust (`trusted = -1`) on a `..` rewrite.
- **fs_watcher: `jn_paths_wd` keeps the old path of a renamed and re-watched
  directory** (`add_watch()` `fs_watcher.c:1438-1444`, `drop_tracked()`).
  The same wd at a new path overwrites `jn_tracked_paths[wd]`, and the old
  `jn_paths_wd[old] = wd` is never removed. Now that every watcher keeps it
  and it is the "already watched?" test, a directory made later at the old
  path under a re-watched parent is skipped by `queue_unwatched_subdir_cb`
  and never watched; the entries also grow with each rename.
  *Fix:* in `add_watch()`, drop the entry of the path `jn_tracked_paths[wd]`
  named before, when it maps to this wd.
- **fs_watcher: one level of a re-watched directory is listed whole in the
  loop turn** (`queue_unwatched_subdirs()`): the tries are bounded at 64, the
  readdir + lstat of each directory is not (a `keys/` of 100000 keys), and
  the doc says it "never blocks the loop". A directory gone between
  `add_watch()` and the walk logs *"Cannot open directory"* as an ERROR with
  stack (benign race; `add_watch()` calls the same case a warning), and the
  return value is dropped without `// Error already logged`.
- **`--stop` details** (`ydaemon.c`):
  - An empty `argv[0]` (a zombie, a process mid-exec) skips the name check
    (`:484`), and the SIGQUIT pass signals without `stop_pid_is_gone()`: a
    root zombie of the comm makes `--stop` as `yuneta` exit 1 with EPERM for
    a process already dead.
  - A failure to read `/proc/<pid>/cmdline` or to `stat` `/proc/<pid>` is
    taken as "gone meanwhile" without a word, whatever the errno (`:466`,
    `:479`). Only ENOENT/ESRCH mean gone.
  - The comment above `daemon_shutdown()` and the **Returns** of
    `daemon_launcher.md` still give -1 only for "could not be signalled",
    not for the overflow.
- **logcenter: the exit-code ERROR blames sudo for every code**
  (`c_logcenter.c:1009`): 126/127 (command not found), the non-unit default
  command, 128+N (child killed). The code is logged, so not silent; the text
  should be generic. `system(NULL)` (the attr set to null) returns 1 and is
  logged as *"killed by signal 1"*.
- **Agent spare window: a reused pid holds its yuno down the full 5
  minutes** (`process_is_gone()`): no double launch (the guard reads the real
  process), only the delay.
- **`c_authz.c:1807`** still comments `// the list: empty`; `list-jwk`
  answers the config's `jwks` (as YUNO_AUTH.md now says).
- **Tests:**
  - none for the agent's spare window;
  - none for the dbsimple `..` case, the 40-link limit, or a third user met
    through a link target;
  - `secret_attrs` proves nothing when the suite runs as real root (the
    file is root's);
  - its `__wrap_stat` red case needs glibc ≥ 2.33;
  - `test_fs_watcher_overflow`'s subtree is one level deep, so the
    breadth-first order is untested;
  - still none for the C_UDP client, the websocket default max, or a frame
    of exactly `max-1`.
- **wattyzer run** (the second machine of the release rule): not done.

## Fix order

1, 2, then the dbsimple `..`, the `jn_paths_wd` rename, the `--stop`
details, and the rest.
