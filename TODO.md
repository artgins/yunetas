# TODO

Only **open** work lives here. Anything shipped is deleted from this file the
moment it ships — its record is [`CHANGELOG.md`](CHANGELOG.md) (release notes),
the docs (`yunos/c/yuno_agent/YUNO_AUTH.md`,
`docs/doc.yuneta.io/yunos/mqtt_broker.md`,
`docs/doc.yuneta.io/guide/guide_tls.md`) and git history.

## Found by the last review before the merge (open: fix before deploying)

The fifth independent review of this cycle's fixes found these; they were
left unfixed on purpose. The HIGH ones block a deploy of the yunos they touch
(every yuno that serves ievents for the masking one; emailsender for its two;
any rt_disk follower for the timeranger2 one).

- **Masking is quadratic and reachable BEFORE authentication: one frame can
  stall a yuno for hours** (HIGH, measured; a regression of this cycle, from
  6abc0a541, not in 7.25.20). `mask_secrets_inline()` (helpers.c ~1600-1618,
  and its JS twin helpers.js ~467) resets `name` only at a blank or after a
  masked value, so every non-secret `=` inside one word re-runs
  `is_secret_name()` over the whole word: `json_mask_secrets({"anything":
  "a=a=a=…"})` takes 11 s at 60 KB and 43 s at 120 KB in C (~70 min at
  1.2 MB). `C_IEVENT_SRV` runs it, uncapped and not trace-gated, over the
  whole peer kw in `peer_card_dump()` — for any event before the identity
  card (c_ievent_srv.c ~2116) and in `log_identity_card_refused()` (~1463) —
  before authentication, with frames up to `gbmem_get_maximum_block()` (16 MB).
  Fix: cap the name length `is_secret_name()` is asked about (secret names are
  short) or restart the name after each non-secret `=`, cap the input
  `mask_secrets_inline()` walks, and cap the kw before masking in
  `peer_card_dump()`. Fix it before this code is deployed anywhere reachable.
- **dbsimple: a yuno running as root hands its secrets to whoever planted the
  file** (medium, plausible): a member of the yuneta group replaces the file
  in the 02775 data dir with a readable one of their own; a root save writes
  every persistent attr (the SMTP password included) into the new 0600 file
  and fchowns it to the planter (dbsimple.c ~550, ~575). Give the file back
  only when its old owner is the data directory's owner (the yuno's user);
  otherwise keep it root's or refuse, logged. Also: the load logs "Persistent
  attrs file of another user, left as it is" and the save then "taken over" /
  "kept its owner" — contradictory lines.
- **dbsimple in-place save: a short write leaves an unparsable file without a
  crash** (low): on the no-fallocate fallback (NFSv3, FUSE) and on
  copy-on-write filesystems (btrfs, reflinked XFS), an ENOSPC part-way gives a
  short pwrite(): new prefix + old tail, unparsable, the next start loads the
  defaults and every save is refused. dbsimple.md ~85-88, guide_sdata.md ~202
  and the CHANGELOG say a full disk leaves the old file as it was and name only
  a crash; "never truncated holds for the rename, not for this path" is wrong
  (the in-place path loses "never half-written", not "never truncated").
- **test_secret_attrs: the no-fallocate case can pass without testing** (low):
  if `prctl(SECCOMP)` fails the child exits 0 when the save succeeds and the
  parent checks only the exit code (`_exit(2)` when the filter cannot be
  installed).
- **The masking rule's limits** (low/nit, C and JS alike): over-masks ordinary
  trace text (`token=x is fine now` -> `token=********`; a clause up to the
  next `word=` disappears; `command_mask_secret_line()` hides its own ` <...>`
  marker after a secret); under-masks across a newline (`password=a b\nuser=bob`
  shows `b`) and a later word containing `=` (`token=abc def==` shows `def==`,
  base64 padding).
- **emailsender: a sender refusal after a run of refusals blocks the queue for
  ever, with no ERROR** (HIGH, confirmed by a probe: scratchpad
  `review/emailsender_r5/main_probe_stuck.c` of round 21). A message-owned
  MAIL FROM refusal calls `refuse_current_message(..., counts=FALSE)`, but the
  `refused_in_row > FREE_REFUSALS_IN_ROW` test sits outside the `counts` guard
  (`c_smtp_session.c` ~1344-1363): with 3+ refusals already counted it takes the
  `refusing` branch, drops the session with `reject_code` and no
  `sender_refused`; `ac_disconnected` sets no `transaction` (prev state
  MAIL_FROM_RESP, ~1642), so the emailsender spends no retry and sends it again
  — refused again, for ever, paced, and `note_failure(..., alarm_on =
  !refusing)` keeps the hourly ERROR quiet. Probe: RCPT always 550, MAIL 250 x3
  then 553: after 14 s `queued 1, failed 3`. Set `transaction` whenever
  `reject_code` is set, or test the counter only when `counts`.
- **emailsender: with the producers' own `from`, an account-level block sends
  the whole queue to the failed queue in milliseconds** (HIGH, confirmed by
  `main_probe_quota.c`). `from_is_default == FALSE` makes the refusal the
  message's before the reply is read (`c_smtp_session.c` ~1425,
  `c_emailsender.c` ~1328), and in production almost every email has a foreign
  `from` (estadodelaire/wattyzer db_history, notifier_wz, webstats; the a.com
  default is `no-reply@artgins.com`; a case difference alone makes it
  foreign). "550 5.7.1 Daily sending quota exceeded" with `from` =
  `alarm@example.com`: 12 emails in the failed queue within 2 ms, one session;
  the refusal-run protection never engages. The `5.7.1` + "sender" text test
  also classifies Postfix's "554 5.7.1 <a>: Sender address rejected: Access
  denied" as the message's even for the default sender, and some account
  blocks use 5.1.x (O365 "550 5.1.8 Access denied, bad outbound sender",
  plausible). The reply must decide before the `from` does, and an
  account-level reply must be paced whatever the `from`.
- **emailsender: refusals that do not count log in once per message at a fixed
  rate** (medium, confirmed by `main_probe_logins.c`): a 5.1.x or own-sender
  refusal does not count toward the run, so with a server that refuses RSET or
  closes after each refusal every queued message costs one new AUTH at the
  `timeout_retry` floor, no doubling: RCPT 550 5.1.1 + RSET 554, 12 emails ->
  AUTH every 2 s. Bound it (count every session closed after a refusal toward
  the pacing, whatever the refusal).
