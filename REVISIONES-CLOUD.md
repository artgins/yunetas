# Cloud review of 7.25.21

Review of what main carries after review round 21 was merged: the 113
commits `6774691e7..96d9f5a60` (7.25.21), about 7,400 lines of C plus
tests and docs. Read only: nothing was fixed in this pass. Date: 2026-10-01.

## Verdict

**The changes are good.** The release builds clean and passes the whole
suite. No leak, double free, use-after-free or refcount imbalance was found
in the reviewed code. Two real defects remain in emailsender (mail left
unsent), plus two gaps in secret masking and one in the cloud SessionStart
hook. None of them is covered by a test.

## Build and suite

- Clean build (`yunetas clean && yunetas init && yunetas build`): no
  compiler warning.
- Full suite as user `yuneta`, under `ulimit -Sn 1024`
  (`fs.inotify.max_user_instances=4096`): **275/275 passed**, 1141 s.
- `timeranger2/test_delete_key_propagation` passes here, but this container
  runs kernel 6.18. The known limitation of the CHANGELOG (Debian 6.x +
  ext4) was not exercised.

## Scope

| Area | Files |
|------|-------|
| gobj-c | `command_parser`, `gbuffer`, `glogger`, `gobj`, `helpers`, `kwid` |
| timeranger2 | `timeranger2.c`, `fs_watcher`, `tr_queue`, `tr2q_mqtt`, `tr2check`, `fs_watcher` util |
| root-linux / yev_loop | `c_tcp`, `c_tcp_s`, `c_gss_udp_s`, `c_iogate`, `c_ievent_srv`, `c_yuno`, `c_fs`, `c_assets`, `dbsimple`, `msg_ievent`, `yev_loop`, `static_resolv` |
| yunos | emailsender, webstats, yuno_agent, controlcenter, watchfs |
| tooling | `.claude/hooks/session-start.sh`, `scripts/check_test_ports.py` |

## Defects

### 1. emailsender: mail queued before `set-email-user` is not sent

`yunos/c/emailsender/src/c_emailsender.c:499-503` (`cmd_set_email_user`).

The session now connects only when it holds a message (`connect_on_start`
0, `c_smtp_session.c:358`, `request_connection()`). `set-email-user` calls
`start_smtp()` and nothing else, so no message reaches the session, no
connection is made and no `EV_ON_OPEN` arrives. The comment above the call
("its EV_ON_OPEN sends the queue") no longer holds.

Scenario: the yuno plays without credentials, mail is queued, the operator
runs `set-email-user`. The queue waits until a new email is enqueued or the
yuno is paused and played. `mt_play` and `cmd_skip_email` call
`tira_dela_cola()` after `start_smtp()`; this command does not.
`main_set_email_user` sends its email after the command, so it does not
see it.

### 2. emailsender: `skip-email` during a transaction stalls the queue

`c_emailsender.c:782-817` (`cmd_skip_email`) with the session FSM.
Found by reading, not reproduced.

`gobj_stop(priv->smtp)` is asynchronous: the session stays in its
transaction state (`ST_WAIT_MAIL_FROM_RESP` ... `ST_WAIT_DATA_RESP`) until
the C_TCP reports the disconnect. The command then sends the next head of
the queue at once, `EV_SEND_MESSAGE` is not declared in that state
(*"Event NOT DEFINED"*), the emailsender goes to `ST_IDLE` with no current
email, and the later detached close publishes nothing that resends it.
The `skip` test only skips a message waiting in `ST_DISCONNECTED`, with
nothing queued behind it.

### 3. Secret echoed in the "extra parameters" answer

`kernel/c/gobj-c/src/command_parser.c:949-967` (`build_cmd_kw`).

`extra_is_secret` covers a secret given as `key=value` only. A required
`SDF_SECRET` parameter given positionally with blanks
(`login correct horse battery`) takes `correct`; the rest has no `=`, so
`last_key` is NULL, `mask_secrets_inline()` masks nothing, and the command
answer carries `'horse battery'` in clear. The trace side
(`command_mask_secret_line`) masks it correctly.

