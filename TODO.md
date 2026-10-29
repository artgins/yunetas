# TODO

Only **open** work lives here. Anything shipped is deleted from this file the
moment it ships — its record is [`CHANGELOG.md`](CHANGELOG.md) (release notes),
the docs (`yunos/c/yuno_agent/YUNO_AUTH.md`,
`docs/doc.yuneta.io/yunos/mqtt_broker.md`,
`docs/doc.yuneta.io/guide/guide_tls.md`) and git history.

Six sections, by what the next step is:

1. **[Defects to fix](#1-defects-to-fix)** — wrong behaviour, nothing left to
   decide.
2. **[Missing tests](#2-missing-tests)** — the code is fixed, the proof is
   not there.
3. **[Decisions pending](#3-decisions-pending)** — the work cannot start
   until a choice is made; several are live defects meanwhile.
4. **[Accepted limitations](#4-accepted-limitations)** — known, documented or
   deliberate, kept as they are for now.
5. **[Future features and improvements](#5-future-features-and-improvements)**
6. **[Operations and deployment](#6-operations-and-deployment)** — per-node
   procedures, rollouts and recurring maintenance, not code.

Severity in parentheses where one was assigned.

---

## 1. Defects to fix

- **`test_c_fs` stalled once in a full `-j8` suite on wattyzer** (7.27.7
  release suite, 2026-10-22; once already in the first 7.27.6 run, whose log
  was lost). Phase 0 passed; right after the test replaces the two inotify
  fds that have an io_uring read pending (`break_inotify_fds()`, a `dup2()`
  of a directory over them), NOTHING ran for 10 s -- not the 300 ms timer of
  phase 1, not the inotify completions -- until `set_auto_kill_time(10)`'s
  alarm woke the loop; the completions ("inotify read FAILED: the watcher is
  gone") were then processed during the shutdown. Not reproduced: 40 runs
  alone, 8 under `ctest -j8` of a subset, 20 while a full suite loaded the
  machine, and a second full suite: all green. Kernel 6.12 (Debian 13); never
  seen on the dev machine (7.0). The scenario is the test's own sabotage of an
  fd under a pending io_uring op, not a production path; next: arm the
  `liburing` trace from phase 1 on so a recurrence says which completion did
  not come. Log: `LastTest.wattyzer-7.27.7.log`, `test_c_fs` section, kept in
  the release notes of 7.27.7.

- **`test_fs_watcher_overflow` failed once under `ctest -j8`** (7.29.2 release
  suite, dev machine, 2026-10-29; passed alone and in a second full pass,
  348/348, and in the 25 runs before it). Case "renamed in overflow: the old
  path is forgotten": the paths were right (old forgotten, new indexed, one
  overflow), but the INFO *"watched tree rescanned after lost inotify
  events"* was never seen. The test runs the loop while `root_overflows == 0
  || fs_event->rescan_dirs`, up to 200000 turns, then 10 more: either the
  turns ran out under load before the pass ended, or the pass ended without
  its INFO. fs_watcher did not change in 7.29.2. Next: count the turns the
  loop took and print them on failure, so a recurrence says which.

- **`build_path()` never returns NULL, and 63 guards believe it does**
  (found in R45, 2026-10-29). On overflow it syslogs, stops at the segment
  that does not fit and returns `bf` with the segments that fitted
  (`helpers.c`); its API page says it returns `bf`. But 63 `!build_path(...)`
  guards (timeranger2.c 37, `c_archive_sender` 9, `c_archive_keeper` 5,
  `c_treedb` 3, `tr_treedb` 3, `fs_watcher`, `tr_queue`, `tr2q_mqtt`) and
  about 10 positive checks treat NULL as the overflow: all dead, so a cut
  path is used. Worst case seen: `!build_path(left, ..., key_archive,
  d_name) || rmrdir(left) < 0` would remove the whole archive of a key if
  `d_name` overflowed (needs paths near `PATH_MAX`). Proposed: return NULL
  on overflow and log with `gobj_log_error`, buffer content unchanged;
  callers that ignore the return see nothing new, the three that use the
  pointer were audited. A contract change of a kernel helper with ~940 call
  sites: waits for the user.

- **timeranger2 reads a date of text as NOW, in silence** (found at 7.28.9).
  The string bounds of a `match_cond` (`from_t`, `to_t`, `from_tm`, `to_tm`,
  in `timeranger2.c`) and a string tkey (`tkey_time_of_value()`) are read
  with `approxidate()`, which has no error flag: a text it cannot read is
  NOW, and the query or the append goes on with it. 7.28.9 fixed the same
  thing in `tr2list`, `tr2search` and `stats_list`. Read them with
  `approxidate_careful()` and log a WARNING with the text (it comes from a
  peer or a user); `c_tranger.c` already does it for `before`.

### Projects (code outside this repo)

Moved to the TODO of each project on 2026-10-02: `msg2db_id_incomplete()` in
the db_history alarms (wattyzer, yunovatios, estadodelaire, hidraulia), and the
url in `C_GATE_PVPC`'s logs (wattyzer).

---

## 2. Missing tests

- **`test_c_node_link_events` fails now and then** — twice on 2026-09-15,
  both inside runs of several suites, never alone; not once in the 75 runs
  logged in `build/` from 2026-09-28 to 2026-10-20. Its message was not
  kept. On 2026-10-03 one test of a serial run of the 25
  c_node/tr_treedb/c_treedb tests failed and its log was overwritten by the
  rerun (three reruns green): it may have been this one. Next time it fails,
  its output is in the run's `build/<date>.j<N>.txt` (`yunetas test` runs
  ctest with `--output-on-failure` since CLI 0.21.2).
- **Each test binary has a fixed port and a fixed directory under
  `~/tests_yuneta`**: two whole suites run at once on one machine collide
  (`ctest -j` within one run is safe since `scripts/check_test_ports.py`,
  `scripts/check_test_databases.py` and the `RESOURCE_LOCK`s of 7.26.1).
  Ports and dirs chosen at run time would end it.
- **`dba_postgres` does not link when `CONFIG_DBA_POSTGRES` is on** (it is
  off everywhere, so nothing builds it): the static `libpq.a` needs
  `libpgcommon` and `libpgport` too (`undefined reference to
  pg_strcasecmp`). Found 2026-10-05 while its `libyunetas-c_postgres.a`, a
  name no build makes, became `${MODULE_POSTGRES}`.
- **`c_gss_udp_s_self_stop` phase 11 does not prove its order** (nit): it
  relies on the subscription order for its restart to come before the start
  C_GSS_UDP_S would post. Looked at 2026-10-03: in that order the restart's
  `mt_stop` clears `start_when_stopped`, so the start is never even posted --
  the "stale start" is not observable from outside, and the guard
  `!gobj_is_running(udp_s)` of `ac_start_udp_server()` is reached only by a
  start that does not go through `mt_stop`, which no public call makes.
  What the phase proves is the outcome (the datagram after it is heard).
- **gobj-ui**: the save kw as it leaves `publish_treedb_write()`
  (`c_yui_treedb_topic_with_form.js`) is untested. Not reachable by vitest:
  the document double does not render Tabulator rows (`element.after`) nor
  `C_YUI_FORM`, and the form opens only from them (tried 2026-10-03). The way
  is a browser check on a deployed SPA (the QA drivers of gui_treedb /
  gui_agent), reading the frame the save sends.
- **No red test** for fixes made after 7.25.4: the entry point closing the log files after the leak report;
  the relink of a test after an installed archive changed; the test harness
  not counting *"io_uring_queue_init_params() pinned-memory pressure,
  retrying"* as an unexpected log (it needs the machine short of locked
  memory). In `c_agent.c`, which no ctest compiles, only the last step of
  `create-yuno`'s refusal stays untested: the -1 answer it builds when
  `build_yuno_release_name()` refuses (the refusal itself is pinned by
  `c_agent_find_new_yunos`). Not exercised live: a form Save through a real
  websocket drop.
---

## 3. Decisions pending

- **`activate-snap` / `deactivate-snap`: reload in place** (level 2; level 1
  is done, 7.28.11: only the yunos whose launch changed are restarted,
  `restart_plan.c`). Not killing a changed yuno at all, where the change
  allows it (a config row that a gclass can take live), needs each gclass to
  re-read its part of the configuration: a new contract, to decide.

  Not a route: snap / flatpak (confinement forbids `/yuneta`, io_uring,
  spawning yunos, `/var/crash`). An OCI image as the build environment is the
  portable form of the matrix option. Not urgent while every node is ours.

- **The node uuid when its recorded source disappears** (`yuneta-node-uuid`,
  after 7.29.1). Four cases left as they are:
    - a node whose recorded source is missing while another is present (an
      ethernet-mac node that sees no NIC at one boot but has DMI or a
      devicetree; a disk imaged from a box with real DMI onto one whose DMI
      is a placeholder) switches source, and its uuid changes. Keeping it
      would stop the first case moving, and make clones keep a shared uuid
      in the second;
    - a 7.29.1 ethernet-mac file whose interface is missing at its very
      first `ensure` of the new script (before `ethernet_ids` exists) is
      replaced: nothing tells this box from a clone yet;
    - the packages restart only the agents when the uuid changes: the yunos
      that run keep the old one until restarted (documented, not done);
    - containers that already hold a host-DMI uuid keep it (no uuid a node
      holds is ever changed), so sibling containers stay identical until
      `set` by hand; a container with none of the markers is not detected.
    - a random uuid that 7.29.2 stored while an image was built (no
      `hostname` recorded) is kept: containers of such an image keep sharing
      it until the image is rebuilt with a later release or `set` by hand.

---

## 4. Accepted limitations

### timeranger2 and treedb

- **A feed nobody reads any more is fed for ever.** The master feeds every
  directory of `<topic>/disks/` (a hard link per key and file), and nothing
  says whether a reader is still behind it: a reader closes its directory
  on an orderly stop, but one killed (SIGKILL, `deactivate-snap`) leaves it,
  and only its own next open takes it back. So the directory of a yuno that
  is deleted for good, or one of the form before the yuno was in the treedb
  list ids (up to 7.27.10), stays fed until it is removed by hand. No data
  is copied, but a purge cannot free the files linked there.

- **A t marker that cannot be written** (`<file>.unordered`, a late
  `__t__`) is retried only by the next append to the same FILE; a file
  never appended again keeps it missing (logged), and a reload reads that
  file's t range from its first and last rows. (`mark-tm-order`, which
  could write it again, went with the tm markers, 2026-10-03.)
- **A demoted master never takes its lock back** while the process lives; it
  needs a restart (documented).
- **In a partial topic, operations on other ids are allowed** (creates,
  updates, links); only the creates of the unloaded ids and the deletes of
  their possible parents are refused.
- **A multi-key (`rkey`) iterator** does not see keys created after it
  opened; the `key_deleted` mark reaches only the process that deleted the
  key. Single-node conveniences by design (philosophy.md, "the key").
- **`default: {}` placeholders are dropped by save + apply**
  (`prune_schema_node()`), so a `required` column whose literal really
  declared `'default': {}` loses it. The lesser of two evils, commented in
  `c_treedb.c`.
- **Apply is off on every in-tree yuno**: the agent, the control center and
  `C_AUTHZ` force `impose_c_schema`, so gui_agent's Apply cannot act on any of
  them until one stops forcing it.

### PostgreSQL

- **`C_POSTGRES` does not connect** (found 2026-10-14): the libpq socket
  polling (`PQconnectPoll` / `PQconsumeInput`, the only `EV_CONNECTED`) is
  commented out since the libuv -> yev_loop port, so it never reaches
  `ST_CONNECTED`. Restore it with a yev poll event on `PQsocket()` (restore,
  don't redesign). Its consumer `dba_postgres` is built by no configuration.
  **Left as it is, by decision (2026-10-15):** no project uses PostgreSQL;
  everything stores in timeranger2/treedb. Restore it when a use appears:
  `yev_create_poll_event()` on `PQsocket()` (no gclass uses that event in
  production yet), the old `on_poll_cb` logic (`PQconnectPoll` while
  connecting, `PQconsumeInput`/`PQflush` after), and the link of
  `dba_postgres` (section 2).

### Traces and secrets

- **The masking rule's limits** (C and JS alike). It over-masks ordinary trace
  text (`token=x is fine now` → `token=********`; a clause up to the next
  `word=` disappears; a secret's name and a `:` is read as a label, so `Wrong
  token: try again` → `Wrong token: ******** again`;
  `command_mask_secret_line()` hides its own ` <...>` marker after a secret),
  and under-masks across a newline (`password=a b\nuser=bob` shows `b`) and a
  later word containing `=` (`token=abc def==` shows `def==`, base64 padding).
  The price of passwords with blanks. A secret's name in the path before a
  query masks the query's values (`/static/token.png?v=2` -> `?v=********`,
  `?token&x=1` -> `x=********`): the word before the `=` is how 7.26.9 masked
  `/reset_password?k=hunter2`, so showing those values would show what 7.26.9
  hid.
- **The word after a bare `Bearer` that cannot be a credential is shown**, by
  decision, so that prose is not masked (`the bearer of this letter`): when it
  is not quoted, not after the scheme again, not under a secret's name and not
  at the end of a dump, and it is shorter than 8 characters, prose of 8 to 23
  (ASCII letters, each run lower case, upper case or capitalized, joined by
  `-`, `.` or `/`) or dots only. So a real token that short or of that shape,
  written bare in a text, shows (`sent Bearer abc123`); under
  `Authorization:` or a secret's name it is masked whole. The agent's audit
  record keeps its stricter reading (any word after `Bearer `). Rule and
  examples: `string_helper.md`; to undo it, `scheme_credential()` in C and its
  twin in gobj-js.
- **A name is judged by its last 128 bytes** (C, and gobj-js since 7.26.22;
  up to 7.26.21 gobj-js judged the last 128 UTF-16 units): a name over 128
  bytes whose secret part lies before them shows its value (`password_` +
  120×`é` + `_x=hunter2`). Its case is folded in ASCII, so a letter that is no
  ASCII makes no secret's name (`toKen` with the Kelvin sign `K`).
- **The name of a query string masks its value only up to a blank or one of
  `&;,"'<>`** (the run of name characters before the `=`): `?password=correct
  horse` shows `horse`, unless the whole word before the `=` is a secret's
  name.
- **A %-encoded URL or query is read decoded twice at most** (C and JS): a value encoded three times shows (`next=https%25253A%25252F%25252Fh%25253Fpassword%25253Dhunter2`), and a `%XX` of a byte over 0x7F stays encoded. Each decoding is one more pass of the rules; the bound keeps the masking linear. A json KEY is judged as written, never decoded: `{"t%6Fken": "x"}` as a dict shows `x` (no producer encodes a json key; as text, inline or in a dump, it is masked).
- **A URL password with an unencoded `/`, `?`, `#` or blank shows** (`https://u:a/b@h`, C and JS): the authority ends at the first of them, as RFC 3986 and a browser read it -- such a password is no userinfo.
- **A `#fragment` after a secret's value is masked with it** (C and JS, as in 7.26.9): the value runs to the end of the run of value bytes, and `#` is one of them, so `?token=abc#section` is `?token=********`.
- **A URL authority that reaches the end of the bytes judged is masked from its first `:` to that end** (C only: the window of `mask_secrets_inline_capped()` and the end of a dump, `mask_secrets_in_text()`; gobj-js masks the whole text and then cuts it, and has no dump), `@` or not: the userinfo ends at the LAST `@`, and the one that ends the password may lie past that end (a password written with a raw `@`, a user that is an email address). So a port there is masked (`connect ws://127.0.0.1:****`), and so is a host after a whole userinfo (`x https://u:*****************`). The colons of an IPv6 literal followed by a port are no password's (`see http://[::1]:****`) when one to five digits run from its `]:` to that end; anything else -- a literal cut before its `]`, a port with another byte, a `[` that is no IPv6 address (`[dead:beef]:`, `[a%x:pass]:`) -- is masked from its first `:`. The price: a userinfo shaped like a literal and a port whose `@` lies past the cut (`[ab::cd]:80…@h`, user `[ab`) shows `[ab::cd]:` when the cut lands inside those digits -- at the end of a dump, or in a capped line after a value masked short (caps 77-78 of `password=`+300 bytes+` url=https://[ab::cd]:80`+300 `z`+`@h/p`) --, and a literal followed by a byte that ends no authority (`[::1]:8080}x`) is masked from its first `:` whole while a cut inside its port keeps it (1 text of 100 000 stretched ones). A literal and its port take 118 bytes at most as written (a zone is 64 bytes at most), fewer than the 256 judged past a cap, so in a capped line a userinfo shaped like a literal never does it alone; %-encoded it takes three times its bytes per decoding (up to 354 once, over 1000 twice), so in the decoded view of a capped line a literal is granted at the cut only when its `[` is written raw and its bytes as written, from that `[` to the cut, are 118 at most. The two ways left are the end of a dump and a value masked short. In a capped line this reaches the bytes shown only when the authority runs from inside the cap to 256 bytes past it. A decoded `&` ends no authority. Ending it at an `@` already seen would show the password in the cases above (the capped tests of `test_secret_attrs`).
- **A json KEY is judged as a name, never as a value**: a key that is itself a
  credential (a dict keyed by tokens, `{"eyJ...": 1}`) is shown as it is
  (`json_mask_secrets()`, C and JS).
- **Secrets a trace cannot tell**: a credential in RECEIVED bytes that has no
  shape the masking knows (an HTTP credential header, a `name=value` or a
  `name: value` with a secret's name, a JWT or a JWE, the word after `Bearer`
  or `Basic` that can be a credential); the rest of a credential cut between
  two reads (the masking of received bytes is per buffer: a JWT or the word
  after a scheme that reaches the END of a buffer is masked whatever its
  shape (and a URL authority, from its first `:`), but what the next read
  brings is not); and a parameter of a command
  for a REMOTE service whose name is not a secret's name (no command table at
  hand says it is `SDF_SECRET`); a credential under a name that is no
  secret's, of a protocol the SDK does not speak: OAuth's `code=` (single use,
  minutes of life; `code` names status, error and country codes everywhere),
  `X-Amz-Signature=` and Azure's `sig=` (`signature` names webstats' attack
  signatures, `sig` a JWKS key use); a `user:password@host` with no `://`
  before it (a DSN, an scp target); a token written as the user of a URL
  (`https://TOKEN@host`).

### Resolver

- **`getaddrinfo()` runs synchronously inside the loop** (`yev_loop.c`:
  connect, source bind, listen). Every gobj, timer and completion of the
  process stops while it resolves. 7.8.2 cached the answers (one lookup per
  host per TTL) and made the cost visible (*"getaddrinfo() BLOCKED the event
  loop"*). A resolution that cannot block would be an async step in the FSM.
  Not urgent while nodes have a working `resolv.conf`.

### Auth

- **OIDC discovery requires `end_session_endpoint`.** `save_oidc_discovery`
  (`c_auth_bff.c`) and `c_task_authenticate` stop when it is absent. Auth0
  does not publish it (proprietary `/v2/logout`) and some Cognito setups omit
  it, so discovery fails there; the workaround is to set explicit
  `token_endpoint` + `end_session_endpoint`, which skips discovery. Kept as
  is; Authentik and Keycloak publish it. (Not live-tested beyond Keycloak: no
  tenants.)

### emailsender

- **A 4xx at MAIL FROM ("451 rate exceeded") costs a retry**, so a rate limit
  dead-letters the head in ~30 s, and the next ones while it lasts. By design.

### Queues

- **A queue link delivers at least once**: when an ack is lost the sender
  resends and the receiver stores the message twice (2026-09-28, `tr2check` on
  a stress store of 36.3 M records: 6.1 M duplicates, same key, `seq` and
  `tm`, stored 3.5 min apart). Repeats are filtered ABOVE the queue, by each
  application, where the data is stored. `C_QIOGATE` carries raw data and
  cannot discriminate by a payload field (a fleet of GPS units may all send
  with the same `id`); if it ever filters anything, it is by **metadata**
  (`__tm__`), never by data. (A filter by key and payload field was written
  and reverted, `02bf70ea5` / `cac246730`.) The application work is in
  yunovatios' TODO.

### Protocols

- **`C_PROT_MQTT` is not migrated to `peername` attribution**: deprecated,
  still in hidraulia production, goes away with it. `C_PROT_MQTT2` is the
  target for protocol work.

### Events

- **A published kw's gbuffer is shared by every subscriber, read cursor
  included** (the design, unchanged): every action the kw passes through sees
  and can process it; one that consumes moves the cursor for the next, one
  that only looks does not. Written in `GOBJ.md` §8.13. C_IOGATE's "send to
  all" copies per channel because each channel's transport consumes.

---

## 5. Future features and improvements

### timeranger2 and C_TRANGER

- **Bound the size of an archive batch at its cause**
  (`tranger2_archive_key_files()`). A first archive of a long history makes a
  batch of unbounded file count, and `archive_keeper` refuses anything above
  `max_files` (100000 since R44; it was 10000, which a first retention of
  about seven months on an hourly topic passed). Split one purge into several
  batches of at most N file ids, all made durable before the base, each with
  its manifest and ready mark, all removed if the base fails: the same cut
  still gives the same answer.
- **A batch whose purge did not happen should not stay visible.** A batch
  whose base was not written holds hard links to live files, so a late
  append, `tranger2_write_user_flag()` or `tranger2_delete_instance()`
  changes it after its `manifest.json` (documented in R44: a reader takes a
  file id from the batch with the latest `@<t>`). Keep the batch hidden
  (`.new-`) until after the base and settle leftovers at the next archive of
  the key (`upto` at or before the base: rename; after it: remove), the case
  of a purge that takes no files included.

- **A replica pays about 1 us per iterator open and per page for
  `check_purge_before_read()`** (since 7.27.5, `83a771089`; study of 7.27.5
  against 7.27.0). A page of 1 row: 5.25 -> 6.20 us (+18%); a filtered page
  +22%; an open +7%; a filtered open +9%. The cause: before each open and
  each page, the replica does a `stat()` of the first md2 file of the key, to
  notice a purge whose signal it did not hear. This is a correctness price,
  so keep the check. The candidates are cheaper ways to do it: one check per
  open and not per page, or skip it while nothing changed on the key. Each
  candidate needs a test that removes the signal, and an A/B on the read
  phase of `perf_timeranger2`.
- **A follower re-reads the first row of the md2 on every notification.**
  Profiled 2026-09-27 on yunovatios' `db_history_ce`: 10.9 % of its work was
  `load_cache_cell_from_disk()` → `load_first_and_last_record_md()`.
  `b2f972382` removed the per-append open/close in the common case (a
  kept-open md2 descriptor, `fstat` instead of `open`+`lseek`), but
  `update_new_records_from_disk()` still reloads a KNOWN cell: the first row
  and the last row on every append, plus a `file_exists()` for the
  `.unordered` marker. For a known cell the first row
  cannot change (md2 files are append-only, a torn tail is cut from the END):
  take the row count from the size, skip the first row, take the last row
  from what `publish_new_rt_disk_records()` reads anyway, keep `fr_t` /
  `fr_tm`. Keep the whole path for a new cell and for a marked file, and keep
  the torn-tail check. Needs a `test_rt_disk*` test and an A/B against the
  last tag; nobody has measured the descriptor path against the 10.9 % yet.
- **Share one rt_disk feed per topic across Live cards.** On a reader
  (`master:false`) C_TRANGER each `open-rt` opens a `tranger2_open_rt_disk`
  feed = one inotify instance, so N Live cards = N instances (a node sat at
  128/128 `max_user_instances` on 2026-07-12; the packages now raise it to
  4096 in `99-yuneta-core.conf`, `info-inotify` shows the usage — that only
  raises the ceiling). One refcounted feed per topic caps it at one instance
  per followed topic. It is a design change across C and the SPA, not a small
  one (read 2026-09-16): the `rt_id` a client gets back stops naming a feed
  and names a SUBSCRIPTION to a shared one, so `cmd_close_rt` and
  `reap_handles_of()` must decrement a refcount instead of closing;
  `publish_rt_callback()` stamps the shared feed's `rt_id` into every publish,
  so gui_treedb's `live_filter()` (`c_tranger_view.js`) must go back to
  filtering by key in the same change, or the cards go silent; and the master
  path (`rt_mem`) has no inotify, so decide whether the shared feed is
  unconditional (two publish contracts otherwise). Same release, coordinated
  deploy, plus a test that opens two cards on one topic and counts inotify
  instances.
- **A follower hears its OWN consumption: half its inotify queue is echo.**
  Each hard link the follower consumes is an `IN_DELETE` in a watched key
  directory, which it then ignores (*"it's me"*). Half of its queue under load
  is that echo, which is why a follower overflowed at ~3000 records/s and why
  one burst overflows it twice. An overflow no longer aborts (the feed is
  rescanned) but costs a rescan of every key. Watch the key directories
  without `IN_DELETE` (the root keeps it: the key-delete signal is a
  `.d<seq>.<key>` directory made and removed there); needs a per-level mask in `fs_watcher`, and `C_FS` still
  wants file deletes.

### archive_keeper and archive_sender

- **Nothing ends a half-open channel on the keeper's side.** `C_TCP` has no
  keepalive and its `timeout_inactivity` works for clients only, so a dead
  sender keeps its channel slot until a `begin` of the same batch takes it
  over (`stale_session_timeout`, R44); stale channels can use up the
  `__range__`. Needs a server-side inactivity close or TCP keepalive in
  `C_TCP`/`C_CHANNEL`, or a keeper timer that drops channels idle past
  `stale_session_timeout`.
- **`same_batch()` rehashes a whole batch in one event**, without yielding
  the loop. It runs only when a sender ships again a batch already stored
  (after a lost `stored`). Hash it in slices, posting an event to itself per
  N MB.

### Schema editing

- **Applying an edit restarts the whole yuno** (`kill-yuno` → `run-yuno
  play=0` → `play-yuno`), not just the treedb: its gate goes down and every
  client — the editor included — reconnects. Reopening only the treedb is not
  available to a third party and should not be (`close-treedb` destroys
  services whose handles the owner has cached). A true in-place reload lives
  in the owner, as a local method — "reload your schema" — using
  `tranger2_write_topic_cols()` (today only called by
  `tranger2_create_topic()`) plus `parse_schema_cols()` + `parse_hooks()` and
  a link reload when hooks or fkeys change.
- **Not every treedb is projected into `__system__`.** `C_AUTHZ` creates its
  `C_NODE` directly (`c_authz.c` ~557) instead of going through `C_TREEDB`'s
  `open-treedb`, so `treedb_authzs` never reaches `__system__` and cannot be
  edited; any other direct `C_NODE` consumer is in the same position.
- **The Schemas console's authz is the target yuno's, and nobody provisions
  it.** A console user needs `read` / `create` / `update` / `delete` of that
  `C_NODE` in **that yuno's** `C_AUTHZ`; an operator who can log into the
  control center still gets `-403` per topic on a yuno where they have no
  role. Part of the authz-roles work. Before diagnosing one: `descs` and
  `nodes` take DIFFERENT authz (`a_schemas` and `a_nodes` in `c_node.c`), so
  an identity with one and not the other loads the schema and is then refused
  per topic — which reads as a broken build and is a missing role.

### Inter-yuno subscriptions (decided 2026-09-25)

- **A negative ack for a refused subscription.** `__subscribing__` is one-way
  (C and gobj-js `send_static_iev()`); when `C_IEVENT_SRV` refuses one
  (`max_subscriptions`, `max_subscription_size`, authz) the only trace is a
  WARNING in the server's log, on the transition. The peer never knows: in a
  SPA the devices beyond the cap just stop updating, with nothing in the
  console, and after a reconnect a different set can go silent. Send a
  negative ack that names the event and the cause, in C and gobj-js alike;
  the client logs it and publishes it, so a view can say which data will not
  update. It changes the wire protocol: an older client must ignore it.
- **Measure what many subscriptions cost.** hidraulia raises
  `max_subscriptions` to 20000 per channel. Memory (a json per subscription
  per channel) and speed (`peer_has_subscription_room()` runs
  `gobj_find_subscribings()` on every `__subscribing__`: O(N²) at connect and
  reconnect; a publish walks the event's subscription list). Decide from the
  figures whether the default of 5000 stands and whether the count should be
  kept instead of searched.

### Resolver

- **Nameservers are tried strictly in order, 3 s × 2 each**
  (`YUNETA_DNS_QUERY_TIMEOUT`, A then AAAA): a dead first entry always costs
  6 s. Cheapest first: drop the per-query timeout, remember which nameserver
  last answered and start there, or query them concurrently and take the
  first reply.

### Auth

- **ROPC → device / client-credentials** — deferred until a non-Keycloak IdP
  is adopted. `c_task_authenticate`'s `action_get_token` uses
  `grant_type=password`, which works because every deployed IdP is Keycloak.
  All 6 callers (`ycli`, `ycommand`, `ystats`, `ytests`, `ybatch`,
  `mqtt_tui`) are headless CLI/server tools with no browser and no local
  listener, so PKCE does not fit. Interactive use → **Device Authorization
  Grant** (RFC 8628: print URL + user code, poll with
  `urn:ietf:params:oauth:grant-type:device_code`, handle
  `authorization_pending` / `slow_down`, discover
  `device_authorization_endpoint`). Headless CI (`ybatch`, `ytests`) →
  **Client Credentials** (a service-account client + secret). Scope:
  `c_task_authenticate` (new FSMs, discovery fields, config attrs) + the 6
  callers + tests + docs; keep ROPC as a Keycloak fallback. **Do not point any
  CLI at a ROPC-disabled IdP before this lands.**
- **`EVF_AUTHZ_INJECT` is declared but not enforced**: no gate exists for
  `gobj_send_event` (`gobj.h` ~333).

### gui_agent

- **Show a `max_run` stop that waits for its node** (`stop_waits_node` of
  the controlcenter's `scenarios`, after 7.29.1). While it waits, `stop_at`
  stays at its due time, in the past; the scenario view shows neither.

### gui_treedb

- **Backend features with no UI** (the SPA uses 15 of ~45 C_NODE / C_TRANGER
  commands). Highest operator value, in order: **snapshots** (`snaps`,
  `snap-content`, `shoot-snap`, `activate-snap`, `deactivate-snap`: tag a
  version, browse it read-only, roll back); **backup / restore** (`export-db`
  / `import-db`, a base64 payload maps to a browser download / file input);
  and the relationship inspector (`parents` / `children` / `links`, plus
  `jtree`, whose ready-made tree with `__path__` is ignored in favour of a
  flat `nodes` list).

### gobj-ui and gobj-js (found from yunovideo and yunomusica, 2026-10-18)

Seen while reviewing two app SPAs; the apps were fixed on their side, the
kernel was not touched.

- **The developer panel has no chips for an app's own trace levels.**
  `yui_dev.js` offers fixed chips for the framework's levels (automata,
  creation, traffic…) and nothing for a gclass's `s_user_trace_level`. An app
  that traces what happens outside its automata (yunovideo and yunomusica
  trace their stores under the root gclass's `stores` / `stores-time`) has to
  draw its own chips in its own sheet, calling `set-gclass-trace` itself. A
  generic section listing the user levels of the app's gclasses, set through
  the same yuno command (so they persist), would serve every app.
- **Tracing what is not a gobj.** Both apps grew the same `trace.js`: a
  module-level tracer bound to a gobj's user level, a `traced(store, name,
  fn)` wrapper for the functions an interface calls, a trace of each channel a
  store emits and of each browser callback it answers directly. A plain module
  close to the browser is a legitimate low-level layer; what makes it
  acceptable is that it is not a blind spot. A small gobj-js facility for it
  (named module, level, briefed arguments) would keep every app from copying
  it.
- **`yui_shell_register_overlay()` buys Back but not Escape.** Escape is a
  separate stack (`yui_shell_push_escape` / `yui_shell_pop_escape`), and
  forgetting it is silent: yunomusica's four dialogs (welcome, install, the
  replace question, a track's card) never closed with Escape. Either let the
  overlay registration take the Escape handler too, or make the pairing
  impossible to miss in the README's dialog section.
- **No `EV_VIEW_HIDDEN`, and no "is this view shown?" query.** A
  `keep_alive` view stays subscribed to whatever feeds it while hidden, and
  rebuilding there was the root of a memory leak and lost focus/scroll in
  both apps. Each view now tracks `shown` from `EV_ROUTE_CHANGED` by comparing
  the route with its own constant. `EV_VIEW_SHOWN` exists (sent on a
  re-show); its counterpart, or `yui_shell_view_is_shown(gobj)`, would replace
  that bookkeeping.
- **`validate-locales` cannot follow an app's own key helpers.** An app that
  names its controls through a helper (`named(key)`, `named_for(key, id)`)
  cannot mark a key it hands to one: the marker is refused unless the
  function is in gobj-ui's `I18N_KEY_FUNCTIONS`. Declaring app key functions
  in `package.json` (next to `attr_keys` / `array_keys`) would let the check
  see them.
- **A plural noun cannot change language in place.** `refresh_language()`
  re-translates by key and cannot pass a count, so the count nouns
  (`t(key, {count})`) carry no key and their views re-render on
  `EV_LANGUAGE_CHANGED`; a dialog open across a language change keeps the old
  word. A `data-i18n-count` (read back by `refresh_language`) would make them
  ordinary.

### Memory and jansson (from the performance round of 7.28.0)

Three decisions of the user (2026-10-24), each one with doubts: kept here to
be weighed again, not as settled.

- **gbmem's own memory manager, the pools of yuneta V6.** **FUTURE.**
  glibc's `malloc()` and `free()` were 13% to 22% of the instructions of
  the network benchmarks in the cloud audit (`perf_tcp_test4`,
  `perf_ievent`, `perf_mqtt`, `perf_auth_bff`). V6 had pools of blocks by
  size (from MAWK's `zmalloc`); the switch is still there
  (`USE_OWN_SYSTEM_MEMORY` in `gbmem_setup()`, `FALSE` in every yuno) and
  the code was not carried to v7. One thread, so no locks. The price: what
  a pool keeps is not given back to the system, so a process stays at the
  size of its peak.
- **The ESP32 copy of jansson stays as it is** (2.14 with the lexer fix by
  hand, `kernel/c/root-esp32/components/esp_jansson`). **FUTURE.** To make
  it Yuneta's jansson: a build setting for the size of the dump buffer (on
  the stack, and the yuno's task has 8 KB), a measure of the 64-bit words
  on its 32-bit cores, a build and a run on a device.
- **Slots for the first pairs inside a jansson object.** **Discarded for
  now.** Most pairs of a message would take no block of their own (some 5%
  of `perf_ievent`, estimated), for some 320 bytes more in every object,
  used or not -- and a treedb holds hundreds of thousands of them. The two
  smaller cousins of this idea (the first buckets of a table and the first
  table of an array inside their object) were measured and left out of
  `2.15.1-yuneta.1`: up to 8% more live memory, no gain in time.

---

## 6. Operations and deployment

- **Per-command authz gate — production enablement.** **FUTURE** (user,
  2026-10-03: not now; do not raise it as pending until asked). NOT by
  flipping the default: a yuno with no C_AUTHZ (gates, db_history,
  logcenter...) would refuse every external command, the agent's forwarded
  ones included (`authz_checker()` answers FALSE with no C_AUTHZ). Only in
  the five yunos that hold one, by config. `enable_command_authz`
  is default-off (YUNO_AUTH.md §4.5). Per node:
  - provision **every principal that sends commands TO each C_AUTHZ yuno**
    with `__execute_command__` / root — not only `yuneta` / admins but the
    **control center** user(s) that reach the agent's `:1993` port (the agent
    store has `yuneta` + `yuneta_admin@…` + `yunetas_admin@…`, NOT
    `yuneta_agent@…`);
  - confirm each of the 5 C_AUTHZ stores (agent, agent22 [shared],
    controlcenter, mqtt_broker, emailsender) holds the `root` / `yuneta`
    model at runtime (a store that missed `Authz.initial_load` re-seeds on its
    next master start, so this is a check, not a repair);
  - run a real **low-privilege deny test** on staging (needs a non-root
    external principal);
  - set `enable_command_authz: true` per yuno, the agent first, staging →
    production. (User management no longer waits for it: C_AUTHZ asks the
    permissions of `treedb_authzs` on its own since the cloud review of
    2026-10-02; the gate is still what guards every OTHER command, the
    agent's `install-binary` / `run-yuno` among them.)
- **Subscription gate.** `enable_subscription_authz` (YUNO_AUTH.md §4.6) is
  default-off too. Enabling it needs the same role model, and every user of a
  treedb GUI needs `read` on the C_NODE and C_TRANGER services it watches
  (`EV_TREEDB_NODE_*` and `EV_TRANGER_RECORD_ADDED` are
  `EVF_AUTHZ_SUBSCRIBE`); a refused GUI keeps its session and gets no live
  updates.
- **ytls TLS posture — per-gate rollout** (validate on staging). The support
  is in ytls and the IdP-client crypto blocks of the project batches are
  hardened (`ssl_use_system_ca` + `ssl_verify_mode=required`). Left: check
  that every client crypto block in the realm configs sets the CA (or the
  explicit `ssl_allow_insecure_client` opt-out for IoT gates); raise the
  server-side gates explicitly where wanted; set the IoT-compat profile
  (`ssl_min_version` + `ssl_ciphers "@SECLEVEL=0"`, OpenSSL backend) on legacy
  gates — no batch carries it yet.
- **Vendored libjwt: periodic re-vendor** from upstream (procedure in
  `kernel/c/libjwt/README.md`, § Re-vendor procedure). Vendored base
  `375e539`, analysed up to upstream v3.6.1, plus local NULL-safety
  backports (2026-09).
- **Memory-safety review not done yet**: `modules/postgres` (libpq wrapper;
  delegated to libpq, lower priority).

## Tests on fixed ports inside the ephemeral range

`test_c_udp_s_tx` (and probably others: check `scripts/check_test_ports.py`)
binds fixed ports such as 34281/34282, inside Linux's ephemeral range
(32768-60999). Any socket of the machine can be given one of them by the
kernel: on 2026-10-27 a local `gate_energia` held 34282 and the test failed in
the release suite of 7.28.8. Move the fixed ports of the tests below 32768
and have `check_test_ports.py` refuse one inside the range.