- **emailsender: `skip-email` during a mail transaction leaves the next email
  unsent** (medium, plausible, read in the code): `gobj_stop(smtp)` sets
  `detached`, but the session stays in a transaction state
  (MAIL_FROM_RESP/RCPT_TO/DATA_GO/DATA_RESP) until the asynchronous
  EV_DISCONNECTED, and those states have no EV_SEND_MESSAGE row: the
  `tira_dela_cola()` after the skip (`c_emailsender.c` ~816) gets "Event NOT
  DEFINED in state", and nothing drives the new head (a detached close
  publishes nothing) until another email or a play. The `skip_email` test only
  covers a disconnected session. Lows: a 4xx at MAIL FROM ("451 rate
  exceeded") costs a retry, so a rate limit dead-letters the head in ~30 s and
  the next ones while it lasts (design); `foreign_from_refused` does not test
  `from_is_default` (its 553 is the message's anyway) and no test has a foreign
  `from` with an account-style reply; the docs' "a batch of refused messages
  costs one attempt each, at the paced rate" is not what happens (the HIGH and
  MEDIUM above); `skip-email` has no `SDF_AUTHZ_X` though it moves an email out
  of the send path (`set-email-user`/`set-url-from` have it); `skip-email`
  while playing without credentials repeats the "username or password is
  empty" ERROR; `url_change_resets_pacing` was widened to 1.8 s without finding
  why a reset pacing connects ~1 s late.
- **An rt_disk follower still hands a reborn key's records before an older
  delete when the key is not in its cache** (HIGH, confirmed by a probe:
  scratchpad `review/r5-fsw/probe_reborn.c` of round 21). The deferred scan of
  a key directory (`defer_key_dir_scan()`, timeranger2.c ~7487) scans AT ONCE
  when the key is not in the cache and the feed owes nothing, on the premise
  "a key new to the follower: no delete of it can be ahead" — false when the
  key was deleted inside the unread part of the stream. A brand-new key born,
  deleted and written again before the follower reads: `[R1 DEL]`, the key out
  of the cache, the next append `[R1 R2]`. A key deleted and reborn twice in
  one unread window: `[DEL R1 DEL]` (3 rows: `[DEL R1 R2 R3 DEL]`), three
  times `[DEL R1 DEL DEL]`; the overflow pass has the same heuristic. One
  reborn of an already-cached key is right (`[DEL R1]`). The consumer gets DEL
  after live records (drops live data), then duplicates. The immediate scan
  is there for speed (a 69632-key flood drains in 3.6 s with it, 6 s without):
  any fix must keep the flood fast. fs_watcher.md and the
  `defer_key_dir_scan()` comment repeat the false premise. Present in another
  shape before 7.25.20.
- **Silent error paths of the deferred scans** (medium): `dir_holds_a_link()`
  answers FALSE with no log when `opendir` fails (EMFILE is realistic on a big
  follower, EACCES), and `dir_identity()` the same on a `statx` failure other
  than ENOENT; both callers then skip the scan, so the key's pending links wait
  for its next record. The old path logged (`find_files_with_suffix_array()`).
- **A deferred scan can wait without limit on a quiet feed** (low):
  `fs_queued_events_end()` over-estimates by `READ_SIZE` on its fallbacks
  (FIONREAD failure, CQ ring overflow, completions still moving after 3 tries,
  fs_watcher.c ~325/333/347), reachable from a deferral made in the overflow
  pass; the scan then waits for ~READ_SIZE more bytes of unrelated events.
- **The directory identity has no birth-time fallback** (low): with no
  `STATX_BTIME` (ext4 with 128-byte inodes, XFS v4, NFS, some FUSE) only the
  inode is compared, and ext4 reuses inode numbers readily: a directory removed
  and remade between the deferral and its due point passes as the same. Treat a
  missing birth time as "cannot tell" (wait for the directory's own IN_CREATE),
  or log it once.
- **Stale wd entries of the fs_watcher table** (low): `watch_again()` now only
  removes the old watch and leaves the entry for IN_IGNORED; when that
  IN_IGNORED was lost in the overflow, the entry stays for the life of the
  watcher (one string per directory replaced during an overflow).
- **`do_test_reborn_behind_overflow()` can hang instead of failing** (low,
  test): `test_rt_disk_overflow.c` ~1236 loops `i < made/32/2 - 512` on
  `size_t`; with `made` < 32768 it wraps and loops practically for ever, and
  the "the test did not test" check just above only prints.
- **C_TCP: the -2222 decrypt branch can run on a freed gobj** (low,
  plausible). `ytls_decrypt_data()` returns -2222 when the sskt was freed
  inside `on_clear_data_cb` (an EV_RX_DATA subscriber destroyed the connection
  synchronously); `c_tcp.c` ~1628 tests `ret < -1000`, which matches it, and
  now calls `tls_last_error(gobj)` and `set_disconnect_cause()` on freed priv
  before the `try_to_stop_yevents()` that was already there. `set_secure_connected()`
  treats -2222 as "already freed"; the decrypt path must too (`if(ret ==
  -2222) { break; }` first), and the re-entrant `encrypt_data` path into
  `write_data` (~1104) the same.
- **C_TCP `disconnect_cause`: "with the backend's reason" holds for OpenSSL
  only** (low). mbedTLS never writes `sskt->last_error` (the reason goes only to
  `gobj_log_set_last_message()`), so its causes are a bare "TLS handshake
  failed" and "TLS: …: " with an empty reason; on OpenSSL "cannot create the
  secure filter: " always ends empty (no sskt yet) and the flush cause omits
  the `last_error` `flush_clear_data` filled. And `set_disconnect_cause()`
  truncates into `char cause[256]` silently (the rule is never swallow a
  truncation). Docs: transport.md says the cause is emptied "at each
  EV_CONNECT" (the reconnect EV_TIMEOUT empties it too, through `ac_connect`);
  transport.md:31 and protocol.md:30,54 say `timeout_inactivity` is in seconds
  (it is ms); tests/c/c_tcp/README.md test7 describes the replaced cause with
  no "up to" release.
- **c_gss_udp_s_self_stop phase 10 does not prove its order** (nit): it relies
  on the subscription order for its posted restart to come before the posted
  start, and nothing checks the posted start really arrived stale; the
  `!gobj_is_running(udp_s)` guard is not tested on its own (`mt_stop` clearing
  `start_posted` already makes the stale start a no-op).
- **Control center rate tick** (low): the tick is a C_TIMER, which checks its
  deadline on the yuno's `timeout_periodic` grain (1000 ms) and re-arms from
  the moment it was processed, so with `timeout` 1000 it fires every 1 s or
  2 s; the rate stays exact (exact elapsed ms), but a 1 s burst in a 2 s
  interval is halved in `max*Msgsec`, under load precisely. Say it in the docs
  ("at least `timeout`, on the yuno's grain") or fix C_TIMER (`t_flush +=
  msec`). And a `timeout` < 1 given in the CONFIG is not checked (`mt_writing`
  does not run before `mt_create`): the tick is off and the rates read 0 with
  nothing said; check it in `mt_create` / `start_rates_tick()`. Nits: the rates
  keep their last value while paused (zero them in `mt_pause`); the `timeout`
  refusal log prints it with `%d` (use `%lld`).
- **Texts left behind by the last fixes** (low/nit): the emailsender README and
  emailsender.md say "a `from` of the message's own is the message's", but a
  4xx to it is the account's (only a 5xx is judged), and every MAIL FROM
  refusal judged the message's is left out of the refusal run, the default
  sender's 501/553 included; gobj-js CHANGELOG 7.25.9 still says "an unquoted
  value ends at a quote"; tests/c/c_tcp/README.md (test7) describes the
  "Operation canceled" replacement as if current; gui_agent 0.29.7 (yunos-js
  1f6506b) hard-codes "SDK 7.25.21", stale if the release takes another
  number; `api/js/logging.md` links `blob/7.25.8/...#L432/#L590/#L603` for
  functions that 7.25.8 does not have (fixed by the gobj-js 7.25.9 tag +
  `verify_js_api_coverage.py --repin` + `--write`, which also clears the
  appendix drift); an upgrade note is missing for a direct fs_watcher owner,
  which must now handle `FS_WATCHER_GONE_TYPE` (drop the pointer, never stop
  the freed watcher) — no project is one today; this TODO still says "Done
  (Unreleased, 2026-09-29)" (~254) and "What the changes after 7.25.4
  (CHANGELOG.md, Unreleased)" (~414) for work that has shipped. `skip-email`
  "works while paused" is not exercised by a test.

## Defects open after 7.25.20 (the next review round starts here)

What the review of 7.25.6-7.25.20 found is fixed (see CHANGELOG, Unreleased).
Left open:

- **C_TRANGER handles opened through the agent are not reaped** when the
  operator's session ends (documented in `api/gclass/data.md`). A
  `command-yuno` reaches the yuno over its one C_IEVENT_CLI link to the agent,
  which is what is stamped as `src_gobj`; the reaping paths
  (`mt_subscription_deleted`, `ac_on_close` of a C_IEVENT_SRV, `mt_stop`)
  never fire for it, and the operator's session lives in the agent (behind a
  control center, not even there). Needs a DESIGN decision: an agent-to-yuno
  "session ended" notification, and a rule for when several sessions of one
  user are one owner.
- **Secrets a trace still cannot tell**: a credential in RECEIVED bytes that is
  neither an HTTP credential header, nor a `name=value`, nor a json
  `"name": value` with a secret's name; and a parameter of a command for a
  REMOTE service whose name is not a secret's name (no command table is at
  hand to say it is `SDF_SECRET`).
- **Each test binary still has a fixed port**: two whole suites run at once on
  one machine still collide (`ctest -j` within one run is safe since
  `scripts/check_test_ports.py`). Ports chosen at run time would end it.