### 4. The `ievents` trace masks by name only

`kernel/c/root-linux/src/msg_ievent.c`, `trace_inter_event()`.

The comment says it uses `ievent_kw_masked()`; the code uses
`gobj_trace_json_masked()`, which masks by key name and `name=value`. The
command table (positional `SDF_SECRET` parameters) is applied in
`ievents2` only, so a command line in `__md_iev__` with a positional
password can reach the `ievents` trace in clear.

### 5. SessionStart hook aborts with `YUNETAS_HOOK_BUILD_C=1`

`.claude/hooks/session-start.sh:34` (`set -euo pipefail`) and `:106`
(`source ./yunetas-env.sh`).

`yunetas-env.sh:34-35` reads `$dir` and `${pwd_opt}`, which nothing sets;
under `set -u` the source fails with *"unbound variable"* and the hook
exits 1 before the ext-libs check and the C build. Without that variable
the hook is not affected.

## Risks (design or performance, not wrong today)

- **timeranger2, delete-heavy followers.** `client_key_deleted()`
  (`timeranger2.c:7144`) closes the key's read fds through
  `close_fd_files()` (`:3540`), which walks every open key instead of looking
  the key up, once per watched feed: a purge on the master costs a follower
  O(feeds x open keys) per delete. `count_key_delete_heard()` also calls
  `fs_queued_events_end()` (an `ioctl(FIONREAD)` plus a CQ ring scan) once
  per other feed for each delete; the same cost was chunked for new keys,
  not for deletes.
- **timeranger2, `dir_identity`.** Without `STATX_BTIME` (some NFS, ext4
  with 128-byte inodes) the identity is the inode alone, and the
  delete-and-rewrite fix falls back to the 7.25.20 behaviour. Not
  documented.
- **timeranger2, `place_key_dir_scan`.** `char entry[96]` holds five 64-bit
  numbers (about 104 bytes worst case) and the `snprintf` truncation is not
  checked. Practically unreachable, but a truncation would skip a scan
  silently.
- **C_TCP_S stop waits for its clisrvs.** A listener stopped and started
  again stays in `ST_WAIT_STOPPED` until every clisrv reaches `ST_STOPPED`.
  One close that never ends (a peer that never drains) keeps it from
  listening again, with no log beyond `TRACE_LISTEN`.
- **C_TCP_S sharing one channel pool.** Two servers on one pool both write
  `tcp_s` on the same clisrvs; only the last one sees them when it stops or
  counts. Pre-existing design, made visible by the new stop.
- **Agent audit log.** Redaction now uses the shared `is_secret_name()`,
  whose exempt segments (`max`, `mode`, `type`, `pub`, ...) win over a secret
  part: a name like `api_key_max` or `token_mode` is written in clear. No
  audit test covers it.
- **Subscription traces in `gobj.c`.** `_delete_subscription()`,
  `gobj_subscribe_event()` and the `__filter__` trace of
  `gobj_publish_event()` still use plain `gobj_trace_json()`; a
  `__filter__`/`__config__` carrying a credential is printed as is.
- **Traffic dumps.** `mask_secrets_in_text()` works per buffer, so a
  `password=` split across two received gbuffers shows its value in the
  second dump. A stated limit, worth saying in the docs.

## Nits

- `gbuffer_serialize()` reads `gbuf->secret` before any NULL check.
- `parameter2json()` parses `SDF_SECRET` json parameters with the verbose
  parser, which logs the raw text on a parse error.
- `trace_inter_event2()`: a NULL from `ievent_kw_masked()` makes `json_pack`
  fail with nothing logged.
- `c_smtp_session.c`: `refused_in_row` is incremented before a `json_pack()`
  that can fail; the CRLF and `.\r\n` appends are unchecked (capacity is
  enough).
- `scripts/check_test_ports.py`: a file that cannot be read is skipped
  silently, and the file handle is not closed.
