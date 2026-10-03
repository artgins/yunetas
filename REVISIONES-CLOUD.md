# Cloud review of main

Reviewed up to `918c3d687` (2026-10-03): the answer to the review of
`57b562be8`, and the readiness of **7.26.0**. Read-only review (no build, no
suite). What is resolved is removed from this file.

## Verdict: the three highs are fixed; not yet ready to tag

What is left in code is two mediums and lows. What blocks the tag is the
release work itself, which the fixing session listed as not done:
`### Performance, against 7.25.22` and the report, the versions, gobj-ui
7.26.0 on npm, the two-machine suite and the doc repin.

## Resolved in this round (holds)

- **Negative bound over several keys**: one `json_deep_copy(match_cond)` per
  key in `tranger2_open_list()` and C_TRANGER `open-list`, owned by each
  iterator, the original released on every exit; no other multi-key loop
  shares one.
- **Negative `tm` bound**: resolved against the tm of the key's last record,
  read from disk (one 32-byte `pread`, fd cached), the same on the master in
  memory, a reload and a replica; errors fall back to the cache, logged.
- **A long key told to every feed** (`delete_ref_keys`, pruned with
  `deletes_applied`; master's own feeds and the overflow resolve `/`-refs).
- **Overflow during a delete**: reserve → `rmrdir` → signal on the master,
  list → read on the follower; a failed `rmrdir` burns the sequence (a gap,
  harmless), tested (`delete_seq_record`) — subject to medium 1 below.
- **`delete_seq.json` unreadable**: `-1`, nothing written, the delete
  refused before anything is removed (when a feed exists).
- **Leftover signal dirs** swept at the master's open; the overflow pass
  skips dot names; a TOLD signal advances the feed's place; the sequence
  going back is logged (no false positive in normal running).
- **The will's leak** (found while fixing): `kw_of_its_own()` gives the treedb
  a `kw_twin()` when the kw carries a gbuffer; the reference count balances on
  every exit of `mt_create_node` / `mt_update_node`; it also covers C_TREEDB,
  C_AUTHZ and the MQTT broker (they all go through `gobj_create/update_node`).
- gobj-ui `8dd3893`: one re-read in flight per loaded parent, a dirty flag
  for the rest, no lost update; `expect_no_debts()`; the skeleton test on its
  own tree; emailsender window 800 ms; `c_mqtt/will_acl` (port 18118 unique,
  packets checked).
- CHANGELOG intro and `### Upgrade steps`; `deploying-yunos.md` "Upgrading to
  7.26.0".

## Open

### Medium

1. **A failed record of the delete sequence lets the delete go ahead, and
   reopens the overflow race** (`next_delete_seq()` `timeranger2.c:4680-4708`,
   called by `reserve_delete_seq()` ~`:4782`). On a failed
   `replace_json_file()` it logs *"Cannot record the delete sequence"* and
   returns the new number anyway. A follower reads `delete_seq.json` from
   disk, not the master's memory: in that window it can list `keys/` without
   K and still read N-1, set `applied[K]=N-1`, and take `.dN.K` for a second
   delete — the race this round closes. On a read-only filesystem, ENOSPC,
   EIO. *Fix:* in `reserve_delete_seq()`, refuse the delete on a failed
   record and roll back the in-memory sequence (before the key is removed,
   refusing costs nothing); keep "use it anyway" only for the lazy call inside
   the mirror.
2. **gobj-ui: a background parent re-read that fails while connected opens
   the app-wide error modal** (`c_yui_treedb_topics.js`,
   `ac_mt_command_answer`): the new `command === "node"` branch catches only
   the disconnected case. A force-delete of a parent whose row is loaded
   publishes `UNLINKED` per child; the re-read reaches the backend after the
   delete, `cmd_get_node` answers *"Node not found"*, and every open viewer
   gets a modal for a refresh nobody asked for. Older than `8dd3893`, but that
   commit touched this exact branch. *Fix:* settle the entry and log a warning
   on any failed `node` re-read; never the modal. Two related edges: an entry
   whose answer never comes stays in `parent_rereads` for good (only a drop
   clears it); and a late `-1` for a read cut by a drop, arriving after the
   reconnect, deletes the NEW read's entry and shows the modal.

### Low

- **The sweep "moves out" nothing**: `sweep_left_delete_signals()`
  (`:6925-6926`) builds `away` as `feed_path/../<name>`, and `build_path()`
  locks its first segment as the root and drops the `..` — the signal is
  renamed INSIDE the feed's dir. It still ends unheard (no `IN_MOVED_*` on the
  client watch, `.stale_signal.*` fails `parse_delete_signal()`), but the
  function header, the commit and the answer say otherwise; the test's check
  for `.stale_signal.` in `disks/` can never fail; and if its `rmdir()` fails,
  nothing ever sweeps `.stale_signal*` names. Build the path from `disks/`.