- **A gbuffer in a published kw is shared by every subscriber**:
  `gobj_publish_event()` hands one kw to all of them, so the first subscriber
  that reads the gbuffer empties it for the next (C_IOGATE's "send to all" had
  the same shape and now copies per channel). Nearly every gbuffer event has a
  single subscriber today; decide whether publishing copies the gbuffer per
  subscriber, or the contract says a gbuffer event takes one.
- **`register_yuneta_environment()` lowercases `root_dir` and `domain_dir`**
  and says nothing: a `work_dir` with capitals is written under another path.
  Dates from 2023 and looks deliberate; decide whether it stays, and if so
  make it say so.

## Schema editing: the admin console

The console is gui_agent's **Schemas** workspace (design and traps in
`yunos/js/gui_agent/README.md`); the backend's gaps are in the next section.

What that workspace still lacks:

- **Authz is the yuno's, and nobody provisions it.** The commands run in the
    target yuno with the logged-in identity, so a console user needs the `read`
    / `create` / `update` / `delete` authz of that `C_NODE` in **that yuno's**
    `C_AUTHZ`. Today an operator who can log into the controlcenter still gets
    `-403` per topic on a yuno where they have no role. Part of the authz-roles
    work, not of the console.

    Worth knowing before diagnosing one: **`descs` and `nodes` take DIFFERENT
    authz** (`a_schema` and `a_nodes` in `c_node.c`). An identity that has one
    and not the other loads the schema and is then refused per topic, so the
    view fills its tabs and answers `-403` for every table — which reads as a
    broken build and is a missing role. Verified on the agents' own treedbs
    with `claudia@artgins.com` (2026-08-19).

## Schema editing: what the `__system__` treedb still needs

The meta-treedb is filled, reconciles by `schema_version` and rebuilds a schema
(7.13.0, `YUNO_TREEDB.md` §3.11). What it does not do yet:

- **Not every treedb is projected at all.** `C_AUTHZ` creates its `C_NODE`
    directly instead of going through `C_TREEDB`'s `open-treedb`, so
    `treedb_authzs` never reaches `__system__` and cannot be edited. Any other
    direct `C_NODE` consumer is in the same position.

- **A removed column with data behind it is still nobody's problem.** The
    write guard refuses what cannot produce a working schema, but dropping a
    column that has values in the topic's records is a legal schema and a data
    decision: the records keep the field, every reader stops showing it, and
    nothing says so. Deciding what the GUI does here (warn, refuse, offer to
    keep it hidden) needs the record side, not the schema side.

- **Applying an edit restarts the whole yuno** (`kill-yuno` → `run-yuno
    play=0` → `play-yuno`), not just the treedb: its gate goes down for the cycle, and any client
    connected to it — the editor included — has to reconnect. Reopening only the
    treedb is not available to a third party and should not be: `close-treedb`
    destroys services whose handles the owner has cached (it now refuses while
    the yuno plays). A true in-place reload would live in the owner, as a local
    method it implements — "reload your schema" — using
    `tranger2_write_topic_cols()` (called only by `tranger2_create_topic()`;
    no reload path uses it) plus
    `parse_schema_cols()` + `parse_hooks()` and a link reload when hooks or
    fkeys change.

## TreeDB / timeranger2: open items

**A flaky test**

- **`test_c_node_link_events` fails now and then** -- twice on 2026-09-15, both
  inside runs of several suites, never alone (15/15 twice). Its message was
  not kept. Next time it fails, keep `build/Testing/Temporary/LastTest.log`
  before running anything else: ctest overwrites it.

**Tests nobody has**: C_NODE commands whose behaviour has no ctest (their
permissions are tested, and some of their refusals, in `test_c_node_authz`):
`node`, `instances` (only its *"What topic_name?"* refusal is tested),
`pkey2s`, `jtree`, `parents`, `children`, `hooks`, `links`, `treedb-info`,
and what the snap commands do to the data (`shoot-snap`, `activate-snap`,
`deactivate-snap`: their answers are tested in `test_c_node_authz`, the
refusal of `snap-content` for another treedb's topic in test 20 of
`c_node_link_events`, and `gc-assets` under an active snap in `c_assets`,
case 7a), and `print-tranger`; `import-db` and `export-db` are tested only
for their error count by cause, their link failures and abort, and the file
name of the export, and a content that is not json (`c_node_link_events`,
tests 14-17 and 19). The refusals on a replica are tested since
7.25.0 (`test_c_node_authz`) and, for C_TREEDB, since 7.25.4.
In gobj-ui, the treedb views got their first wiring tests on 2026-09-23
(`test/dom_double.js`); the save kw as it leaves `publish_treedb_write` is
still untested.

## TreeDB / timeranger2: what 7.25.0 leaves open

What the changes of 7.25.0 (and of gobj-ui 7.23.193-7.23.196 / gui_treedb
0.17.52-0.17.54 / gui_agent 0.22.74, see the `CHANGELOG.md` of each) leave
open:

- **Apply is off on every in-tree yuno:** each one forces `impose_c_schema`,
  so gui_agent's Apply is off on all of them until one stops forcing it.

## ytls (mbedTLS): a failed encrypted-output callback frees the gbuffer twice

Seen 2026-09-26 while fixing the TLS writes of `C_TCP`. `flush_encrypted_data()`
of `mbedtls.c` does `gbuffer_decref(to_send)` when `on_encrypted_data_cb()`
answers < 0, but `C_TCP`'s callback hands the gbuffer to the kw of
`EV_SEND_ENCRYPTED_DATA`, which releases it whatever happens. `C_TCP` no longer
answers < 0 from its action, but `gobj_send_event()` still does when the event
is not in the current state (a TLS record produced while the connection is
already in `ST_WAIT_STOPPED`, say): that path is a double decref. The OpenSSL
backend ignores the answer. Decide who owns the gbuffer after the callback and
make both backends agree.

## timeranger2: a NEGATIVE `from_t` matches no record, silently

Found 2026-09-26 in yunovatios' stress test. `get_segments()` adjusts a
`from_t` below the key's first `t` to "from the start", but the per-record check
of `tranger2_match_metadata()` (`if(md_record_ex->__t__ < from_t)`, since
`0547bf2eb`, 2024-11-24) compares the `uint64_t` `__t__` with the RAW `json_int_t`
of the match_cond: a negative `from_t` becomes a huge unsigned number and every
record is left out. `tr2list <topic> --key=K --from-t=-86400` answers 0 records
where `--from-t=0` answers 997.

It is not academic: FOUR `db_history` yunos open their realtime list with
`"from_t", -3600*24` and the comment *"recupera desde el último día"* --
yunovatios (`db_history_ce`/`_co`), hidraulia, estadodelaire and wattyzer. The
value looks like the relative semantics of the pre-v7 timeranger; under
timeranger2 their start-up load hands the callback NOTHING, so whatever arrived
while the yuno was stopped (or behind) is never processed: in `raw_tracks`, never
in the history nor in its alarms. yunovatios fixes its own caller; the other
three projects are untouched and still carry it.

Decide the contract and apply it in BOTH places: either a negative `from_t` /
`from_tm` is relative (to now? to the key's last `t`?) as the callers believed,
or it is refused with a log. What it must not do is what it does: be accepted and
match nothing.

## timeranger2: a follower re-reads the first row of the md2 on every notification

Found 2026-09-27 profiling yunovatios' `db_history_ce` (1720) with gdb while it
digested a gate outage on the central: **10.9 %** of its work was
`load_cache_cell_from_disk()` -> `load_first_and_last_record_md()`, blocked in
`pread` (`read_md2_row`). `update_new_records_from_disk()` calls it for EVERY
hard link the master leaves in `disks/<rt_id>/<key>/`, i.e. for every append:
open the md2, `lseek` its size, read the FIRST row and the LAST row, close; and
then two `file_exists()` for the `.unordered` / `.tm_unordered` markers.

When the file's cell is already in the cache (`cur_cache_cell`, found just
before by `find_cache_cell()`), its first row is known and cannot change: md2
files are append-only, and a torn tail is cut from the END. Only the size (the
new row count) and the last row are news, and `publish_new_rt_disk_records()`
reads the new rows right after anyway -- the last one among them.

