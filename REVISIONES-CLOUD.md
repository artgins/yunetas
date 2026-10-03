# Cloud review of main

Reviewed up to `0db20a63d` (2026-10-03): the answer to the review of
`a8e6dd30d`. What is resolved is removed from this file; this version is the
fixing session's answer to the review of `0db20a63d` (`2f0b63238`), item by
item. Every fix has a test that fails on the code before it, except where it
says otherwise.

The review's own check, on `0db20a63d`: suite 287/287 as user `yuneta`
under `ulimit -Sn 1024`. On wattyzer, on `0db20a63d`: suite 287/287 (a
worktree of its own, `init/build --sdk-only`, the agent binaries put back,
the worktree removed). This answer: clean build (`yunetas clean/build
--sdk-only`) with no warning, suite 287/287 under `ulimit -Sn 1024` on the
dev machine.

## Resolved

### Medium

| # | Item | Test |
|---|------|------|
| 1 | Agent spare window: `restart_wait` is `{yuno_id: [pids]}`. Until the 10 s the pids are watched as before; after them, ONLY the spared ids, each asked of the yuno itself (`find_living_yuno_pids()`: its role and configuration, so a reused pid does not hold it), and each launched alone (`launch_enabled_yuno()`, the body of `run_enabled_yunos()`, now shared) when no process of it is left. The window launches nothing else, and its end launches nothing | No ctest compiles `c_agent.c`. The live check on the local agent (a root look-alike of a stopped yuno, which `restart_nodes()` cannot kill, then `deactivate-snap`, a `kill-yuno` of another yuno inside the window, the look-alike killed) was refused by the session's permission classifier: left to the user |
| 2 | `--stop`: the list has no fixed size (`gbmem_realloc`), so nobody's look-alikes can crowd the real daemon out of it, root caller or not; the own-uid-first passes of `0db20a63d` are gone. The docs no longer claim what held only for the daemon's uid | By hand, scratch daemon on `ydaemon.c`: 70 look-alikes of the user started first, the daemon started and stopped as root: stopped, exit 0 (a list of 64 never reached it). 70 root look-alikes, the daemon as the user: stopped, exit 1 (the `EPERM`s) |

### Low

| Item | Test |
|------|------|
| dbsimple `..`: the walk returns to the user it started with (the one a symlink carried in, or none); what the directories it leaves had named is not carried. Only symlinks count toward the 40 (a `..` always walks a shorter path; counting it refused legitimate paths) | `secret_attrs`: the realm behind `test_secret_attrs.x/../test_secret_attrs.real`, `.x` the intruder's and closed: the intruder's file refused (red on `0db20a63d`: loaded); a third user's directory behind a link: refused; a chain of 40 links: trusted |
| fs_watcher: `add_watch()` drops the old path of a wd watched again at another path from `jn_paths_wd` | `test_fs_watcher_overflow` `do_test_renamed_in_overflow`: `m` renamed to `m2` during an overflow, the pass watches it again: `m` no longer indexed, `m2` at the same wd (red on `0db20a63d`: `m` still indexed) |
| fs_watcher: `queue_unwatched_subdirs()` reads the directory itself (`opendir`/`readdir`, the type `readdir` gives, `fstatat` only for `DT_UNKNOWN`): a directory gone since its watch says nothing (`ENOENT`), another failure is an ERROR without stack. The listing of one directory is still whole in its turn: the doc says so now instead of "never blocks the loop" | `test_fs_watcher_overflow`: the subtree now has two more levels under `s000`: each handed once, after its parent |
| `--stop` details: an empty `argv[0]` is a zombie when `/proc/<pid>/stat` says so, and is not taken; an `EPERM` on a process already a zombie is not a failure; a `cmdline` that cannot be read for another reason than `ENOENT`/`ESRCH` is said and makes `--stop` exit 1 (not known whether it was the daemon). The comment of `daemon_shutdown()` and the **Returns** of `daemon_launcher.md` list every -1 | By hand: a root zombie named like the daemon, `--stop` as the user: exit 0 (was 1, its `EPERM`); a script of the same name survives; a renamed binary is stopped |
| logcenter: the exit-code ERROR names no cause it cannot know (1, 126/127, 128+N are explained, not assumed); an empty `restart_yuneta_command` is an ERROR, not `system(NULL)` read as *"killed by signal 1"* | No test (no ctest runs logcenter) |
| Agent spare window, a reused pid: the window asks the yuno (item 1), not the pid | As item 1 |
| `c_authz.c`: the comment of `list-jwk` without a users treedb says it lists the config's `jwks` | -- |
| Tests: `secret_attrs` skips the trust-chain cases, saying so, when the suite runs as real root (the yuno's user would be root, its files trusted anyway); its `__wrap_stat` notes it needs glibc 2.33 | -- |

## Not done, and why

- **The live check of item 1**: see the table.
- **Still without a test of their own**: the C_UDP client, the websocket
  default max, a frame of exactly `max-1`.
