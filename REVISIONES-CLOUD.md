# Cloud review of main

Reviewed up to `dc416aa4f` (2026-10-02): the fixes to the review of TODO.md
section 1 (`62e070ec5..dc416aa4f`). What is resolved is removed from this
file. Last check: clean build with no warning; the full suite was still
running when this was written (124/283 passed so far), its result follows.

## Verdict

The fixes are good. Of the 26 items fixed, 21 are correct and complete:

| Items | Area |
|-------|------|
| 4, 5 | Commands run with `system()`, emailsender commands |
| 6, 8 | mbedTLS double decref, TLS reason in `disconnect_cause` |
| 11, 13, 14 | `dir_identity`, closing an rt_disk feed, dbsimple in-place save |
| 16, 17, 18, 20 | Agent C_COUNTERs, queue gauges, stats envelope, uptime |
| 21, 22, 23, 24, 25, 28 | Full C_TCP_S, control center, rail, `skip-email`, stale texts |

Item 19 is correct in another design than the one I proposed; that is
accepted, and its limits are listed under "Low".

Five fixes are incomplete (items 1, 2, 3, 7 and 15), and item 27 works but
has defects in its packaging. One new defect was found by the way
(`c_authz.c`). They are listed below in the order I would fix them.

Items 9a/9b and 26 stay in TODO.md, as the fixing session said.

## Open

### High

**1. `write-attr` still reaches what the C_AUTHZ check protects** (item 3).

- `jwks` (`c_authz.c:339`) and `default_role` (:349) are
  `SDF_WR|SDF_PERSIST`.
- `write-attr` (`c_yuno.c:415`) is `SDF_AUTHZ_X` only, which does nothing
  while `enable_command_authz` is off.
- So `write-attr gobj_name=authz attribute=jwks value=[...]` plants a
  trusted signing key (read at the next start), and `default_role=root`
  gives root to every user an IdP provisions.

Preferred fix: as for `cert_sync_copy_cmd`, both become `SDF_RD`, set from
the config. Keys are managed only through `add-jwk`/`remove-jwk`, which are
now checked.

**2. Role bypass through the IdP** (item 3).

- `register-idp-user role=...` (`c_idp_keycloak.c:503-537`) checks only that
  the role exists.
- `ac_idp_user_created` then links that role. A holder of
  `register-idp-user` can give `roles^root^users` to an email they control,
  with no `update` on `treedb_authzs`.

Preferred fix: `register-idp-user` with a role asks the same
`gobj_user_has_authz(..., "update")` as `update-user`; without a role, it
uses `default_role` only.

**3. A malformed log call, now reached on every refusal** (new).

- `c_authz.c:4020` reads `"username"      "%s", username,` with a comma
  missing. The key becomes `username%s` and the username is used as the
  format string.
- It runs for every external C_AUTHZ command from a user not in the treedb,
  now that `refuse_without_authz()` calls the checker. A `%` in a username
  (an email taken from a JWT) is undefined behaviour.

Preferred fix: add the comma. A test with a username holding `%s`.

**4. dbsimple: a trusted file is loaded whatever its mode** (item 2).