What to do: for a known cell, take the row count from the size (an `fstat`,
or the `lseek` already there), skip the first row, and take the last row's
range from what the publish reads, keeping the cell's `fr_t` / `fr_tm`. Keep
the whole path for a new cell (a file the follower has not seen) and for a
marked file (`.unordered`, which is read whole on purpose). Mind the torn-tail
check, which lives in the same function and must still run.

Expected: most of the 10.9 % (one `open`/`close`, two `pread`s and two
`fstatat` per append, on a follower of ~3000 appends/s). Hot path of every rt_disk follower:
the change needs a test of the follower (`test_rt_disk*`) and an A/B against
the last tag, per the release checklist.

## Queues and channels: two things a gate under load does not say, or says too often

Found 2026-09-27 measuring the capacity of yunovatios' stress gate
(`gate_central^2120`) on the development machine.

**1. The size of a persistent link's queue cannot be read from outside.**
`C_QIOGATE` reports `msgs_in_queue` and `pending_acks` only from its
`mt_stats`. `C_MQIOGATE`'s `mt_stats` asks each `C_QIOGATE` child with
`build_stats()` (`stats_parser.c`), which reads the `SDF_STATS` attributes of
the gobj and of its bottom chain and never calls the child's `mt_stats` -- so
the two figures are lost, and `stats-yuno id=<gate> service=__output_side__`
shows only the bottom `C_TCP`'s counters. How far behind a link is had to be
computed by difference (messages received by the gate minus messages received
by the consumer). Either `C_MQIOGATE` asks its children with `gobj_stats()`
(which honours `mt_stats`), or `C_QIOGATE` declares the two as `SDF_RSTATS`
attributes backed by `mt_reading`. A test: a queue with N messages and the
peer down, `stats` through the `C_MQIOGATE`, `msgs_in_queue == N`.

**2. A full `C_TCP_S` logs an ERROR per refused connection.** When the
`child_tree_filter` finds no free channel (the `__range__` of channels of a
gate is full), `c_tcp_s.c` logs *"TCP_S: Connection not accepted: no free child
tree found"* as an ERROR for every attempt, and the peers retry: 600 channels
and 1000 simulated controllers made **38,156** of them in a few minutes, which
drown the log. It is a capacity condition caused by the peers, not a broken
invariant: warn on the transition (full / free again) with the count of
refusals, and keep a counter stat (`refusedConnxs`) of it. The one line that
matters now -- the channels are full -- is also the one that is hard to find.

## Stats: three things a live monitor cannot trust

Found 2026-09-29 scoping a real-time monitor GUI (cpu % and msg/s per yuno,
polled with `stats-yuno`) against yunovatios' stress yunos on the controller.

**1. Asking a `C_IOGATE` or `C_QIOGATE` service for its stats answers `-1`
with no comment.** `stats-yuno id=2120 service=__top_side__` (a `C_IOGATE`)
and `service=output-qiogate-0` (a `C_QIOGATE`) both return *"ERROR -1"* and
nothing else, while `service=__output_side__` (`C_MQIOGATE`) answers. The two
`mt_stats` return the BARE data dict -- they were written for a parent that
reads its children (`C_IOGATE` calls `gobj_stats(child)`) -- and
`c_ievent_srv`'s `ac_mt_stats` sends that dict back as the stats response,
where the caller expects the `build_command_response()` envelope
(`result`/`comment`/`data`). A silent error, and the only way to read a
`C_QIOGATE`'s `msgs_in_queue` directly. Related to item 1 of *Queues and
channels* above: fixing either path makes a queue observable.

**2. `C_IOGATE`'s and `C_CHANNEL`'s msg/s depend on who reads them, and
when.** Their `txMsgsec`/`rxMsgsec` are computed INSIDE `mt_stats`: the
counter delta since the previous read, divided by WHOLE seconds
(`(ms - last_ms)/1000`, truncated), and `last_*` are reset by the read. A read
1.9 s after the previous one divides by 1 (+90 %); two readers (the gui_agent
Statistics tab and a monitor, or two operators) steal each other's window.
yunovatios' application services do it right -- a 1 s timer, milliseconds
(`c_gate_central.c` `ac_timeout`) -- and so does `C_YUNO`'s `cpu`. Compute the
rate on a timer, not on the read.

**3. `C_YUNO`'s `uptime` is the MACHINE's uptime, in jiffies.** The attr is
described as *"Yuno living time"*, but `read_uptime()` reads `/proc/uptime`
and returns ticks: a yuno started the day before answered `31007411` (3.6 days
of the host). Either compute it from `start_time`, or rename/redescribe it; a
monitor has to use `start_date` meanwhile.

## Stats pushed to a subscriber: what is left after `watch-yuno-stats`

**Done (Unreleased, 2026-09-29):** the agent's `watch-yuno-stats` pushes
`EV_YUNO_STATS` along the requester's route, the readings SAMPLED by the agent
(option (a) below), the control center relays it (item 3, with `__relays__`
and a `watch_ttl`), and gui_agent's Monitor uses it both ways, polling a node
that refuses. **Left:** (b) the yuno sending its own stats, and the
Statistics cards moving to the same stream.

**What the transport allows (checked 2026-09-29).** A subscription travels
only from a CLIENT to a SERVER: `C_IEVENT_SRV` has no
`mt_subscription_added`, and a `__subscribing__` that reaches a
`C_IEVENT_CLI` is refused ("subscription event ignored, I'm client") and the
channel dropped. The agent is the SERVER of its yunos' channels and the
control center the server of the agents', so neither can subscribe
downstream. (A first version of this entry said they could; it was wrong.)
What does go downstream-to-upstream is an inter-event SENT along a ROUTE the
agent kept from a request: the PTY mirror (`add_console_route` at
`open-console`, then `EV_TTY_DATA` built with `msg_iev_build_response(...,
route)`), which crosses the control center because the control center has
its own relay for it (`ac_tty_mirror_data`, `tty_mirror_dst_service`).

**The design that fits.**

1. **`watch-yuno-stats` on the agent** (`id=<yuno>... period=<ms> on|off`):
   the agent keeps the route of the requester, per yuno, and SENDS
   `EV_YUNO_STATS {yuno_id, yuno_role, cpu, ..., <service stats>}` along it
   every period; `off`, the requester's channel closing, or the yuno dying
   remove it. Direct browsers reach it as they reach the terminal.
2. **Where the figures come from** -- the open decision:
   - (a) the AGENT samples: it asks its own yunos (`stats_to_yuno`, local
     loopback) on the watch period and forwards each answer. Works with every
     yuno already deployed; only the agent changes. The periodic query moves
     from the browser into the node.
   - (b) the YUNO sends: `C_YUNO` gets a `publish-stats` command and, while
     on, sends `EV_YUNO_STATS` to its agent from the timer that already runs
     `load_stats()`. Cleanest (the producer publishes), but a yuno publishes
     only once REBUILT with the new kernel -- every project yuno of every
     node -- so it needs (a) as the fallback during the transition anyway.
   Either way one reader on a timer settles item 2 of *Stats: three things a
   live monitor cannot trust*.
3. **The control center relays `EV_YUNO_STATS`** like `EV_TTY_DATA`: a new
   action and the per-channel destination, in `c_controlcenter.c` -- a
   deploy of the control center on a.com.
4. **gui_agent** sends `watch-yuno-stats` instead of arming its C_TIMER, and
   drops the polling exception; `list-yunos` per tick becomes an
   `EV_YUNO_STATE` in the same stream.

## Controlcenter: scenarios in its treedb (approved 2026-09-29)

Decided with the user; phase 2 (gui_agent's Users workspace, the authz of
each yuno, both control centers included) shipped in gui_agent 0.28.0. What
is left, in order:

1. ~~Clean the control center, scenarios in its treedb~~ -- done, 7.25.14
   (`schema_version` 3: `scenarios` + `scenario_runs`; commands `scenarios`,
   `save-scenario`, `delete-scenario`, `run-scenario`, `scenario-runs`; the
   four legacy topics and their dead code removed).
2. ~~Users view~~ -- done, gui_agent 0.28.0.
3. ~~One Scenarios workspace in gui_agent~~ -- done, gui_agent 0.29.0
   (list, tree of yunos as cards, live view; Statistics removed).
