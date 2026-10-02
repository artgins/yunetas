# Cloud review of main

Reviewed up to `a8e6dd30d` (2026-10-02): the answer to the review of
7.25.22 (`da85b7a9b..a8e6dd30d`). What is resolved is removed from this file.
Clean build with no warning; the full suite was still running when this was
written, its result follows in the next commit.

## Verdict

The eight open items are fixed in what they asked:

- the dbsimple trust chain planted by a group member;
- `write-attr` of `SDF_PERSIST`-only attrs;
- the rpm SELinux requirement;
- the UDP read stall;
- the frame of exactly the default max;
- `--stop`;
- C_TIMER0 ending the loop;
- the restarts outside the unit.

So are the low items, except the ones below.

Three fixes bring a defect of their own: `--stop` in `a8e6dd30d`, the
fs_watcher re-watch in `1d2083dc3`, and the dbsimple walk in `da85b7a9b`.
One path of the -1 contract was left in C_TCP.

## Open

### Medium

**1. C_TCP: a subscriber's -1 still closes a TLS connection on the flush
after the handshake** (`4f64eaaa8`).

`set_secure_connected()` (`c_tcp.c:893-914`) takes any negative answer of
`ytls_flush()` as a TLS error: *"TLS: the flush of clear data failed"*, then
`try_to_stop_yevents()`.

`ytls_flush` → `flush_clear_data` → `on_clear_data_cb` publishes
`EV_RX_DATA`, and since `4f64eaaa8` a subscriber's -1 comes back as -1. So a
subscriber that answers -1 to the first application data arriving with the
handshake closes the connection. That is the contract `ytls.h` now says it
does not do, and the decrypt path already handles the same case
(`ret < -1000`).

Preferred fix: `if(ret < -1000)` there too, as on the decrypt path.

**2. `--stop` exits 0 again when it cannot stop the agent** (`a8e6dd30d`
undoes part of `603fde26d`).

`collect_proc()` (`ydaemon.c:440-465`) reads `/proc/<pid>/exe`. That fails
with EACCES for another user's process, so the process is "left alone" with a
line on stderr and nothing marks a failure: `--stop` exits 0 and the agent
stays up. This is item 6a of the previous review in another form, and the
EPERM path of `603fde26d` is now practically unreachable.

Two more effects:

- Run as `yuneta` from the root init script, `--stop` prints that line for
  the script itself on every non-unit `stop` and in `_start_unit()`.
- An agent whose binary was renamed (a `mv` to `*.bak-pre-<version>` before
  the new one is put in place) has an `exe` that follows the rename. It reads
  as "another binary of the same name" and is skipped, and `--stop` exits 0.

Preferred fix:

- Take a process whose exe cannot be read as a failure of `--stop` (exit 1,
  said) unless its uid shows it is not a yuneta process. For the init script,
  compare the `exe` with the shell's, or skip by uid 0 when the caller is not
  root.
- Match a renamed binary by inode (`stat` of `/proc/pid/exe` against the
  file), not by path.

**3. fs_watcher: the subtree re-watch has no bound** (`1d2083dc3`).

`rewatch_subtree_cb()` (`fs_watcher.c:935-961`) has three problems:

- It answers TRUE when `add_watch()` fails with ENOSPC/ENOMEM, so the walk
  goes on trying every remaining directory, each one failing.
- It runs synchronously at a batch end, bypassing the 64-per-batch cap of the
  retries.
- For each directory it scans all of `jn_tracked_paths` by value: O(subtree ×
  tracked).

With 100 000 key directories, at exactly the moment watches ran out, that
blocks the loop for a long time.

Preferred fix:

- Stop the walk on ENOSPC/ENOMEM (FALSE) and leave the rest to the next
  retry.
- Count the walk against the per-batch cap.
- Keep a path → wd index beside `jn_tracked_paths`.

**4. dbsimple: a symlinked parent makes a root yuno lose its attrs**
(`da85b7a9b`).

The walk from `/` down uses `O_NOFOLLOW`, so it stops at a symlink, for
example `/yuneta -> /srv/yuneta`, and trusts nobody. A root yuno then refuses
its own yuneta-owned file and refuses every save. The old `stat()` walk
followed the link. No layout of this repo symlinks `/yuneta`, so this is
conditional.

Also, *"each owned by root or by that user"* (in the comment, the commit and
the answer) is not enforced (`dbsimple.c:160-163`). Once a non-root owner is
taken, a lower closed directory of a third user continues the chain. The
impact is low: that user can only move existing files.

The committed test does not fail on the old code for the planted case. The
old walk used `stat()`, which the test's `__wrap_fstat` does not cover; the
red came from a temporary `__wrap_stat` that was not committed.

Preferred fix:

- Follow a symlink only when it and its target are owned by root (or resolve
  the data directory once with `realpath()` and walk the result).
- Enforce the owner rule as written.
- Commit the red case for the plant.

### Low

- **Deprecated `C_PROT_MQTT`:** with `istream_consume()` now checking the
  append (`769af7b41`), a PUBLISH at or above the max block never completes.
  Before, it was delivered truncated. Every later chunk logs an ERROR with a
  stack, and no payload timeout ends it. A peer can do it. Cap the remaining
  length, as tcp4h and websocket now do.
- **OpenSSL `encrypt_data`** never resets `want_retries` after a write that
  made progress (mbedTLS does): six WANTs spread over a long write abort it.
- **logcenter:** a refused or failed `restart-yuneta` (sudo refused, or
  systemctl failed) is not logged (`c_logcenter.c:957-977` checks only
  `ret < 0`). Its loop is now blocked in `system()` up to the units' stop and
  start timeouts (60 s), where it used to be about 1 s.
- **`restart_nodes()`:** a yuno spared after its 10 s (D state, SIGKILLed,
  certain to die) is never launched again once it dies. Re-arm its launch
  when it goes, or list it in the answer.
- **ExecStopPost:** `kill -KILL $p && logger` logs nothing when the kill
  fails.
- **`close_range`:** `SYS_close_range` comes from the kernel headers; with
  headers older than 5.9 it compiles but always takes the slow loop
  (`#define __NR_close_range 436` as a fallback).
- **No test** for the C_UDP client, the websocket default max, a frame of
  exactly `max-1` completing, the ytls -1 and WANT-stall paths, `--stop`,
  `restart-yuneta`, the init script and ExecStopPost.
- **Docs:** `list-jwk` "(empty)" for a C_AUTHZ without a treedb is wrong when
  the config sets `jwks`: it lists keys that are never validated.

## Order I would fix them in

1. The C_TCP flush path (1): one line, the same rule as the decrypt path.
2. `--stop` (2).
3. The fs_watcher re-watch (3).
4. The dbsimple symlinked parent (4), and its test.
5. The low items as their area is touched.
