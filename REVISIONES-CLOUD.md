# Cloud review of main

Reviewed up to `57b562be8` (2026-10-03): everything since the TODO-defects
report (`c80c7fc68..57b562be8`, 24 commits), and the readiness of **7.26.0**.
Read-only review (no build, no suite). Every finding below was checked
against the code.

## Verdict: not ready for 7.26.0

Two defects in this round's own fixes break the cases they were made for
(items 1 and 2), the release text is incomplete, and the release rules that
apply (two-machine suite, performance report) have not run on this HEAD.

## Resolved in this round (holds)

- **tm markers removed** (`4274d33e2`): no path skips a file or ends a scan
  by `tm`; the `t` marker (`.unordered`) is intact; a stray
  `marks_tm_unordered` or `.tm_unordered` is ignored (tested); nothing calls
  the removed APIs.
- **Block reads** (`161926278`) and **match condition parsed once**
  (`a395aa34a`): a block never reads past the segment's rows, offsets and the
  backward window are right, short reads are logged, `md_write_gen` covers
  every md2 write; every parse runs after `get_segments()`.
- **Delete sequence** (`0c2af2a6b`): persisted durably (tmp + fsync + rename
  + dir fsync) before any signal carries it; name format fits `NAME_MAX`;
  keys cannot look like a signal; hole (b) (a feed opened after another heard
  a delete) and the second-delete-in-doubt case are closed; a follower
  restart replays nothing.
- **`with_link_events` on by default**: the three defaults are `1`
  (`c_node.c:393`, `c_treedb.c:495`, `c_authz.c:342`); no in-repo C
  consumer relied on the parent `UPDATED`; gobj-ui's topics and graph handle
  `LINKED`/`UNLINKED`; the v1 recipe is in the CHANGELOG.
- `jtree` without `rename_hook`, C_NODE `parents`/`children` without
  options, the case of `domain_dir` kept, the MQTT will under the ACL
  (`client_id` right, checked before a volatile client goes), CLI role
  warning, the two test hangs, the new red tests.
- Guards: `verify_api_coverage.py` 862/862, `api_index.py --check`,
  `check_doc_line_refs.py`, `verify_js_api_coverage.py`, the schema diagram
  `--check`: all exit 0.

## Open

### High