4. ~~`scenario_runs` + `run-scenario`~~ in the control center -- done,
   7.25.14 (who ran what and when, each step with its answer; startable from
   `ycommand`). Not recorded: the PEAKS of a run (rates are measured by the
   console, not by the control center).

**Deployed** 7.25.14 on a.com (1996, 1997) on 2026-09-29 and verified end to
end: a scenario saved from the console, its `report` run by the control center
through the dev node's agent (both steps answered, as the console user), its
run listed, then deleted. The scenario of the yunovatios stress test is kept
there as `yunovatios-stress`. Left, minor: on a phone the rail's fifth item is clipped at 360 px in Spanish (the bar
scrolls); a control center started with `run-yuno play=0` and played seconds
later logs one *"Publish event WITHOUT subscribers"* (`EV_ON_OPEN` of
`__input_side__`, autoplay) per agent that reconnects in between -- its
subscription is made in `mt_play`.

The two points left by the review of 2026-09-30 went into 7.25.16: a save
carries the revision it read (refused if somebody saved since), and the agent
answers at once for a yuno that runs but is not connected.

## Agent: a C_COUNTER still running when the agent stops

Seen deploying 7.25.13 (2026-09-29), at the orderly `--stop` of the main agent
on yunovatios-central (the 7.25.12 package agent) and on the dev node (a
pre-release build): per pending counter, one `gobj_destroy` *"Destroying a
RUNNING gobj"* and one `gobj_unsubscribe_event` *"No subscription found"*
(subscriber `C_AGENT^agent`C_COUNTER^N`C_TIMER^N`, event
`EV_TIMEOUT_PERIODIC` of the yuno), 2 and 5 of them. The counters are the ones
a multi-yuno command (`run-yuno`, `kill-yuno`) arms and waits on for
`EV_FINAL_COUNT`; the likely source is a `run-yuno` of a yuno that never came
up (on the dev node the yunos that cannot read their TLS key). A counter left
waiting has to be stopped in the agent's `mt_stop` (or end by its own
expiration) instead of being destroyed running at `gobj_end`.

Read of the code (2026-09-29, not changed): the agent creates each one with
`gobj_create_volatil(..., C_COUNTER, ...)` as its own child (`run-yuno`,
`kill-yuno`, `play-yuno`, `pause-yuno`, `c_agent.c` ~5180/5359/5547/5723),
with `expiration_timeout` = `timeout_expiration` (30 s), and subscribes it to
`__input_side__`'s `EV_ON_OPEN`/`EV_ON_CLOSE`. `C_COUNTER` only stops and
destroys itself in `publish_finalcount()` (count reached or the timer
expired), and its `mt_stop` already unsubscribes everything and clears its
timer. So a counter younger than 30 s when the agent stops is still running:
the fix is for the agent's `mt_stop` to stop (and, being volatile, destroy)
its `C_COUNTER` children -- e.g. `gobj_match_children` by
`__gclass_name__` -- before `gobj_end` walks the tree.

## C_NODE: every link collapses the WHOLE parent, O(children) per link

Found 2026-09-26 in yunovatios' stress test (`db_history_ce`, 20000 new devices
filed under three `device_types`). Without `with_link_events`, which is the
default, `_link_nodes()` publishes the backward-compatible
`EV_TREEDB_NODE_UPDATED` of the PARENT, and `C_NODE`'s `treedb_callback()`
answers it with `node_collapsed_view()` of that parent -- every hook list, the
ids of every child included. A parent with thousands of children costs that
much PER LINK: filing a device took ~70 ms of cpu, the history fell from
3000 to ~13 new devices/s, and a fleet of N new devices costs O(N^2). Profiled
with gdb: 10 of 15 samples in `apply_child_list_options()` under
`release_treedb_events()` of `treedb_link_nodes()`.

It only bites the FIRST link of each child (steady state does not link), but
that is exactly the moment a whole installation comes on line. Options, not
decided: publish the parent without its child lists (the subscriber that needs
the children asks for them), collapse only when the event has subscribers, or
make `with_link_events` the default once no v1 SPA depends on the parent's
update (estadodelaire and hidraulia still do -- see its memory note).

## Agent: a yuno that lost its channel is launched AGAIN while it still lives

Found 2026-09-26 on the dev node (yunovatios' `sim_controllers`). The yuno was
alive but stuck loading 13.7 M queued messages, and when the agent restarted it
could not keep the channel: `ac_on_close` logged *"yuno down"* and, as the yuno
is `must_play`, `run_yuno` launched a SECOND process at once. The first still
held its persistent queues' exclusive lock, so the second opened them *"as not
master"*, its first `trq_append()` failed and `C_QIOGATE` aborted (*"Message NOT
SAVED in the queue"*, `LOG_OPT_ABORT`: a core of 2.6 GB). Seconds later the old
one reconnected and the agent killed it (*"yuno ALREADY living, killing new
yuno"*, which kills the one reconnecting, here the OLD one).

A dropped channel is not a dead process: before relaunching, the agent should
check that the pid it knows (or `yuno.pid`) is gone, and give a live one time to
come back instead of starting a twin that fights it for its stores.

## CLI: two registered projects with a yuno of the same role overwrite each other in `outputs/yunos`

Found 2026-09-28 on hidraulia, in production. Both registered projects,
hidraulia and yunovatios, build a yuno whose role is `gate_caudal`, and both
install it to `$YUNETAS_BASE/outputs/yunos/gate_caudal`. `yunetas build` builds
them one after the other and the LAST one wins, without a word (yunovatios,
06:29:36, over hidraulia, 06:29:30). The node's own reinstall script then ran
`install-binary id=gate_caudal content64=$$(gate_caudal)`, which reads
`outputs/yunos`, and hidraulia's gate ran yunovatios' binary for three hours:
*"GClass NOT FOUND: C_DECODER_CAUDAL"*, no input channel, no listener.

`$$(role)` is only a name, and with two projects on one node the name is no
longer enough. Options, not decided: `yunetas build` refuses (or at least
warns) when a project installs a role another registered project already
installed; or each project installs to its own `outputs/yunos/<project>/` and
`$$()` resolves against the project it is called from. Until then, a node with
two projects must not share a role name between them, or its deploy scripts
must name the binary by path.

## TreeDB / timeranger2: what 7.25.5 leaves open

What the changes after 7.25.4 (`CHANGELOG.md`, Unreleased) leave open:

- **The tm marker after a rollback**: a 7.25.4-or-earlier binary appends
  out-of-order tm without `.tm_unordered`; `mark-tm-order` re-marks the topic,
  but nothing does it on its own. A per-writer stamp would make it automatic.
- A marker that cannot be written is retried only by the next append to the
  same FILE; a file never appended again keeps it missing (logged) until
  `mark-tm-order` runs.
- `mark-tm-order` runs synchronously and blocks the yuno (linear; ~19 ms for
  1 key x 30 files x 20 000 rows, `perf_timeranger2`).
- A demoted master never takes its lock back while the process lives; it needs
  a restart (documented).
- In a partial topic, operations on other ids are allowed (creates, updates,
  links); only the creates of the unloaded ids and the deletes of their
  possible parents are refused.
- **A multi-key (`rkey`) iterator** does not see keys created after it opened;
  the `key_deleted` mark reaches only the process that deleted the key. Both
  are single-node conveniences by design (philosophy.md, "the key").
- `default: {}` placeholders are dropped by save + apply, so a `required`
  column whose literal really declared `'default': {}` loses it (as in 7.25.4).
- msg2db consumers (the db_history alarms of wattyzer, yunovatios,
  estadodelaire, hidraulia) do not use `msg2db_id_incomplete()` yet: an alarm
  absent after a damaged load can be announced again as new
  (`tr_msg2db.md` has the code).
- **No red test** for: `deactivate-snap` -1 on a failed save, the fs_watcher
  root, `save_json_to_file()`'s `close()` failure, the crash window between a
  marker and its md2 row; the `kw_incref()` of C_NODE `cmd_treedbs` /
  `cmd_links` / `cmd_hooks` / `cmd_get_node` (every path to them goes
  through the command parser), of C_MQIOGATE's `view-channels`, and the
  `kw_update_missing()` of C_IEVENT_SRV's `EV_ON_CLOSE` (none of those kws
  carries a gbuffer today); the two yuno-skeleton fixes (`MSGSET_INTERNAL`,
  the timer as a pure child: checked by hand with a yuno made by
  `yuno-skeleton -p`; no ctest builds the templates); the agent's exit 0 when
  its treedb does not open (no ctest runs the agent); the entry point closing
  the log files after the leak report; `find-new-yunos create=1` skipping the
  rows already registered (the preview is tested, `c_agent_find_new_yunos`; the
  skip in `c_agent.c` runs in no ctest); the relink of a test after an installed
  archive changed (shown by hand: the build prints the link line once, then
  nothing); the test harness no longer counting *"io_uring_queue_init_params()
  pinned-memory pressure, retrying"* as an unexpected log (gobj-c
  `testing.c`: it needs the machine short of locked memory while the test
  creates its loop); `create-yuno` refusing a release name longer than
  `NAME_MAX` (`cmd_create_yuno()` lives in `c_agent.c`, which no ctest
  compiles). Not exercised live: a form Save through a real websocket drop.