- `check_persist_file()` (`dbsimple.c:141-184`) trusts a root-owned file
  (or, for a root yuno, a file of the data dir's owner) and never replaces
  it.
- A file written 0664 by a release before 7.25.19 run as root stays
  group-writable in the 02775 dir. A group member edits it, and it is
  loaded at every start.
- Also, for a root yuno: the parents of `data` are 02775 too, so a group
  member can rename `data` away and make their own, with their own file;
  the owner test then passes.

Preferred fix:
- Refuse (or replace, if it is the yuno's own) a trusted file with group or
  other write bits.
- For a root yuno, take as the owner the one of the yuno's bin/realm
  directory, which only root and the agent write, not the one of `data`.

**5. Agent: packaging of the systemd units** (item 27).

- **Removing the deb leaves `yuneta_agent22` running.**
  - `prerm` (`make-yuneta-agent-deb.sh:2296-2310`) stops only
    `yuneta_agent`.
  - `postrm remove` calls `/yuneta/agent/service/remove-yuneta-service.sh`,
    a file of the package that dpkg has already deleted by then, so its
    `systemctl disable --now` never runs.
  - Result: agent22 runs from a deleted binary, its unit file is gone, and
    a `multi-user.target.wants` link is left dangling.
  - The rpm is right.
- **The unit's comment on SIGKILL is wrong.** With `KillMode=process` only
  the main process (the watcher) gets the final SIGKILL.
  - Scenario: an agent that hangs in its orderly shutdown, or a watcher
    asleep before a relaunch.
  - The agent stays alive, unsupervised in the cgroup, and the next
    `systemctl start` meets it.

Preferred fix:
- `prerm` stops and disables both units with `systemctl` (inline, not
  through a package file); `postrm` only cleans links.
- The unit comment says what happens. Either an `ExecStopPost` that signals
  a leftover agent, or `KillMode=mixed` with the yunos in their own cgroup
  (slice); otherwise state the limit.

### Medium

**6. Agent: a living yuno that never reconnects cannot be killed** (item
15).

- The fix uses a scan of `/proc` instead of `yuno.pid`. That design is
  better: no stale pid file, no reused pid.
- But the pid it finds is not written into `yuno_pid`, and `kill-yuno`
  filters `yuno_running=true` (`c_agent.c:5284`).
- A yuno alive but hung is refused by `run-yuno` ("alive but not connected:
  not launched again") and by `kill-yuno` ("not found"). The only ways out
  are `deactivate-snap` or a manual kill. Before, it was relaunched.

Also: `restart_nodes()` sends SIGKILL and calls `run_enabled_yunos()` at
once (`c_agent.c:10351`), without waiting for the processes to end. The new
instance can meet the old one's exclusive locks, which is the original
failure. This predates the fix.

Preferred fix:
- `kill-yuno` also reaches a yuno found alive by the scan (signal its pid,
  logged).
- `restart_nodes()` waits for the killed pids to go before it relaunches,
  through a timer or the child's exit notification; no busy wait.

**7. Peer-sized reservations left** (item 1).

- **C_PROT_TCP4H** (`c_prot_tcp4h.c:637`) still does
  `istream_create(frame_length, frame_length)` from the peer's 4-byte
  header, capped only by `max_pkt_size`, which defaults to the max block.
  It is the same up-front reservation that the websocket fix removed.
- **Off-by-one in C_WEBSOCKET.**
  - The guard is `frame_length > max_payload` (`c_websocket.c:1946`) with
    the buffer `istream_create(MIN(len, 4K), max_payload)`.
  - A gbuffer holds at most `max - 1` bytes (`gbuffer.c:134`), so a frame
    of exactly `max_payload_size` passes the guard and never completes
    ("NOT ENOUGH SPACE").

Preferred fix:
- tcp4h: the same pattern as the websocket (small initial size, max =
  frame length), and a smaller default for a server side with no session.
- websocket: `>=`, or a buffer max of `max_payload + 1`.

**8. C_TCP: paths left after the -2222 fix** (item 7). Rare; found by
reading.

- **`set_secure_connected()`** (`c_tcp.c:835-836`) calls
  `set_inactivity_timeout()` after `start_pending_writes()`, even when the
  write returned -2222 or after a stop. That can be freed priv.
- **A drop on `EV_CONNECTED`** (:805-807) with no write in flight stops
  synchronously. `set_disconnected()` clears `priv->sskt`, and the
  `ytls_flush(priv->ytls, 0)` that follows dereferences it in both
  backends.
- **A failed `write_encrypted_data()`** inside the encrypt callback can run
  `set_disconnected()` synchronously; `encrypt_data`/`write_data` then go on
  with the freed sskt and gbuffer.
- **OpenSSL leak** (predates): `flush_encrypted_data()` does not release
  its gbuffer when `BIO_read` returns 0 or less (`openssl.c:1259-1281`).
- **A stalled read** (predates): a subscriber answering -1 makes
  `decrypt_data` return -1, and C_TCP (`c_tcp.c:1700-1711`) breaks without
  re-arming the read and without a stop. The connection hangs in silence.

Preferred fix: one rule for C_TCP. After any call that can publish or stop
(subscriber callbacks, `write_encrypted_data`), check that the gobj and the
sskt are alive before touching them — the alive marker that
`flush_clear_data` already has, extended to the encrypt side and
`set_secure_connected`. Free the sskt in `try_to_stop_yevents()` once the
end is decided (the follow-up of the review, not done).

**9. CHANGELOG: operator-visible changes not said** (items 3 and 5).

- **-403 on the C_AUTHZ commands, jwk commands included, where nothing
  answered it before:**
  - on every yuno with no users treedb of its own (logcenter, auth_bff,
    sgateway, watchfs, dba_postgres), where `priv->gobj_treedb` is NULL;
  - through `command-yuno`, which forwards `__username__`, so the child
    checks the operator against its own treedb.
- **The new `SDF_AUTHZ_X` flags do nothing by default** (no yuno enables
  `enable_command_authz`), while the CHANGELOG lists them under SECURITY.
- **The doc example of a role** in `YUNO_AUTH.md` has no `realm_id`, which
  `treedb_schema_authzs.c` requires; as written it is refused or grants
  nothing.

Preferred fix:
- A C_AUTHZ with no treedb keeps answering the jwk commands, which never
  used it, and says clearly that user management is not here.
- The upgrade notes say the rest.
- The example carries `realm_id`.

### Low

- **`__reset__` does not zero `noChannelConnxs` nor `refusedConnxs`**
  (item 21). `c_tcp_s.c` `mt_writing` (:256) does not map them back to
  priv, and `mt_reading` keeps answering the old value.
- **Raising the soft open-files limit in every yuno** (item 10):
  - Every process it forks inherits the raised limit, and the daemon's close
    loop (`entry_point.c:435-443`) runs up to it: 1-4 million `close()`
    calls per yuno launched by an agent started by hand.
  - It also takes the `ulimit -Sn 1024` axis of the release suite away from
    every yuno-based test.
  - Preferred fix: close with `close_range(3, ~0U, 0)` (or walk
    `/proc/self/fd`); keep a test that runs a yuno under a low limit with
    `limit_open_files` set.
- **The fs_watcher warning at half the limit counts per watcher** (item
  10), while `EMFILE` is per process: four followers with 400 descriptors
  each, under 1024, never warn. Count against the process (`/proc/self/fd`
  or a process-wide counter).
- **fs_watcher: a FIONREAD failure still pads** (item 12). Inside a batch
  the end can be short, and on a quiet watcher `pad_check_callback` returns
  without re-arming, so the end is never closed. The review asked for
  `FS_WATCHER_GONE`.
- **Control center: pause then play in one loop turn** (items 22/23).
  `clear_timeout0()` leaves the timer's yev `CANCELING`, and
  `start_rates_tick()` in the same turn logs "cannot start timer: is
  CANCELING"; the tick stays off until the next play. Found by reading.
- **TLS:**
  - An mbedTLS `close_notify` from the peer still reads "TLS: decrypt
    failed" with no reason (`mbedtls.c:1337-1355`).
  - The "WANT stall" and "handshake PENDING" exits give a cause with no
    reason.
- **Rates (item 19, accepted design):**
  - The window is as wide as the reader's interval: a reader every 60 s
    averages over 60 s.
  - The maxima move only when a window closes.
  - Messages before the first read count in no rate.
- **`info-uptime`:** the error answer uses `strerror(errno)` after
  `gobj_log_error`, which may have changed `errno`.
- **timeranger2 9c/9d:**
  - The NULL-body skip also drops the record for metadata-only audiences.
  - ENOENT as a warning also covers a master's own read, where it means
    corruption.
- **rt_disk close** (item 13):
  - The first `rmrdir` that loses the race logs an ERROR even when the retry
    succeeds.
  - A `.closing.<pid>.*` leftover whose pid is reused is not cleaned while
    that pid lives.
- **The agent's `--stop` gives the agent 1 s before SIGKILL** (predates),
  and the postinst uses it for the SysV-to-systemd transition.
- **`--pid-file` without `--start`** is ignored with nothing said
  (`entry_point.c:419`).
- **No red test for:**
  - item 6 (mbedTLS in `ST_WAIT_STOPPED`);
  - item 15 (a manual check with SIGSTOP);
  - item 16;
  - the root-yuno branch of item 2;
  - "has `create` but not `update`, with a role" in item 3.

## Order I would fix them in

1. **Security:**
   - the missing comma (3);
   - `jwks`/`default_role` config-only (1);
   - the IdP role (2);
   - the dbsimple mode check (4).
2. **The deb removal of the agents** (5), before the units reach a node
   through a package.
3. **`kill-yuno` of a living yuno, and the wait in `restart_nodes()`** (6).
4. **The tcp4h reservation and the websocket off-by-one** (7).
5. **The C_TCP liveness rule** (8), with a test per path.
6. **The CHANGELOG notes** (9); the low items when their area is touched
   again.