- `yuno_config_file.c`: the temp name is 8 bytes longer than the final one;
  temp files of a yuno never relaunched are not cleaned.
- `fs_watcher.c` `watch_again()`: a stale wd whose `IN_IGNORED` was lost in
  an overflow stays for good (documented in its comment).

## Checked and correct

- **Subscriptions:** the refused subscription is removed from both lists and
  released; a removed subscription zeroes `publisher`/`subscriber`, so a
  stale list is safe and the publish loop skips it.
- **Secret masking:** buffer bounds of `mask_secrets_inline()`, the memo
  and budget of `json_mask_container()` and the linear scans of
  `mask_secrets_in_text()` are sound. All masking cost sits behind trace
  flags or on error paths, so the hot paths gain nothing.
- **SMTP:** AUTH lines are sent as secret gbuffers and the stack copies are
  wiped. Multi-line replies, dot-stuffing and CRLF injection checks are
  correct, as is the msectimer pacing. Each message is resolved once.
- **Atomic writes:** `dbsimple` (`mkostemp` 0600, fsync, rename, fsync of
  the dir, `O_NOFOLLOW`) and the agent's config files (`fstatat`
  `AT_SYMLINK_NOFOLLOW` + `unlinkat`) are correct.
- **Network:** C_TCP's close drops are reported on every path, and the
  C_TCP_S stop/start races are closed with posted events. C_GSS_UDP_S
  backoff is sound. `c_iogate` `send_all()` balances its kw and gbuffer.
- **Agent and controlcenter:** `kw_incref` replaces `json_incref` at every
  site. Routing now trusts only a top-side channel or a recorded link. Timer
  and stats handling are correct.
- **webstats:** the DST gap and repeated hour are handled in `local_day`;
  the IPv6, range and zone id rules hold in `ip_literals`.
- **On-disk compatibility:** no new persisted state in timeranger2, so
  existing stores are unaffected.
- **Dismissed:** freeing a dup/dup2 accept event in `yev_loop` cannot leave
  a pending SQE pointing at it. `forget_kept()`, called first in
  `really_free_yev_event()`, removes the event's own entries from the
  submission queue.

## Suggested order

1. Defects 1 and 2 (mail left unsent in production; small fixes, each with
   its red test).
2. Defects 3 and 4 (secrets in an answer and in a trace).
3. Defect 5 (hook).
4. The timeranger2 delete cost, measured on a follower with many open keys
   before deciding.

## Status (2026-10-01)

Defects 1 to 5 are fixed, each with a test that fails on the code before it
(the hook excepted: verified by sourcing `yunetas-env.sh` under
`set -euo pipefail`): `emailsender/set_user_queued`,
`emailsender/skip_in_flight`, and two checks in `secret_attrs`. The NULL of
`ievent_kw_masked()` in `trace_inter_event2()` (a nit) is fixed with
defect 4. See `CHANGELOG.md` "Unreleased".

The other six nits are fixed too, each with a test that fails before it
(`gbuffer`, `secret_attrs`, `test_yuno_config_file`,
`test_fs_watcher_overflow`), except the emailsender appends, reached only out
of memory, and `check_test_ports.py`, checked by hand with a file it cannot
read. The note on `refused_in_row` is kept as it was, with a comment: the
server did refuse the message, so it counts even if the answer cannot be
built.

The eight risks are dealt with: the delete cost of timeranger2 measured (a
follower with 50000 keys open: 4.3 ms per delete and feed, 17 us after
looking the key up; the per-feed cost of `fs_queued_events_end()` kept, linear
in the feeds, measured and documented), the stop of a `C_TCP` bounded by
`timeout_stop_tx`, a pool of the new method no longer taken by a second
`C_TCP_S`, the audit record redacting by `is_secret_name_any()`, the
subscription traces masked, a filesystem without birth time said, the scan
note sized, and the per-buffer limit of the traffic dumps documented. See
`CHANGELOG.md` "Unreleased".