## Agent: the spare agent is only refreshed on the package path

`install.sh` restarts `yuneta_agent22` after an upgrade, once the main agent is
confirmed healthy on the new binary (7.12.0). That covers the **runtime** nodes,
which install the `.deb` / `.rpm`.

It does not cover the nodes that **build from source**: there the binaries come
from `yunetas build` and both agents are restarted by hand. Nothing checks, and
nothing says the spare is stale — which is exactly how it went five days unnoticed
on four nodes, running the version-comparison bug 7.12.0 fixes.

The natural home is the `yunetas` CLI, since it is what put the binaries there:
after a build that replaced `/yuneta/agent/*`, restart the main agent, verify it,
then the spare. Same order as `install.sh` — the spare is the only way into a node
whose main agent is broken, so it is never touched first.

The REPORT half is done: `tools/agent/audit-agents.sh` (see `tools/README.md`).
What is left is the automation: the `yunetas` CLI restarting the agents itself
after a build that replaced `/yuneta/agent/*`, main first and the spare only
once the main one is confirmed healthy.

## gui_treedb: leftovers from the 2026-07-13 audit

One item, and it is a feature project rather than a leftover:

- **Backend features with no UI** (the SPA uses 15 of ~45 C_NODE/C_TRANGER
  commands). Highest operator value, in order: **snapshots** (`snaps`,
  `snap-content`, `shoot-snap`, `activate-snap`, `deactivate-snap` — tag a
  version, browse it read-only, roll back: all there, zero UI); **backup /
  restore** (`export-db` / `import-db`, a base64 payload maps straight to a
  browser download / file input); and the relationship inspector
  (`parents` / `children` / `links`, plus `jtree`, whose ready-made tree with
  `__path__` we ignore in favour of a flat `nodes` list).

## Resolver: name resolution still blocks the event loop

7.8.2 cached DNS answers and made the cost visible (`getaddrinfo() BLOCKED the
event loop`, plus the syslog trail from `static_resolv.c`). That bounded the
blast radius of the `central.yunovatios.es` outage — a black-holed first
`nameserver` costing ~6 s per lookup — but the shape of the problem is intact:

- **`getaddrinfo()` runs synchronously inside the loop** (`yev_loop.c`, three
  call sites: connect, source bind, listen). Every gobj, timer and pending
  completion in the process stops for the duration. The cache means one lookup
  per host per TTL instead of one per connect, but that first lookup still
  freezes everything. A resolution that cannot block would have to be an async
  step in the FSM, like any other I/O — which is the framework's own rule.
- **Nameservers are tried strictly in order, 3 s x2 each**
  (`YUNETA_DNS_QUERY_TIMEOUT`, A then AAAA). A dead first entry always costs
  6 s before the second is reached. Options, cheapest first: drop the per-query
  timeout, remember which nameserver last answered and start there, or query
  them concurrently and take the first reply.

Neither is urgent while nodes have a working `resolv.conf`; both are what turns
a misconfigured node from an outage into a log line.

## Inter-yuno subscriptions: a refused subscription is silent, and the cost of many

Decided 2026-09-25, no C SDK code touched yet.

- **A negative ack for a refused subscription.** `__subscribing__` is one-way:
  the client (`C_IEVENT_CLI`, C and gobj-js: `send_static_iev()`) waits for no
  answer and `C_IEVENT_SRV` sends none. When a subscription is refused
  (`max_subscriptions`, `max_subscription_size`, authz), the only trace is one
  WARNING in the server's log, on the transition (*"SUBSCRIBING refused, the
  peer holds max_subscriptions"*). The peer never knows: in a SPA the
  devices beyond the cap just stop updating, with nothing in the console, and
  after a reconnect (`resend_subscriptions()`) a different set can go silent
  if the order changes. Implement a negative ack that names the event and the
  cause, in C and gobj-js alike, and have the client log it (and publish it,
  so a view can say which data will not update). Note that an ack changes the
  wire protocol: an older client must ignore it.
- **What a large number of subscriptions costs.** hidraulia raises
  `max_subscriptions` to 20000 per channel (its SPA subscribes twice per
  device). Measure what that costs, in memory (each subscription is a json
  kept per channel, plus the refused-subscriptions count) and in speed
  (`peer_has_subscription_room()` runs `gobj_find_subscribings()` on every
  `__subscribing__`, so a SPA that subscribes N times does O(N^2) work at
  connect and at every reconnect; and a publish walks the subscription list
  of its event). Decide from the figures whether the default of 5000 stands,
  and whether the count needs to be kept instead of searched.

## Auth: OIDC migration follow-ups

- **Real-IdP smoke tests beyond Keycloak.** Auth0 / Cognito / Authentik are not
  live-tested (no tenants). Code finding from the discovery contract:
  `save_oidc_discovery` hard-requires `end_session_endpoint` and aborts
  (`STOP_TASK`) if absent. **Auth0 does NOT publish `end_session_endpoint`**
  (proprietary `/v2/logout`); some Cognito setups omit it too → discovery would
  fail. Workaround exists (set explicit `token_endpoint` +
  `end_session_endpoint`, skips discovery). **Decision needed:** relax the
  requirement (degrade to local logout when absent) vs document the
  explicit-endpoints requirement for those IdPs. Authentik exposes it.
- **ROPC → device/client-credentials migration — deferred until a non-Keycloak
  IdP is adopted.** `action_get_token` in `c_task_authenticate` uses
  `grant_type=password` (username + password + client_id, single round-trip).
  Works today because every deployed IdP is Keycloak (permits ROPC). Becomes
  necessary when an IdP that disables ROPC by default (Auth0 / Cognito / Azure
  AD / Authentik) is adopted. Not a drop-in swap to PKCE:
  - All 6 callers (`ycli`, `ycommand`, `ystats`, `ytests`, `ybatch`, `mqtt_tui`)
    are CLI/server tools with **no browser and no local HTTP listener**; the tree
    has no device-flow, loopback-redirect, or browser-open primitive. Classic
    PKCE (authorization code + loopback) does **not** fit — these run headless over
    SSH. (`c_auth_bff`'s PKCE is server-side for the web SPA, a different context.)
  - Correct replacements split by use:
    - **Interactive** (`ycli`; the others when a human runs them) → **Device
      Authorization Grant** (RFC 8628): print URL + user code, poll the token
      endpoint with `urn:ietf:params:oauth:grant-type:device_code` (handle
      `authorization_pending` / `slow_down`). Discover `device_authorization_endpoint`.
      No password in the tool; works on every IdP.
    - **Headless CI** (`ybatch`, `ytests` — no human at all) → device flow can't
      work either; use **Client Credentials** (a service-account client + secret),
      machine-to-machine. The token subject is the service account, not a user.
  - Scope when undeferred: `c_task_authenticate` (new FSMs + discovery fields +
    config attrs) + all 6 callers + tests + docs. Keep ROPC as a fallback for
    Keycloak. **Do not point any CLI at a ROPC-disabled IdP before this lands.**

## Security: per-command authz gate — production enablement

The gate (`enable_command_authz`) is **default-off** (design in YUNO_AUTH.md
§4.5). To enable in production, per node:

- provision **every principal that sends commands TO each C_AUTHZ yuno** with
  `__execute_command__`/root — not only `yuneta`/admins but the **controlcenter**
  user(s) that reach the agent's `:1993` port (the agent store currently has
  `yuneta` + `yuneta_admin@…` + `yunetas_admin@…`, NOT `yuneta_agent@…`);
