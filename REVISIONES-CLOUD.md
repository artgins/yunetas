# Cloud review of main

Reviewed up to `62e070ec5` (2026-10-02). What is resolved is removed from
this file. Last check of the code: clean build with no warning, suite
278/278 as user `yuneta` under `ulimit -Sn 1024`.

## Acted on (2026-10-02)

Every item was checked against the code before acting (TODO.md §1 carries
the corrections). Fixed, see `CHANGELOG.md` "Unreleased":

- **6** (mbedTLS double decref) and **7** (C_TCP -2222). The fix differs
  from the one proposed: its point 3 (`try_to_stop_yevents()` idempotent in
  `ST_DISCONNECTED`) would hang the stop of an idle clisrv of the new method
  and of a disconnected client, and was not done. The defect is wider than
  item 7 says: `EV_STOPPED` is where a host destroys a VOLATILE C_TCP, and
  C_TCP used itself after it in `set_disconnected()` (its url, and a
  client's `idle_closed`) and in `mt_stop()`. Reach: only a clisrv of the
  legacy method in a channel with no C_TCP (sgateway); the agent's input
  pre-creates its C_TCP. Test `c_tcps/test7`.
- **15** (agent relaunch of a living yuno). Not by `yuno.pid` (a yuno writes
  it where its own config says) nor by `/proc/<pid>/exe` (it names the
  release): by `/proc`, argv[0] and the configuration files in its `bin/`.
  `yuno_pid` is not written (the guard of `ac_on_open()` would then kill the
  yuno when it reconnects). `restart_nodes()` kills such a yuno.
- **13** (rt_disk close races the master). `IN_MOVED_FROM` was not in
  fs_watcher's mask, `rt_id_is_confined()` did not refuse a leading dot,
  `walk_dir_tree()` skips dot names, and the master's `FS_SUBDIR_DELETED` of
  the renamed directory would have logged an ERROR: all handled. Test
  `test_delete_key_propagation` (`close_races_master`).

## Review of TODO.md, section 1 "Defects to fix"

Each item was read against the code of main. Line numbers are those of
`62e070ec5`. Verdicts:

- **TRUE**: the defect is there as described.
- **PARTLY**: the defect is there, but some of the text is wrong (said
  below).
- **STALE**: already fixed, or the premise is wrong.

**Summary**

- 34 items: 32 are true or partly true, and the 2 of other projects cannot
  be checked here. None is wholly stale.
- Four are wider than the TODO says: C_AUTHZ, the dbsimple load, the C_TCP
  -2222 paths and the rates of C_IOGATE.
- Five details of the TODO are wrong; they are listed under
  [Corrections to TODO.md](#corrections-to-todomd).
- The order I would fix them in is at the end.

### Security

**1. A frame before the session is parsed whole** — TRUE, medium.
- `c_ievent_srv.c:2053`: `ac_on_message()` parses the whole frame
  (`iev_create_from_gbuffer()` → `gbuf2json()`).
- The check that only `EV_IDENTITY_CARD`/`EV_GOODBYE` may come before the
  session is later, at :2118-2149.
- Not in the TODO: `c_websocket.c:1920-1936` reserves
  `istream_create(frame_length, frame_length)` from the length the peer
  writes in the frame header, before any payload arrives. About ten
  connections that each claim 200 MB reach the agent's 2 GB ceiling with a
  few bytes each.
- **Preferred fix:**
  - In `ac_on_message()`, before the parse: if the state is not
    `ST_SESSION` and `gbuffer_leftbytes() > max_pre_session_frame` (a new
    `SDF_RD` attr, default 1 MB; a card with a JWT is a few KB), then
    `gobj_log_warning` (`MSGSET_PROTOCOL`, peername, size, capped dump),
    drop, `return -1`.
  - In `c_websocket`: grow the payload istream as data arrives (small
    initial size, max = `frame_length`), or cap `frame_length` while the
    channel has no session.

**2. dbsimple: a root save hands the secrets to whoever planted the file**
— TRUE, and the LOAD is worse. High.
- **Save:** `dbsimple.c:550-575`. The data dirs are 02775
  (`entry_point.c:513`), and a root save gives the new 0600 file to the old
  file's owner with `fchown`.
- **Load (not in the TODO):** `check_persist_file()` (:105-127) accepts a
  file of another user with a warning only, and `load_json()` loads it. A
  member of the `yuneta` group can therefore write the persistent attrs of
  any yuno.
- **Chain from a group member to code run as `yuneta`:**
  - The agent's `cert_sync_copy_cmd` is `SDF_PERSIST` and goes to
    `system()` (item 4).
  - `restart_yuneta_command` of logcenter is the same.
- **Preferred fix:**
  1. **Load:** refuse a file whose owner is neither the euid nor the owner
     of the data directory. Log an ERROR and set `*failed = TRUE`, so the
     saves are refused too. This uses the fail-closed path that exists.
  2. **Save:** use `fchown` only when the old owner is the directory's
     owner. Otherwise keep the file as the euid's, with a warning.
  3. Make the two contradictory log lines agree.

**3. User management open while `enable_command_authz` is off** — TRUE,
wider. High.
- **All of C_AUTHZ is open.** Every command of `c_authz.c:279-296` is
  `SDF_AUTHZ_X` only, and the gate defaults to off (`c_yuno.c:520`). This
  covers `update-user`, `set-user-pwd`, `set-max-sessions` and, worst,
  `add-jwk` / `remove-jwk`: adding a trusted signing key lets the caller
  forge JWTs.
- **Role bypass.** `create-user` / `update-user role=roles^X^users` link the
  role through `gobj_update_node()`, with no check (`ac_create_user` :4245,
  `role_ref_is_linkable()` only checks that the role exists). So the role
  link that `link-nodes` refused is reachable this way.
- **Preferred fix:** an always-on check like C_NODE's
  `refuse_without_authz()`, with C_NODE's own permissions on the authz
  treedb:
  - `create` / `update` / `delete` for the write commands, `-403` when
    refused;
  - `read` for the read commands.
  - Internal calls with no principal keep working.

  No new role model is needed. Enabling `enable_command_authz` is the other,
  general fix (items 4 and 5 and every agent command); it should come after
  this one.

**4. `write-attr` reaches a command run with `system()`** — TRUE, medium.
- **The path:**
  - `c_yuno.c:414` `write-attr` writes any `SDF_WR` attr and persists it.
  - `c_agent.c:987-990` `cert_sync_copy_cmd` is `SDF_WR|SDF_PERSIST`.
  - `cert_sync_tick` (:10253-10267) runs it with `system()`.
  - `cert-sync-now` (:865) has flag 0 and fires it at once.
- **Same pattern:** logcenter's `restart_yuneta_command`
  (`c_logcenter.c:127`, `system()` at :956/:968).
- **Preferred fix (the TODO's, completed):**
  - `cert_sync_copy_cmd`, `cert_sync_store_dir` and
    `restart_yuneta_command` become `SDF_RD`, config only, without
    `SDF_PERSIST` either, so a planted file cannot set them (item 2).
  - `cert-sync-now` gets `SDF_AUTHZ_X`.
  - CHANGELOG note: a value persisted at run time is no longer read; move
    it to the config.

**5. `skip-email` has no `SDF_AUTHZ_X`** — TRUE, incomplete. Low.
- `remove-emails-failed`, `disable-alarm-emails` and
  `enable-alarm-emails` (`c_emailsender.c:106-110`) have no flag either.
- **Preferred fix:** give the four of them `SDF_AUTHZ_X`. Leave
  `send-email`, `help` and `list-queues` open.

### TLS and transport

**6. mbedTLS: double decref after a failed output callback** — TRUE,
medium.
- **Who owns the gbuffer.** By the contract of `ytls.h:95-98`, the callback
  owns it. C_TCP's callback hands it to the kw of
  `EV_SEND_ENCRYPTED_DATA`, and every negative answer of
  `gobj_send_event()` has already released that kw.
- **The second release.** `mbedtls.c:1136` then decrefs the gbuffer again.
- **When it happens.** C_TCP in `ST_WAIT_STOPPED` with its sskt still alive
  (a drop from a subscriber while a write is in flight), followed by more
  TLS output in the same ytls call.
- **Preferred fix:**
  - Remove that `gbuffer_decref` (keep the log and the -1).
  - Make OpenSSL log a negative answer as well.
  - Write in `ytls.h`: "owned by the callback, whatever it answers".
  - Do NOT add a no-op `EV_SEND_ENCRYPTED_DATA` to `ST_WAIT_STOPPED`.
  - Follow-up, at its layer: free the sskt in `try_to_stop_yevents()` once
    the end is decided.

**7. C_TCP: the -2222 branch** — TRUE, wider. **Medium (the TODO says
low).**
- **Decrypt path, gobj destroyed.** `c_tcp.c:1665` tests `ret < -1000`,
  which matches -2222, and touches priv after the gobj may be gone.
- **Decrypt path, gobj alive (the common case).** A subscriber's drop has
  run `set_disconnected()` inline. On a new-method clisrv this re-armed the
  `yev_accept`. The second `try_to_stop_yevents()` cancels it again and
  goes to `ST_WAIT_STOPPED`. Its guard checks only `ST_STOPPED`, so it is
  not idempotent, although it is declared so.
- **Encrypt path.** In the WANT_* branch the backends answer -1, not -2222.
  `write_data()` then calls `gbuffer_leftbytes()` on the gbuffer that
  `set_disconnected()` already released: a use-after-free.
- **Preferred fix:**
  1. `if(ret == -2222) { break; }` before `ret < -1000`.
  2. Both backends answer -2222 in the encrypt path too, and
     `write_data()` returns on it before any log, cause or gbuffer use.
  3. Make `try_to_stop_yevents()` idempotent in `ST_DISCONNECTED` as well.
  4. A test of a drop from an `EV_RX_DATA` subscriber on a TLS clisrv.

**8. `disconnect_cause` with the backend's reason on OpenSSL only** —
TRUE, low.
- mbedTLS never writes `sskt->last_error`.
- **Preferred fix:**
  - mbedTLS: a `set_last_error(sskt, ret)` (`mbedtls_strerror`) at each
    failure (handshake, read, write).
  - "cannot create the secure filter": drop the `: %s`, since there is no
    sskt yet and the ERROR above already says why.
  - The flush cause: add `tls_last_error()`.
  - `set_disconnect_cause()`: check what `vsnprintf` answers, warn on
    truncation, and make the buffer large enough for a 256-byte reason plus
    its prefix.

### timeranger2 and fs_watcher

**9. Notes beside the reborn-key defect** — low. One verdict per note:
- **(a) A second delete taken as "same" below its mark: TRUE.**
  `count_key_delete_heard()` (`timeranger2.c:7835-7856`).
- **(b) A feed opened late takes a live key out of the cache: TRUE.** The
  new feed's `watched_from` entry is deleted when it opens (:7973), so it
  owes nothing and clears the cache (:7708-7719). Item 13 makes the path
  reachable.
- **(c) NULL body: PARTLY.**
  - Gone on the descriptor path (`keys_are_the_life_of()`).
  - Still there on the by-path fallback: `ENOTSUP`, without `/proc`, or a
    descriptor that could not be opened.
- **(d) CRITICAL that kills the tranger: STALE for the kill.** No
  `gobj_log_critical` on these paths, and the reads log with opt 0. They
  are still CRITICAL for a race that is legitimate on the fallback: noise.
- **Preferred fix:**
  - (a)/(b): each in-doubt mark keeps the id of the feed that heard first,
    and a feed just opened checks the deletes of the last window before it
    takes a delete as heard first. A test first.
  - (c): on the fallback, a NULL content is skipped with a debug line (its
    delete is queued), not handed to the callback.
  - (d): ENOENT down to a warning.

**10. A follower out of descriptors or of watches** — TRUE, medium.
- **Descriptors.** One per live key directory (`fs_watcher.c:869/960`),
  outside the EMFILE recovery of `get_topic_rd_fd()` (:3497-3511).
- **Limits.**
  - A yuno does NOT raise its limit by default: `limit_open_files` is
    `"0"` (`c_yuno.c:538`). The TODO's "a yuno raises its limit" is wrong.
  - `tr2list` raises its own (200000).
  - The yunovatios stress case (30000 keys) is above a soft limit of 1024.
- **A watch that cannot be made** (`ENOSPC`) is taken for a directory gone
  (`place_new_key_dir_scans()`, :8263). Its links are never read, and
  nothing brings it back except an unrelated overflow pass.
- **Preferred fix:**
  1. A follower that creates an `FS_FLAG_DIR_FDS` watcher raises its soft
     `RLIMIT_NOFILE` to the hard one, logged at info.
  2. A warning, once per watcher, past 80% of the limit.
  3. A watch that cannot be made is not a directory gone: its own wd (-2,
     with the errno). The placement reads that directory by path, and the
     ERROR says that later links are not heard until the next pass.
  4. Rate-limit the per-key "watched by its path" ERROR.
  5. No timer to retry: that would be polling.

**11. `dir_identity()` fails in silence** — TRUE, medium-low.
- `timeranger2.c:8033-8035`: any `statx` failure answers FALSE with no
  log, and both callers take the directory for gone.
- Not in the TODO: `statx` → `ENOSYS` (seccomp in containers) skips every
  scan of the fallback, in silence.
- **Preferred fix:**
  - ENOENT and ENOTDIR stay silent.
  - Any other errno: ERROR once per class, with the path.
  - `ENOSYS`: fall back to `lstat()` (inode only, birth -1, which reuses
    the warning of a filesystem with no birth time).

**12. A deferred scan waits without limit on a quiet feed** — TRUE, low.
- **The over-estimate.** `fs_queued_events_end()` adds `READ_SIZE` on its
  three fallbacks (`fs_watcher.c:349/357/371`).
- **The only real case.** The CQ ring overflowing after the burst that
  caused the `IN_Q_OVERFLOW`, after which the feed goes quiet.
- **Preferred fix:** the watcher closes its own over-estimate.
  - It keeps `pad_end`.
  - At the next read delivered, or once the loop has flushed the overflow
    with no completion waiting, it moves the offset to
    `max(offset, pad_end)` and emits `FS_BATCH_END`.
  - A FIONREAD failure goes to `FS_WATCHER_GONE`, not to padding.

**13. Closing a reader's rt_disk feed races the master** — TRUE. **High.**
- **The open question, answered from the code: the master links into the
  abandoned directory for ever.**
- **The close ignores its failure.** `tranger2_close_rt_disk()` ignores
  the return of `rmrdir()` (`timeranger2.c:6121`). `remove_tree_walk`
  stops at the first failure, and under load the master has made a key
  directory again in the meantime.
- **The master never closes its side.** It closes its `rt_mem` only on
  `FS_SUBDIR_DELETED` of `disks/<rt_id>` (:6497) or on an overflow pass
  that finds the directory gone (:6336). Neither comes, so
  `master_to_update_client_load_record_callback` goes on with mkdir +
  `link()` for every new key and file.
- **It survives a restart.** `find_rt_disk` at topic open (:2133) opens a
  feed for every directory under `disks/`.
- **The only clean.** Each deleted key is mirrored into every
  `disks/*/<key>`. Nothing removes the directory.
- **Cost.** A link and a directory per key, growing with each rotation of
  files, plus the master's CPU per append. It also feeds item 9(b).
- **Preferred fix: detach atomically, then remove.**
  1. `rename(disks/<rt_id>, disks/.closing.<rt_id>.<pid>)`, then `rmrdir`
     of the renamed tree. The master builds its links from `disk_path`, so
     it can no longer reach it.
  2. The master's `disks/` watch also takes `IN_MOVED_FROM` of a directory
     as `FS_SUBDIR_DELETED`.
  3. `find_rt_disk_cb` skips names beginning with `.`, and removes the
     `.closing.*` that a crash left (`rt_id_is_confined` already forbids a
     leading dot).
  4. Check the return of `rmrdir` and log what stays.

  A retry of `rmrdir` does not converge at 3000 records/s.

### dbsimple

**14. In-place save: a short write leaves an unparsable file** — TRUE in
the code and the docs. STALE for the CHANGELOG. Low.
- **The code.** `save_json_in_place()` (`dbsimple.c:375-468`) makes one
  `pwrite` with no retry, and the code comment (:404-410) claims "no ENOSPC
  half way".
- **The docs.** `dbsimple.md:85-87` and `guide_sdata.md` ~202 overclaim
  too.
- **The CHANGELOG** (:385-390) already qualifies it.
- **Preferred fix:**
  - `pread` the old bytes first; the file is small.
  - Loop `pwrite` over short returns, so the real errno is said.
  - On a failure, write the old bytes back, `ftruncate` to the old size,
    `fsync`. Without copy-on-write this always works. With copy-on-write,
    an ERROR that says the file is left unparsable.
  - Correct the comment and the two docs.

### Agent

**15. A yuno that lost its channel is launched again while it lives** —
TRUE, the mechanism PARTLY. **High.**
- **Where the second launch comes from.** Not from `ac_on_close` (it only
  logs and clears `yuno_running` / `yuno_pid`). It comes from the sweeps
  that trust `yuno_running` alone: `run_enabled_yunos()` (:9556) and
  `run_util_yunos()` (:9614) at the agent's boot, `restart_nodes()`
  (:10148) and `cmd_run_yuno` (:5092).
- **Why the agent cannot know.** `yuno_running` and `yuno_pid` are not
  persistent, so after a restart of the agent every yuno is "not running,
  pid 0". `yuno.pid` in the yuno's bin directory (`c_yuno.c:931`) is the
  only source.
- **Preferred fix:** a `get_yuno_live_pid()`, used at the three launch
  points.
  - It reads `yuno.pid`, checks `kill(pid, 0)`, and checks that
    `/proc/<pid>/exe` is this yuno's binary (against a reused pid).
  - Alive: do not launch, warn "alive but not registered, not launched
    again", and write the pid into `yuno_pid` so `kill-yuno` can reach it
    (today it answers "yuno PID NULL").
  - No timer: a live yuno reconnects by itself.
- In passing: `save_pid_in_file()` (`c_yuno.c:5199`) does not check
  `fopen()`.

**16. A C_COUNTER still running when the agent stops** — TRUE, low.
- **Preferred fix:** in the agent's `mt_stop`,
  `gobj_match_children(gobj, {"__gclass_name__": C_COUNTER})`, then
  `gobj_stop()` + `gobj_destroy()` for each one.
  - `gobj_stop()` alone does not destroy it.
  - Do not force `publish_finalcount()`: it would answer over channels that
    are closing.

### Gates, queues and stats

**17. A persistent link's queue size cannot be read** — TRUE, medium.
- `build_stats()` reads only the `SDF_*STATS` attrs (`stats_parser.c`),
  never `mt_stats`. C_QIOGATE has none.
- **Preferred fix (the TODO's second option):**
  - `msgs_in_queue` and `pending_acks` as `SDF_RD|SDF_STATS` gauges (not
    RSTATS, which `__reset__` would pretend to zero).
  - Both backed by `mt_reading` (`trq_size()`, `priv->pending_acks`).
  - C_MQIOGATE does not change.

**18. `stats-yuno` on a C_IOGATE / C_QIOGATE answers -1** — TRUE, medium.
- **The contract.** It is in `gobj.h:689`: `mt_stats` returns the response
  envelope. These two return a bare dict.
- **Preferred fix:** fix the gclasses, not `ac_mt_stats`.
  - Both return `build_stats_response(gobj, 0, 0, 0, data)`.
  - The one reader in the tree that expects the bare form, C_QIOGATE's
    merge of its bottom (:271), reads `data` from the envelope.
  - Recognising the envelope by a `result` key would hide the violation and
    would be needed again in `c_ievent_cli` and in the agent.
  - It changes the contract: CHANGELOG, and check the projects that read a
    C_IOGATE's `gobj_stats()` bare.

**19. C_IOGATE / C_CHANNEL msg/s depend on the reader** — TRUE, worse.
Low-medium.
- **Worse than the TODO says.** A read less than 1 s after the previous
  one computes nothing AND resets the counters: those messages count in no
  rate.
- **Preferred fix:** the control center's model.
  - A pure-child C_TIMER at `timeout` while playing.
  - The rate over the exact interval (monotonic).
  - The rates as `SDF_RSTATS` with `mt_reading`, zeroed in `mt_pause`.

**20. C_YUNO `uptime` is the machine's, in jiffies** — TRUE, low.
- `read_uptime()` also fails in silence.
- **Preferred fix:** compute it, do not rename it.
  - `uptime` = seconds since `mt_create`, from `start_msectimer(0)`.
  - Delete `read_uptime()`; the same in the ESP32 yuno.
  - Correct the doc example (`yuneta_agent.md:176`).
  - CHANGELOG note of the unit change (jiffies to seconds).

**21. A full C_TCP_S logs an ERROR per refused connection** — TRUE,
medium (38k lines in minutes).
- **Preferred fix:**
  - A third cause, `REFUSAL_NO_FREE_CHANNEL`, in
    `note_refused_connection()`, with its message from a per-cause table.
  - Its own counter `noChannelConnxs` (`SDF_RSTATS`), so "full" is told
    from "denied".
  - Level warning: a full server is a capacity condition, not a broken
    invariant.
  - "no gobj_bottom found" stays an ERROR.

### Control center

**22. The rate tick** — PARTLY, low.
- **True:** the `timeout` of the config is not checked, the rates are not
  zeroed while paused, and C_TIMER's grain halves `max*Msgsec`.
- **False:** the `%d` → `%lld` part. `priv->timeout` is `int32_t` and is
  printed `(int)` (`c_controlcenter.c:395`).
- **Preferred fix:**
  - Check `timeout` in `start_rates_tick()` (below 1: ERROR, use 1000).
  - Zero the rates in `mt_pause`.
  - C_TIMER periodic: `t_flush += msec`, re-based when still in the past.
    It is a kernel change seen by every periodic timer, so it needs its
    test; otherwise correct the doc to "at least `timeout`, on the yuno's
    grain".

**23. "Publish event WITHOUT subscribers" after `run-yuno play=0`** — TRUE
in the code. Low.
- It depends on the realm's config (`__input_side__` with autoplay), which
  is not in this repo.
- **Preferred fix:** no autoplay for `__input_side__`. The control center
  already starts and plays it in `mt_play` and stops it in `mt_pause`.
  Subscribing earlier would accept agents into a paused control center.

**24. gui_agent: fifth rail item clipped at 360 px in Spanish** —
plausible; a layout cannot be verified by reading. Low.
- Five labelled items, and `.yui-nav-iconbar` does not let them shrink.
- **Preferred fix:** decided once against the longest locale, as
  `CLAUDE.md` says: an icon-only bottom bar, or shorter mobile labels.
  Check it with the real Bulma.

### emailsender

**25. `skip-email` without credentials repeats the ERROR** — TRUE, low.
- `cmd_skip_email` calls `start_smtp()` with no condition (:820).
- **Preferred fix:** keep `was_started` before the stop, and restart only
  if it was started.

### Projects

**26. msg2db consumers; wattyzer `C_GATE_PVPC`** — not verifiable here.
- `msg2db_id_incomplete()` exists, is documented with an example and is
  tested.
- Both items belong in each project's own TODO.

### Packaging

**27. The agent is a SysV script** — TRUE, low.
- **Preferred fix:** a native `yuneta-agent.service` for agent1 only.
  `yuneta_agent22` stays outside it. Two points that the TODO does not
  mention:
  - **The pid file.** `--start` daemonizes behind the ydaemon watcher, and
    the pid file holds the CHILD's pid, which changes when the watcher
    restarts it. So either `PIDFile` takes the watcher's pid, or the unit
    runs the agent in the foreground with `Restart=on-failure`.
  - **The cgroup.** The yunos are launched by the agent and land in its
    cgroup. The unit needs `KillMode=process` with
    `ExecStop=... --stop`, or a `systemctl restart` kills every yuno.
- Test it on Rocky and on Debian.

### Stale texts

**28. Doc and comment lines** — five of six are TRUE.
- **True, with their fixes:**
  - `transport.md:31` and `protocol.md:30` (`C_PROT_HTTP_CL`) say seconds:
    it is ms.
  - `transport.md:35` and the attr description (`c_tcp.c:163`): the cause
    is emptied "when a connection attempt starts (EV_CONNECT or the
    reconnect timer) and when a connection begins".
  - `tests/c/c_tcp/README.md` test7: add "up to 7.25.20".
  - gobj-js `CHANGELOG.md:28-30`: an unquoted value ends only at a quote
    that closes an outer value, or before the next `name=`.
  - `c_smtp_session.c:253`: remove "(not counting a bad address)".
  - CHANGELOG upgrade step for a direct fs_watcher owner: handle
    `FS_WATCHER_GONE_TYPE`, drop the pointer, never stop it.
  - `YUNO_AUTH.md:202-218`: discovery needs `end_session_endpoint`; for
    Auth0 and some Cognito setups, set both endpoints explicitly.
- **False:** `protocol.md:54` (`C_PROT_HTTP_SR`) is CORRECT. That gclass
  does use seconds (`c_prot_http_sr.c:43, 235`).

## Corrections to TODO.md

- **Agent relaunch:** the second launch does not come from `ac_on_close`.
  It comes from the sweeps and `run-yuno`, and the "known pid" is always 0
  after a restart of the agent.
- **Follower out of descriptors:** "a yuno raises its limit" is false by
  default (`limit_open_files` = 0).
- **Control center:** the `%lld` fix is not needed.
- **Stale texts:** `protocol.md:54` is not stale.
- **C_COUNTER:** "stop (and so destroy)" — a stop alone does not destroy
  it.
- **Severity:** the C_TCP -2222 item is medium (two use-after-free paths),
  not low. The C_AUTHZ item should name `add-jwk`/`remove-jwk` and the
  role bypass through `create-user`.

## Order I would fix them in

1. **Security (all three in one release):**
   - C_AUTHZ always-on check, including the role bypass (item 3);
   - dbsimple: refuse a foreign file at load, no `fchown` at save
     (item 2);
   - command attrs to `SDF_RD` without `SDF_PERSIST` (item 4).

   Together they close the chain from a group member, or a holder of
   `command-agent`, to `system()`.
2. **Production effects:**
   - the relaunch of a living yuno (item 15);
   - the rt_disk directory the master feeds for ever (item 13).
3. **Memory safety in C_TCP/ytls:** the -2222 paths and the mbedTLS double
   decref (items 6 and 7), one change with one test.
4. **Robustness:**
   - the pre-session frame and the websocket allocation (item 1);
   - the follower out of descriptors (item 10);
   - the ERROR flood of a full C_TCP_S (item 21).
5. **Observability:** stats of C_QIOGATE / C_IOGATE (items 17, 18, 19) and
   `uptime` (item 20).
6. **The rest:** low items, and the texts in one documentation commit.
