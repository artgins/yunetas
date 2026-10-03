# Cloud review of main

Reviewed up to `a8e6dd30d` (2026-10-02): the answer to the review of
7.25.22 (`da85b7a9b..a8e6dd30d`). What is resolved is removed from this file;
this version is the fixing session's answer, item by item. Every fix has a
test that fails on the code before it, except where it says otherwise.

The review's own check, on `a8e6dd30d`: suite 287/287 as user `yuneta`
under `ulimit -Sn 1024`. Last check of this answer: clean build (`yunetas clean/build --sdk-only`) with no warning,
suite 287/287 under `ulimit -Sn 1024` on the dev machine.

## Resolved

### Medium

| # | Item | Test |
|---|------|------|
| 1 | C_TCP: `set_secure_connected()` closes only on a TLS error (`ret < -1000`), as the decrypt path; `ytls_flush()`'s contract says the -1 | No red test: the first data must arrive in the read that ends the handshake, a timing of the loopback (as test8's TLS path) |
| 2 | `--stop` takes the processes of the name STARTED as it: the base name of `argv[0]` in `/proc/<pid>/cmdline`, readable whoever owns the process. A script's `argv[0]` is its interpreter (`/bin/sh`), so the SysV script is not taken; another user's agent is, and its `EPERM` exits 1; a renamed or replaced binary is still taken (the watcher and its child fork, they do not exec). Not the inode the review proposed: the `--stop` of a deploy is run by the NEW binary, whose inode is not the running one's either | By hand, a scratch daemon built on `ydaemon.c`: a shell script of the same name survives its `--stop` (exit 0, daemon gone); the binary renamed to `*.bak-pre-x` and a new one put in place, the new one's `--stop` stops the old daemon (exit 0); the daemon run as root, `--stop` as the user: `EPERM` said, exit 1. And, from the security review of the push: `comm` and `argv[0]` are the starter's choice, so the caller's own processes are collected first and a list overflow fails `--stop`; 70 root look-alikes started before the daemon: it is stopped, exit 1 (red on `bbff25b3b`: the daemon left up) |
| 3 | fs_watcher: the re-watch is breadth first through `jn_unwatched`: a directory watched again queues its subdirectories not watched, tried in the same batch while there is room, parent first. Every try counts against the batch's 64, an `ENOSPC`/`ENOMEM` ends them, and the lookup is `jn_paths_wd` (path → wd), now kept for every watcher. *"Directories watched again"* is said at the end of the batch where none is left (`unwatched_said`), not while a subtree is still queued; and a path watched meanwhile is not handed twice | `test_fs_watcher_overflow` `do_test_unwatched_subtree_bound`: 200 subdirectories made under an unwatched `a`; `a` watchable, its children not: 3 `inotify_add_watch` calls (red: 202, and the ERROR said twice); then each child handed once, parent first, at most 64 per batch |
| 4 | dbsimple: a symlink met inside the closed chain is followed (only root or the chain's user can have put it there); its target is walked from `/` again under the same rules, the user named so far still holding; `..` is the parent of the directories walked; 40 links at most. A third user's closed directory ends the chain (enforced; with the user carried across links, it changes no answer, so no test of its own). The plant's red case is committed: `__wrap_stat` tells the directories as `__wrap_fstat` does | `secret_attrs`: the realm behind `test_secret_attrs -> ../tmp/test_secret_attrs.real` is trusted (red on `a8e6dd30d`: refused); the plant under the 02775 parent (red on `da85b7a9b~1` now: loaded) |

### Low

| Item | Test |
|------|------|
| Deprecated `C_PROT_MQTT`: a remaining length above the max block less one is refused at its header (*"Mqtt packet too large"*, a warning, the connection closed), as tcp4h and the websocket | No test (the deprecated gclass has none of its own) |
| OpenSSL `encrypt_data()`: `want_retries` back to 0 after a write that progressed, as mbedTLS | No test (a WANT stall is not staged) |
| logcenter: `restart_yuneta_command` through one helper that says its end: `system()` that could not run, killed by a signal, or an exit code other than 0 (sudo refused, systemctl failed), each an ERROR. It still waits for the command: under the units that is the agent's restart, bounded by the units' timeouts, and the refusal of sudo comes at once. Running it detached would lose the exit code the review asked to log | No test (no ctest runs logcenter) |
| `restart_nodes()`: the pids left after the 10 s are looked at every second for 5 minutes; each yuno is launched when its process is gone, and the last warning says to `run-yuno` them. A new restart resets it | No test (no ctest compiles `c_agent.c`) |
| ExecStopPost: `if kill; then logger killed; else logger kill FAILED; fi` | The shell of both units checked with `sh -n` |
| `close_range`: `SYS_close_range` given as 436 when the headers lack it, on the architectures built (the number is the same on all of them since 5.9's unified table) | Compiles |
| Docs: `list-jwk` of a C_AUTHZ without a users treedb answers the config's `jwks`, which it never uses (YUNO_AUTH.md, CHANGELOG) | -- |

## Not done, and why

- **"No test for"** list: the ones above say why each has none. The C_UDP
  client, the websocket default max and a frame of exactly `max-1`
  completing are still without a test of their own.
- **wattyzer run** (the second machine of the release rule): not done in
  this session.