- confirm each of the 5 C_AUTHZ stores (agent, agent22[shared], controlcenter,
  mqtt_broker, emailsender) holds the `root`/`yuneta` model at runtime — a
  store that missed `Authz.initial_load` re-seeds itself on its next master
  start (`C_NODE` applies the attr), so this is a check, not a repair;
- run a real **low-privilege deny test** on staging (needs a non-root external
  principal — infeasible on the yuneta-only local plano);
- then set `enable_command_authz: true` per yuno (pilot the agent first),
  staging → production.

**Seen live with gui_agent's Users workspace (0.28.0, 2026-09-29):** through
the control center, `claudia@artgins.com` -- who has NO role in the local
agent's store -- created, disabled, enabled and deleted a user there, because
`create-user` & co. of `C_AUTHZ` are `SDF_AUTHZ_X` only. The role link of the
same session was refused ("No permission to 'update' in service
'treedb_authzs'"): `link-nodes` is a `C_NODE` command with its own check,
always on. So today, user management on every node is open to whoever the
control center lets run `command-agent`.

The subscription gate (`enable_subscription_authz`, YUNO_AUTH.md §4.6) is
**default-off** too. Enabling it on a yuno needs the same role model, and
every user of a treedb GUI needs `read` on the C_NODE and C_TRANGER services
it watches (C_NODE's `EV_TREEDB_NODE_*` and C_TRANGER's
`EV_TRANGER_RECORD_ADDED` are `EVF_AUTHZ_SUBSCRIBE`); a refused GUI keeps
its session and gets no live updates. `EVF_AUTHZ_INJECT` is still **declared
but not enforced** — no gate exists for `gobj_send_event`.

## Security: ytls TLS posture — per-gate rollout

Remaining is **per-gate deployment config** (validate on staging):

- raise high-level gates explicitly where wanted; set the IoT-compat profile
  (`ssl_min_version` + `ssl_ciphers "@SECLEVEL=0"`, OpenSSL backend) on legacy
  gates;
- turn on peer verification per high-level gate (`ssl_trusted_certificate` or
  `ssl_use_system_ca`); IoT gates opt out with `ssl_allow_insecure_client=true`.
  Remaining is the per-gate **deployment** config: set the CA (or the explicit
  `ssl_allow_insecure_client` opt-out) on each client crypto block in the realm
  config, and raise the server-side gates.

## Security: MQTT broker ACL — model + default-deny decision

The publish + subscribe ACL (model A: per-group `publish_acl`/`subscribe_acl`,
`enable_acl` default off) is in the broker treedb — see mqtt_broker.md. Open
decisions (Rosa):

- the **A/B/C model choice** — A = treedb group ACL (shipped); B = reuse the
  framework `C_AUTHZ` via `gobj_user_has_authz` (one authz system, but its
  checker is per-authz-name not topic-pattern → needs extending); C = a broker
  config attr holding the pattern map (no schema migration, off the treedb/UI);
- whether to flip enforcement to **default-deny** (validate on staging first).

## Security: not yet reviewed for memory-safety

- `modules/postgres` (libpq wrapper) — delegated to libpq, lower priority.
- the `yuno_agent` control plane + `watchfs` command-exec — re-audit if the
  agent's `SDF_WR` command attrs become remote-writable (prior fixes
  `8c03eb686` / `5dbede6a1` + authz gating).

## Security: vendored libjwt — maintenance

- **Periodic re-vendor from upstream** — procedure in
  `kernel/c/libjwt/README.md` (§ Re-vendor procedure).

## Observability: source-IP attribution in decoder logs — remaining pass

The `peername` roll-out across the protocol/decoder error logs shipped
2026-06-21 (kernel + hidraulia + estadodelaire); its record is `CHANGELOG.md`
and git history. What was intentionally skipped and is still open:

- The scope rule is `CLAUDE.md`'s decoder-severity question, *"could a
  remote peer trigger this with bad bytes?"* (`c_prot_mqtt2.c` and
  `c_prot_http_cl.c` are done, 7.22.0).

- **Still open: wattyzer `C_GATE_PVPC`** — an outbound client too, and so
  likely the same answer as `c_prot_http_cl.c` (nothing a peer can trigger; the
  url or the endpoint name is the useful field). Unlooked-at. It lives in the
  wattyzer repo, not here.

- **Out of scope, do not migrate:** `C_PROT_MQTT` (`modules/c/mqtt`) is
  deprecated but still in Hidraulia production.

The canonical read pattern is the one in `c_websocket.c` / `c_prot_mqtt2.c`:
read `peername` off the bottom gobj once, in the cold error branch.

## C_TRANGER: realtime feed (Live cards) — inotify scalability

Context: `open-rt`/`close-rt` + `EV_TRANGER_RECORD_ADDED` (public) power
gui_treedb's Live records card. On a **non-master (reader)** C_TRANGER —
e.g. `db_history_wz`, `master:false` — each `open-rt` opens a
`tranger2_open_rt_disk` feed = **one inotify instance**. The problem surfaced
under real use (found 2026-07-12 on e.com, where the node sat at 128/128
`fs.inotify.max_user_instances`, its default):

- **#1 — Share one rt_disk feed per topic across Live cards.** Today each Live
  card opens its own per-key feed → N cards = N inotify instances on a reader
  backend. Open a single `rt_disk` feed per topic (`key=""`, all keys),
  refcounted, and let each subscriber filter by key on its
  `EV_TRANGER_RECORD_ADDED` subscription (subscriptions cost no inotify). Caps
  usage at **1 inotify per followed topic** regardless of card count. Small,
  high-value change.

  Note (2026-07-14): a Live card now subscribes filtering on **its own feed's
  `rt_id`**, not on the key — with SEVERAL feeds alive, a `{topic, key}` filter
  matches every publish of that key and the cards double each other's rows.
  Under this design there is only ONE feed, so its publishes all carry the same
  `rt_id` and the subscribers MUST go back to filtering by key: whoever
  implements it has to flip `live_filter()` in `c_tranger_view.js` in the same
  change, or the cards go silent.

  **Read in depth 2026-09-16, and it is NOT the small change this entry calls
  it.** What `open-rt` returns today IS the feed: `cmd_open_rt` opens one
  `tranger2_open_rt_mem/disk` per `rt_id` and `register_handle()` files it, so
  one client = one feed = one inotify. Sharing means the `rt_id` a client gets
  back stops naming a feed and starts naming a SUBSCRIPTION to a shared one,
  and everything keyed on that assumption moves with it:

  - `cmd_close_rt` must decrement a refcount and close the underlying feed only
    at zero, instead of closing what it finds;
  - `reap_handles_of()` (the session-death and subscriber-death reaper) must
    decrement too, not `tranger2_close_list()` — otherwise one dead session
    takes the feed away from every other card on that topic;
  - `publish_rt_callback()` stamps the SHARED feed's `rt_id` into every
    publish, which is exactly why the SPA has to filter by key again;
  - the master path (`rt_mem`) has no inotify and no such problem, so the
    sharing is only worth it on a reader — but doing it on one side only
    leaves two publish contracts, and the SPA cannot tell which it is talking
    to. Decide whether the shared feed is unconditional.

  So it is one design change across C and the SPA, landing in the same release
  with a coordinated deploy, plus a test that opens two cards on one topic and
  counts inotify instances (`info-inotify`). Worth doing — the node sat at
  128/128 — but not in passing.