- **The sweep's "last one" rests on a topic-wide `last_seq` that can be
  wrong**: a sequence burned by a failed `rmrdir` raises it (a real last
  leftover is then removed unheard); a failed record (medium 1) lowers it
  (old leftovers heard late); an unreadable record (`last_seq = -1`) marks
  every leftover heard — the hazard the sweep exists to avoid.
- **A refusal happens only when `disks/` holds a feed**: with none,
  `reserve_delete_seq()` returns 0 and the delete proceeds; a feed appearing
  before the mirror runs is then not told (the lazy `next_delete_seq()`
  fails, logged).
- **Silent or false errors**: `readdir()` without the `errno` check in
  `reserve_delete_seq()` (a failure reads as "no feed" and skips the
  reservation) and in the sweep (leftovers stay, no log); the mirror's new
  `break` after a failed `next_delete_seq()` leaves `errno` set, so a false
  *"readdir() of disks/ FAILED"* follows.
- **`check_master_delete_protocol()` logs an ERROR** when `delete_seq.json`
  is missing, also for a new master that has not opened the topic yet
  (followers started first on the upgrade boot); nothing says it cleared.
  The CHANGELOG asks followers to stop before and start after the master,
  but `deploying-yunos.md` calls a node-wide `upgrade-yunos` (all at once)
  acceptable "the follower logs once": say it is an ERROR per feed, and that
  it is expected there.
- **`c_node.c` `kw_of_its_own()` (~5750) with `hand_files_to_record()`
  (2532)** (latent, reviewer's trace, not proven end to end): when the
  gbuffer was put into a record dict still nested in the caller's kw, the
  twin is taken and freed through the treedb while the caller's dict keeps
  `"gbuffer"` with a released pointer. Nothing reads it today; a second
  holder that decrefs that dict would. Delete the key from the original after
  twinning (the reference moves instead of being copied).
- **`get_segments()` ~`:14019` clamps a resolved negative `from_tm` to
  `total_from_tm`**, the cache's lowest tm — exact on the master in memory,
  first/last-row approximate after a reload or on a replica: rows in
  `[from_tm, total_from_tm)` kept on one, dropped on the other. The clamp
  selects nothing more; remove it.
- **The realtime half of a multi-key `tranger2_open_list()`**
  (`:15832/15843`) gets the original `match_cond`, negative bounds
  unresolved: a negative `from_tm` is no bound there, a negative `to_tm`
  rejects every row — unlike a one-key list. Defensible (no single "last
  record" for all keys), but undocumented.
- **"The pre-v7 meaning" is overstated**: in `tr2migrate/30_timeranger.c:3343`
  the last record was the TOPIC's, of any key; per key is a v7 choice — say
  so in the comment and the CHANGELOG.
- **CHANGELOG**: the intro says "three changes of behaviour" while the
  upgrade steps list five; the tm-marker API removal and `write-attr` carry no
  **BREAKING** tag; the C_NODE leak entry names only the will, but
  `retain__store()` (`c_mqtt_broker.c:2588`, a retained PUBLISH with payload)
  leaked the same way (fixed by the same change, untested).
  `deploying-yunos.md` "Upgrading to 7.26.0" omits that a `db_history`
  started with `from_t=-86400` now LOADS a day at start (memory, start time).
- **Small**: `key_of_delete_ref()` rebuilds the whole index on every miss
  (per call, not once); `key_last_record_tm()` logs a bare `file_id` as the
  path; a microsecond race at the overflow (a key written and deleted again
  between the follower's listing and its read keeps the key); `will_acl` steps
  on fixed 300 ms timers without waiting for CONNACK/SUBACK, and its leak
  check is vacuous where `CONFIG_DEBUG_TRACK_MEMORY` is off (wattyzer);
  `expect_no_debts()` only restates prune's predicate.

## Release 7.26.0: what is still missing

1. Medium 1 (it is the race of this cycle's protocol); medium 2 can ride
   gobj-ui 7.26.0.
2. `### Performance, against 7.25.22` and the report (A/B, `.html` + `.json`,
   the rows in `reports/README.md`, `README.md`, `performance.md`), stating
   the tm query on a topic marked in 7.25.22 going 7.4 → ~14.5 ms and the
   fsyncs per key delete on topics with feeds.
3. `YUNETA_VERSION` 7.26.0, `RELEASE` **1** (it is 3), CLAUDE.md's
   "7.25.22".
4. gobj-ui 7.26.0 on npm (`8dd3893` is pushed, not published), the pointer,
   `^7.26.0` in gui_agent / gui_treedb, `verify_js_api_coverage.py --repin` +
   `--write`.
5. The two-machine suite on the final HEAD.
6. At tag time: `check_doc_line_refs.py --repin=7.26.0`, myst cache cleared,
   `deploy.sh`, live site checked; the release body from the CHANGELOG.
