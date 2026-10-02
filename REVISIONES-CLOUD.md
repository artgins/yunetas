# Cloud review of main

Reviewed up to `62e070ec5` (2026-10-02). What is resolved is removed from
this file: of the 28 items of the review of TODO.md section 1, two are left
below (9a/9b, 26); every other one is listed under *Acted on*. Last check of the code: clean build with no warning, suite
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
- **1** (frame before the session): both halves. The C_IEVENT_SRV cap does
  matter -- parsing amplifies ~100x, so it is not only "saving the parse";
  the frame is not dumped (a card carries a jwt). The websocket buffer
  grows with what arrives; that needed `gbuffer_realloc()` to grow to its
  max (a doubling past it was refused). No production ceiling existed
  (the 2 GB one is a TRACK_MEMORY build's).
- **2** (dbsimple). The proposed "owner is the data directory's owner" is
  not used for a yuno not run as root: the parents are 02775 too, so the
  directory could be replaced. Trusted: euid, root, and (as root) the data
  directory's owner.
- **3** (C_AUTHZ). Permissions of `treedb_authzs`'s C_NODE, only for
  external calls (as command_parser): `authz_checker` refuses a call with no
  `__username__`. `check-user-pwd` asks `update` (a password oracle).
- **4** and **5**: as proposed (`restart_on_alarm` kept writable: a boolean,
  toggled by its own command).
- **9**: (c) and (d) fixed (a record with no body is not handed; ENOENT of
  a read is a warning). (a) and (b) are real, and left in `TODO.md` with
  their analysis: they need the master's order of signals, not queue
  positions, and a red test first.
- **13** (rt_disk close races the master). `IN_MOVED_FROM` was not in
  fs_watcher's mask, `rt_id_is_confined()` did not refuse a leading dot,
  `walk_dir_tree()` skips dot names, and the master's `FS_SUBDIR_DELETED` of
  the renamed directory would have logged an ERROR: all handled. Test
  `test_delete_key_propagation` (`close_races_master`).

Acted on later the same day (`CHANGELOG.md` "Unreleased"; each with a test
that fails without the fix unless said):

- **10** (a follower out of descriptors): a yuno raises its soft open-files
  limit to its hard one; fs_watcher says once when its directory
  descriptors pass half the limit. What is left (a watch that cannot be
  made, `ENOSPC`) is in `TODO.md`.
- **21** (a full C_TCP_S): a third cause of refusal, said on the
  transition as a warning, counted in `noChannelConnxs`.
- **17, 18, 19, 20**: the queue gauges of C_QIOGATE as stats attrs; the
  envelope from the `mt_stats` of C_IOGATE / C_QIOGATE (yunovatios'
  `c_sim_controller.c` migrated); the msg/s over a window of 1 s at least,
  in ms, that a sooner read does not move (not the proposed timer: two
  readers share one window, said in the docs); `uptime` in seconds since
  the yuno started, and `info-uptime` for the machine's (the ESP32 yuno,
  frozen, is not touched).
- **16**: the agent's `mt_stop` stops and destroys its waiting C_COUNTERs
  (no red test: no agent harness).
- **22, 23**: the control center's rate tick is a C_TIMER0, its config
  `timeout` checked, its rates 0 while paused; `__input_side__` is not
  autostarted in the a.com realm configs (artgins operation repo, not
  deployed: `TODO.md` section 6).
- **25**: `skip-email` restarts the SMTP side only if it was started.
- **8**: mbedTLS keeps the reason of a TLS failure, C_TCP adds it to the
  cause only when there is one, and warns on a truncation (the test needs
  `CONFIG_HAVE_MBEDTLS`; run linked by hand).
- **11**: `dir_identity()` is silent only for a directory gone, falls back
  to `lstat()` when `statx()` is refused, and logs anything else.
- **14**: the in-place save loops its write and writes the old content back
  when it stops half way; the docs no longer promise more.
- **12**: an end of the queued events said past the stream is closed by the
  watcher itself (a one-shot turn of the loop, and after each batch): the
  stream jumps to it with an `FS_BATCH_END`.
- **28**: every stale text, the gobj-js CHANGELOG included.
- **24**: the gap of Bulma's `.level` dropped from gobj-ui's icon bar
  (7.25.24); measured at 360px with the real Bulma in two engines.
- **27**: native units for both agents, `Type=forking` with a new
  `--pid-file` (the watcher's pid). Two points beyond the review: systemd
  runs ExecStop also when the watcher ended on its own, so ExecStop is
  scoped to the unit's own agent, not `--stop`; and the transition stops an
  agent running outside its unit before starting the unit. Checked by hand
  on wattyzer; the package transition and Rocky are in `TODO.md`.

## Still open from the review of TODO.md section 1

Verdicts as written by the review; line numbers are those of `62e070ec5`.

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

**26. msg2db consumers; wattyzer `C_GATE_PVPC`** — not verifiable here.
- `msg2db_id_incomplete()` exists, is documented with an example and is
  tested.
- Both items belong in each project's own TODO.