- **#2 — Closing a reader's rt_disk feed races the master that feeds it.**
  Seen 2026-09-26 in yunovatios' stress test (a `db_history_ce` reading the
  `raw_tracks` of a `db_tracks_ce` that appends 3000 records/s over 30000
  keys): every orderly stop of the reader under load logs `remove_tree_walk`
  `rmdir() FAILED` *"Directory not empty"* (errno 39) on
  `<topic>/disks/<rt_id>/`. The master keeps hard-linking new md2 files into
  the per-key subdirectories of that feed while the reader removes the tree,
  so the directory is left behind with fresh links in it. Never seen without
  load (the same reader on an idle central: 0). Unknown yet whether the master
  goes on linking into a feed nobody reads after that, which would be links
  accumulating for ever. Repro: `yunovatios/yunos/sim_controllers` at 3000/s
  against the stress realm, then `kill-yuno` of its `db_history_ce`.

- **#3 — A follower hears its OWN consumption: half its inotify queue is
  echo.** Each hard link the follower consumes (`update_key_by_hard_link()`
  unlinks it) is an `IN_DELETE` in a watched key directory, which it then
  ignores (*"it's me"*). Half of what fills its queue under load is that echo,
  which is why a follower overflowed at ~3000 records/s, and why one burst
  overflows it twice (`test_rt_disk_overflow` sees two overflows). An overflow
  no longer aborts the yuno (the feed is rescanned, see the fs_watcher page),
  but it still costs a rescan of every key. Watching the key directories
  without `IN_DELETE` (the root keeps it: a key directory's deletion is the
  key-delete signal) would halve the queue; it needs a per-level mask in
  `fs_watcher`, and `C_FS` still wants file deletes.

Node-side mitigation (already provisioned, independent of the above): the deb/rpm
packagers ship `99-yuneta-core.conf` raising the default
`fs.inotify.max_user_instances` of 128 — too low for a node running ~12 yunos
with rt_disk followers — to 4096 (`max_user_watches = 524288`,
`max_queued_events = 65536`). Observe live usage with
`ycommand -c 'info-inotify'` (limits + this yuno's instances/watches). It only
raises the ceiling: **#1 still multiplies instances per Live card**, which is
what the remaining work above fixes.

## Packaging: the sparse SDK in the `.deb` serves one glibc at a time

The `.deb` installs a sparse SDK under `/yuneta/development/yunetas`
(`outputs/`, `outputs_ext/`, `tools/`, `.config` — no sources) so a node can
compile a project against the published runtime without a source tree. That
promise does not hold today, and cannot hold for more than one glibc at a time.

The shipped `outputs/lib/*.a` are **static** archives: they reference glibc
internals (`_dl_x86_cpu_features`, backing the ifunc `memcpy`/`strlen`
dispatch) whose layout moves between releases. Linking fresh objects against
them under a different glibc succeeds silently and corrupts the heap at run
time — SIGABRT inside `_int_malloc` seconds after start, no framework error
first. `tools/cmake/libc_guard.cmake` stops it at configure time via
`outputs/lib/yuneta_libc.stamp`.

Since 7.8.6-3 the `.deb` is built in a `debian:13` container (glibc 2.41),
matching Debian 13 nodes, which can build. Before that it came off an
`ubuntu-22.04` runner (glibc 2.35) that **no node ran**, so the guard fired
everywhere and the sparse SDK was dead weight in the package. (The EL9 `.rpm`
never had that problem: `rockylinux:9`, glibc 2.34, matching Rocky 9 nodes. The
guard compares only `major.minor`, so EL9 point releases — 2.34-231 vs
2.34-272 — do not break it.)

So the promise now holds, but for exactly one distro per package: an Ubuntu
22.04/24.04/26.04 node still cannot compile against the shipped `.deb`, and
neither can Debian 12. Moving the base moved the boundary; it did not remove
it, which is what the options below are about.

Options, in rough order of cost:

- **Drop the build half of the `.deb`** (leaves `outputs/lib`, headers and
  `.config` out; keeps the runtime binaries). Honest about what the package is,
  and matches how deploys already work — binaries are built on a dev machine
  and pushed with `yunetas sync-binaries`. **Current preference.** Note this is
  *not* a size argument: the archives and headers are ~77 MB of a 1.1 GB tree
  (the static yunos are 944 MB of it), so the `.deb` would barely shrink. The
  reason is that the package ships, documents and maintains a capability the
  guard blocks on every node.
- **Build the `.deb` on a matrix** (22.04 / 24.04 / 26.04) and publish one per
  base. Keeps static linking and keeps the sparse SDK working. The fallback if
  on-node compilation is ever needed again. What it actually costs:
  - **The external archives must be rebuilt per base too**, not just the SDK
    ones — and they are the bulk: 19 of the 31 archives and 61 MB of the 71
    (OpenSSL, mbedTLS, pcre2, ncurses, liburing, jansson). The workflow already
    builds them from source (`extrae.sh` + `configure-libs.sh`), so this is
    runner time, not new machinery; jobs run in parallel, so wall-clock stays
    near the current ~15 min.
  - **Asset selection becomes real work.** Three `.deb`s instead of one means a
    naming scheme and an `install.sh` that detects the distro *version*, not
    just the family (`apt` vs `dnf`, all it does today) — plus a new failure
    mode when a node's version is not covered.
  - **Unknown: third-party code under much newer compilers.** 22.04 ships gcc
    11, 26.04 ships gcc 15. Whether OpenSSL/ncurses/pcre2 build clean four gcc
    majors forward is untested here; assume it needs work before costing this
    option.
- **Ship shared libraries instead of static archives.** glibc versions its
  symbols, so a `.so` built against the oldest supported glibc links and runs
  on every newer one — one artifact, no matrix. It gives up the
  `CONFIG_FULLY_STATIC` property for the SDK libs, which is a deliberate
  feature of this project, so it is a real trade, not a free win.
- **Distribution packaging** (Debian/Fedora build against their own glibc).
  Correct by construction and the highest cost by far: their policies, their
  schedule, their review, and a version lag we do not control.

Not a route: **snap / flatpak**. Snap confinement grants only `$HOME`, so a
snap-delivered toolchain cannot read `/yuneta/...` — this repo already hit that
with snap-packaged CLI tools (see the note in `CLAUDE.md`), and the agent
itself writes `/yuneta`, spawns yunos, uses io_uring and dumps cores to
`/var/crash`, all of which confinement exists to prevent. What *does* work in
that family is an **OCI image as the build environment** — a container pinned
to the glibc the archives were built against, i.e. a portable form of the
matrix option.

Decide in the cold. Nothing here is urgent while every node is ours and no one
compiles on one — and less urgent since 7.8.6-3, which at least aims the one
supported glibc at a distro that is actually deployed.


## Repeated messages: filtered above the queue, per application

A queue link delivers **at least once**: when an ack is lost, the sender
resends, and the receiver stores the message twice. First real measure,
2026-09-28, `tr2check` on a stress store of 36.3M records: **6.1M duplicates**,
the same message stored twice (same key, `seq` and `tm`, stored 3.5 min apart).

**Not in `C_QIOGATE`.** Its job is to carry raw data, and it swallows
everything: it cannot discriminate by a field of the payload such as `id`.
A fleet of GPS units may all send with the same `id`, and resend when they
lose GPRS and get it back in another zone. What identifies a repeat there is
the metadata `__tm__`, the time the unit generated the message. So repeats
are filtered in a yuno ABOVE the queue, by each application, where the data
is stored (ignore a message already in the database). If `C_QIOGATE` ever
filters anything, it is by **metadata**, never by data.

(A filter in `C_QIOGATE` by key and payload field was written and reverted,
02bf70ea5 / cac246730.)

## Packaging: the agent is a SysV script, and systemd does not see an agent started outside it

`yuneta_agent` is installed as `/etc/init.d/yuneta_agent`, which systemd wraps
through `systemd-sysv-generator`. When the agent is started outside systemd
(by hand, or by an xscript), `systemctl status yuneta_agent` answers
*inactive (dead)* while the agent runs: seen on the yunovatios central
(Rocky 9.7) on 2026-09-28. An acceptance test that checks the service with
`systemctl start/status/restart/stop` reads that as a failure. Ship a native
unit, the way `yuneta-webserver.service` already is (`Type=forking` with the
daemon's pid file, or the agent in the foreground), keep `yuneta_agent22`
outside it as the escape hatch, and make the xscripts start it through
`systemctl`.