1. **A negative bound in a list over several keys is resolved against the
   FIRST key and reused for every other** (`timeranger2.c:13486-13491`, and
   the `to_t` / `from_tm` / `to_tm` twins). `get_segments()` writes the
   resolved value back into `match_cond`, and `tranger2_open_list()` hands
   every key the same object (`:15323-15327`, `json_incref(match_cond)`);
   the same in C_TRANGER's `list` (`c_tranger.c:2211-2215`, `:2263`). The
   first key turns `from_t=-86400` into its own absolute time; the next keys
   see that value: a device that stopped earlier gets nothing, one ahead gets
   more than a day. `trmsg_open_list()` (`tr_msg.c:322`) opens exactly such a
   list with no key — very likely the `db_history` path of the three projects
   the fix was for. The multi-key iterator is safe (it deep-copies per key).
   `test_tm_order` uses single-key roads only.
   *Fix:* `json_deep_copy(match_cond)` per key in both loops (keep the
   write-back in each iterator's own copy); a test with an `rkey` / no-key
   list and two keys of different last `t`.
2. **A key longer than the signal name allows (233-255 bytes) is told to the
   FIRST feed only** (`timeranger2.c:7360-7393`, `key_of_delete_ref()`
   `:8233`). The `/<sha256>` ref is turned back into the key by scanning the
   cache; the first feed's `DELETE_FIRST_HEARD` removes the key from the
   cache, so every later feed finds no key, `named` is FALSE and its
   callback is skipped, with no log. The overflow path skips such refs too
   (`tell_deletes_lost_in_overflow()`, `/`-prefixed refs). `do_test_odd_keys`
   opens one feed. *Fix:* keep `{ref: key}` with the applied delete until
   every feed has heard it; a test with two feeds and a long key.
3. **An overflow during a concurrent delete can still give a double delete**
   (`tell_deletes_lost_in_overflow()` ~`:8868-8915`). It reads `now_seq`,
   then lists `keys/`, and takes a key missing there as deleted at
   `<= now_seq`. The master removes `keys/<key>` BEFORE it bumps the
   sequence (`tranger2_delete_key()` ~`:4970` → `next_delete_seq()` `:4643`,
   with a tmp write and fsync between, milliseconds): the follower records
   `applied[k] = N`, tells the delete, then hears `.d(N+1).k` as a new one
   and drops the cache entry again — possibly a key another feed has
   reloaded. Under load, when bulk deletes run. *Fix:* the master bumps and
   persists the sequence BEFORE removing `keys/<key>`, and the follower reads
   its bound AFTER listing; a test.

### Medium

- **A failed read of `delete_seq.json` rewinds the master's sequence for
  good** (`next_delete_seq()` `:4643-4650`): a read error (logged) becomes
  `seq = 0`, then `1` is written durably over the real record; a valid file
  without the key reads 0 with no log. Then (or after a backup restore, or a
  topic copied without the file) followers misbehave silently: their
  `delete_seq_heard` / `delete_seq_max` only grow, so new entries are pruned
  at once and later feeds take the signal for a first delete again; on the
  master a reused number can meet a leftover signal dir (`EEXIST`, logged).
  *Fix:* refuse to signal (and do not overwrite) when the read failed; log a
  sequence that goes back.
- **The old/new delete protocol mismatch is silent in both directions.** A
  7.25.22 follower takes `.d<seq>.<key>` as a key (callback with a bogus key,
  the real delete never heard); a 7.26 follower of a 7.25.22 master hears no
  delete at all — this direction is not in the CHANGELOG. "Upgrade together"
  is a doc note only. *Fix:* a protocol version in `__timeranger2__.json`
  (or the topic) checked by the follower, refusing loudly.
- **A negative `tm` bound is resolved against an approximate "highest tm"**
  (`:13572`, `:13608`): since the markers are gone, a file's tm range at load
  comes from its first and last rows only, so a master in memory, the same
  master reloaded and a replica resolve `from_tm=-N` against different
  values (rows tm 100, 900, 150: 900 vs 150). The CHANGELOG, docs and tests
  say "highest tm". *Fix:* document it as approximate (it is, now), or derive
  it from the rows the scan reads.
- **`with_link_events`: the gobj-ui topic table brings the per-link parent
  collapse back** (`c_yui_treedb_topics.js:2807-2822`):
  `ac_treedb_node_linked` answers every `LINKED`/`UNLINKED` with
  `treedb_get_node()` of the parent — the collapsed parent with every child
  id — so with a GUI open on the parent topic, each link costs O(children)
  per viewer again, the cost the BREAKING change was made to remove. *Fix:*
  coalesce per `(parent_topic_name, parent_id)`, or re-read only a loaded /
  visible row.
- **`test_rt_disk_overflow`'s `expect_no_debts()` is always true now**
  (`:521-535`): it reads `feed["deletes_unheard"]`, a key `0c2af2a6b`
  removed; called at 11 sites. Check `topic["deletes_applied"]` instead, as
  `test_delete_key_propagation` does.

### Low

- A signal dir left by a master crash between `mkdir` and `rmdir` (or a
  failed `rmdir`) stays in `disks/<rt_id>/` for good; that feed never hears
  the delete, and an overflow rescan hands `.d<seq>.<key>` to
  `defer_key_dir_scan()` (no dot filter on that path).
- `deletes_applied` can grow without bound when a watched feed's directory
  cannot be written by the master (mkdir `EACCES`, logged on the master
  only); `hear_delete_signal()` returns `DELETE_TOLD` before advancing
  `delete_seq_heard`, and the overflow path never advances it.
- `key_of_delete_ref()` hashes every long key of the cache for every
  long-key signal, on every feed.
- `yuno_skeleton/templates` falls back to `/yuneta/development/yunetas` and
  to the installed `/yuneta/bin/yuno-skeleton` (`test_yuno_skeletons.sh:20-24`):
  in a worktree run without `YUNETAS_BASE` (the wattyzer release flow) it
  tests the live tree. Set `YUNETAS_BASE` from the test's own source dir and
  fail instead of falling back. Its second run can pass without `C_SKTSVC`
  (grep `'gclass': 'C_SKTSVC'`).
- `emailsender/url_change_resets_pacing` window cut to 300 ms (the failing
  case takes ~7 s): a thin margin under load.
- No test for the MQTT will under the ACL.
- `8f90a384e` names five nodes checked for capitals; estadodelaire's node is
  not among them — check it if it is a host of its own.

## Release 7.26.0: what is missing

1. **Items 1-3 above** fixed, with their tests.
2. **CHANGELOG `## Unreleased`**: coverage is complete, but it has no intro
   paragraph, no `### Upgrade steps (operators, read first)` (7.25.21 and
   7.25.22 have one) and no `### Performance, against 7.25.22`. The upgrade
   steps must gather: the tm-marker API and `mark-tm-order` removal (rollback
   to 7.25.x needs its `mark-tm-order all=1`); the delete-signal protocol
   (master and followers of a topic together, BOTH directions); the
   `with_link_events` default (v1-SPA yunos set it `0` on C_TREEDB and in
   C_AUTHZ's kw; hosts that subscribe to everything declare
   `LINKED`/`UNLINKED`; GUIs need gobj-ui >= 7.25.26); `write-attr` now writes
   only `SDF_WR` attrs. The tm-query cost figures disagree (~0.39 s, 0.439 →
   0.168 s, 0.152 → 0.0145 s): one net A/B figure.
3. **`deploying-yunos.md`**: nothing on the delete-signal protocol, the
   `with_link_events` default or `write-attr`; only the tm-marker section.
   `performance.md:340` still says a tm query costs ~0.39 s.
4. **Versions**: `YUNETA_VERSION` 7.25.22 → **7.26.0**; `RELEASE` is **3**
   → reset to **1**; CLAUDE.md's two "7.25.22" (SDK line, Useful Files).
   Tag `7.26.0` is free.
5. **JS packages** (the SDK minor moves): the next gobj-ui release is
   **7.26.0** (one comment-only commit past 7.25.26 already — the JS coverage
   guard warns about three moved files), then the submodule pointer, the
   `^7.26.0` range in gui_agent / gui_treedb, and
   `verify_js_api_coverage.py --repin` + `--write`. gobj-js: no code to
   publish; its next release takes 7.26.x.
6. **TODO.md**: pruned; only the parenthetical "(Red tests added
   2026-10-03: …)" in §2 records shipped work — remove it.
7. **Release rules that apply** (not LITE: kernel changes):
   - the **two-machine suite** on the final HEAD — local under
     `ulimit -Sn 1024` and wattyzer (its last run was on `0db20a63d`);
   - the **performance report** (block reads, matcher, tm-marker removal,
     delete sequence on hot paths): A/B against 7.25.22,
     `performance/reports/7.26.0.html` + `.json`, the rows in
     `reports/README.md`, `README.md` and `performance.md`. It must state
     the tm query on a topic marked in 7.25.22 going from 7.4 ms to ~14.5 ms
     (the price of the removal), and the two fsyncs per key delete on topics
     with rt_disk feeds;
   - at tag time: `check_doc_line_refs.py --repin=7.26.0` (1705 + 9 links
     on 7.25.22), myst cache cleared, `deploy.sh`, live site checked; the
     GitHub release body from the CHANGELOG without the 4-space indent.

## Order

Items 1, 2, 3 (high) → the `delete_seq.json` read and the protocol version →
`expect_no_debts()` → the CHANGELOG upgrade steps and `deploying-yunos.md`
→ the rest of the mediums and lows that fit → versions, JS packages → the
two-machine suite → the performance report → tag.
