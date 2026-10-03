# Cloud review of main

Reviewed at `5f547c5eb` (2026-10-03). The fix rounds of the previous reports
are closed: nothing pending from them. This report answers a new question:
the defects still open in `TODO.md` — are they real, are they needed, and
when. Read-only review (no build, no suite).

## 0. The fixing session's answer (2026-10-03)

Done, from the "now" items of the order below. Checks: a clean build with no
warning or error, and the tests of each fix (the full suite is not run after
a round of fixes, user rule of 2026-10-03).

| Item | Done | Test |
|------|------|------|
| Negative `from_t` / `to_t` / `from_tm` / `to_tm` | `get_segments()` resolves a negative bound against the key's last `t` (or highest `tm`), the pre-v7 meaning (`from` after `last - N`, that bound excluded; `to` up to `last - N`), and writes it back; the matcher compares signed, and an unresolved negative (a key with no record yet) bounds nothing as `from` and takes no row as `to`. The TODO's "decide" is closed as the pre-v7 meaning (restore, don't redesign) | `test_tm_order`: six cases on the three read roads, both directions, for the master, a reload and a replica (red on `5f547c5eb`: every one answered nothing) |
| CLI role collision | `yunetas build` ends with a red WARNING per yuno role installed in `outputs/yunos` by more than one build (each tree's `install_manifest.txt`); yunetas CLI 0.20.4 on PyPI (tui_yunetas `b141926`), submodule bumped. A warning, NOT a refusal: on the dev machine it names six roles today (`db_history`, `db_tracks`, `gate_auraair`, `gate_enchufe`, `gate_mqtts`: estadodelaire AND hidraulia; `gate_caudal`: hidraulia AND yunovatios), and a refusal would stop every build there -- the user's call (TODO) | `tests/test_yuno_role_collisions.py` (3 cases), and the check run on the real registry |
| MQTT will | `will__send()` asks `mqtt_acl_check(..., "write")` when it sends the will; a refused one is not published, with a warning | No test of its own (a client with a will, a subscriber of its topic and an unclean end are not staged); `c_mqtt/*` 9/9 relinked |
| MQTT `client_id` <-> user | Checked: no binding (the `clients` topic has no user link, CONNECT compares nothing). Said in the `enable_acl` description, `mqtt_broker.md` (a warning with an example) and TODO, as part of the model decision. Not built: the binding is the next cycle's model | -- |
| `test_secret_attrs` no-fallocate | `deny_syscall()` exits 2 when the filter cannot be installed | The test passes |
| `do_test_reborn_behind_overflow()` | The churn count computed without wrapping, none when the room was not made: fails instead of hanging | The test passes |

Not done, and why: `with_link_events` on in yunovatios (that project's
configuration and its nodes: for the user to decide and deploy); the
lowercased `domain_dir` (its first step is a check of the nodes, before the
next agent-facing change); the next-cycle items; section 2 (`tm` markers), a
decision for the next cycle.

## 1. Open defects of TODO.md

Verdicts: **Real** (checked against the code), **Need** (impact today),
**When** (now / next cycle / before an event / never).

### §1 Defects to fix

| Item | Real | Need | When |
|---|---|---|---|
| **rt_disk follower: two holes in the accounting of key deletes** (`count_key_delete_heard()`, `timeranger2.c:7973-8085`) | Yes, both. (a) the doubt is matched by queue position, so a second delete below the mark is taken for the first; the debt for it is never paid and the cache is cleared late. (b) a feed registered after the first feed processed the delete gets no debt and no doubt (`mirror_key_delete_to_disks()` signals feed by feed), takes the signal for a new delete and drops a key written again | Real on replicas with several rt_disk feeds per topic plus key deletes, inside a window of milliseconds while a feed opens: low probability, but silent data loss on a follower | **Next cycle**, as decided. The chosen design (a per-topic sequence persisted by the master, carried in the signal's name `.delete.<seq>.<key>`) is sound: keys cannot start with `.`, so no clash; the `IN_MOVED_TO` name already carries key and sequence, so cookie pairing is probably unnecessary. Two things to add to the design: a pruning rule for the follower's per-key "last applied seq" map, and the sequence written before the signal (monotonic across restarts and restores). The inode alternative is unsafe (ext4 on Debian 6.12 reuses inode and birth time). Hard cut, master and followers upgrade together. Red test first |

### §3 Decisions pending that are live defects

| Item | Real | Need | When |
|---|---|---|---|
| **A negative `from_t` / `from_tm` matches no record, silently** | Yes. `get_segments()` clamps the negative value in a local variable only (`timeranger2.c:13746-13757`); `tranger2_match_metadata()` re-reads the raw `json_int_t` and compares it with the `uint64_t` `__t__` (`:14119`, `__tm__` at `:14143`), so -86400 becomes ~1.8e19 and every row is left out, with no log. Pre-v7 (`tr2migrate/30_timeranger.c:3547-3555`) a negative value was **relative to the last record's t** (exclusive bound). In this repo: `command-yuno ... from_t=-86400` through C_TRANGER reaches it; `tr2list --from-t=-86400` does not (it goes through `approxidate`), so the TODO's repro does not prove it — the C comparison does | Yes: data lost at start-up in three projects (hidraulia, estadodelaire, wattyzer: their `db_history` never processes what arrived while stopped) | **Now**. Restore the pre-v7 meaning: resolve a negative value in `get_segments()` against the key's `total_to_t` / `total_to_tm` and write the absolute value back into `match_cond` (as is already done for strings); refuse with a log any negative that still reaches the matcher; the same for `to_t` / `to_tm`. A few kernel lines, no change in the projects |
| **C_TRANGER handles opened through the agent are not reaped** (`c_tranger.c`) | Yes. `watch_owner()` skips a `src` that is not a `C_IEVENT_SRV` (the agent link is a `C_IEVENT_CLI`), and there is no subscription, so only `close-*`, a topic close or a service stop releases them. No cap on lists, rts or iterators | Moderate for operators: a forgotten stateful `open-list` grows with every append for the life of the yuno; an `open-rt` publishes for ever. Only ycommand / control-center users; gui_treedb is reaped | **Next cycle**: the agent-to-yuno "session ended" notice, reaping a user's relayed handles when that user's LAST agent session ends (authz already treats one user's sessions as one owner). A cheap stop-gap now if wanted: an idle timeout, or a per-owner cap with a log, for handles opened through `C_IEVENT_CLI` |
| **A gbuffer in a published kw is shared by every subscriber** (`gobj.c:10078-10087`) | Not a defect: it is the design, unchanged (now written in `GOBJ.md` §8.13 and moved to TODO §4 "Events") (the same gbuffer object has reached every subscriber since at least 2023; `kw_twin()` for `__local__`/`__global__`, added 2026-09-25, keeps sharing it). Every action the kw passes through sees and can process the gbuffer. What is shared is also its READ CURSOR: an action that consumes (`gbuffer_get()`, `gbuffer_getline()`) moves it for the next one, one that only looks (`gbuffer_cur_rd_pointer()` + `gbuffer_leftbytes()`, or saving and restoring `gbuffer_set_rd_offset()`) leaves it. No publisher in this repo has two subscribers that consume (transports and protocols are CHILD; `C_IOGATE` copies per channel) | No change needed | **Never** as a change. At most one line in `GOBJ.md` saying the cursor is shared. (A previous wording of this report proposed a "one consumer" contract: withdrawn, it would restrict what the design allows.) Done |
| **`register_yuneta_environment()` lowercases `root_dir` and `domain_dir`** (`yunetas_environment.c:42-43`, silent) | Yes, and worse than the TODO says: the agent builds the yuno's `bin/` and `logs/` paths and its browse paths in original case (`build_yuno_private_domain()`, `c_agent.c:8072-8097`), while the yuno writes its logs, persistent attrs and `temp/` under the lowercased one. A capital in realm owner/name/role/env, role or id gives two sibling directories, and the agent's log and data commands do not see the yuno's own files | Latent: no capital in the repo's configs; production realm names unknown | **Before the next agent-facing change**. First check the nodes (`find /yuneta/realms -maxdepth 3 -name '*[A-Z]*'`). Preferred: drop the lowercasing (the agent already owns the path); if it stays, at least a warning when it changes the string |
| **C_NODE: every link collapses the whole parent, O(children)** (`tr_treedb.c:11435-11471` → `c_node.c:5363` → `node_collapsed_view()`) | Yes; `publish_treedb_event()` does not ask whether anybody listens, and C_TREEDB creates C_NODE as a service with no subscriber, so the collapse often runs for nobody | Yes for large fleets (yunovatios measured O(N²) at onboarding); harmless for small treedbs | **Now, by config, no code**: `with_link_events` is already a C_TREEDB attr forwarded to its C_NODEs (`c_treedb.c:495`, `:1038`) and switchable at run time (`set-link-events`), so yunovatios can turn it on wherever no v1 SPA reads the treedb. **Next cycle**: collapse only when the event has subscribers (payload unchanged, safe for v1). Do not drop the child lists or flip the default while estadodelaire and hidraulia are on v1 |
| **CLI: two projects with a yuno of the same role overwrite each other in `outputs/yunos`** (`project.cmake:97,106`; `yunetas build` runs `make install` per project) | Yes; nothing detects it. It also reaches `sync-binaries` with its default directory (matches by file name and role), not only `$$()` | Yes: it already caused a production incident (hidraulia ran yunovatios' `gate_caudal` for three hours) | **Now, detection only**: after each project's install, read its `install_manifest.txt` and refuse (or warn loudly) when a role was installed by another registered project (a role→project map in `~/.yuneta`). Per-project `outputs/yunos/<project>/` touches `$$()` in `helpers.c`, `sync-binaries` and the node scripts: next cycle at the earliest |
| **MQTT broker ACL: model and default-deny** | Yes. `enable_acl` defaults to 0 (`c_mqtt_broker.c:128`) and `mqtt_acl_check()` then allows all (`:3605`); with it on, a client whose groups declare no pattern is still allow-all. Authentication is required by default (C_AUTHZ, no anonymous, `enable_new_clients=0`), but **any authenticated client can publish and subscribe to any topic, `#` included**. Two gaps the TODO does not list: **the will message bypasses the ACL** even with `enable_acl=1` (`will__send()` `:2785` has no check; the CONNECT will topic is only syntax-checked); and the ACL is keyed by the client-chosen `client_id` — no binding between the authenticated user and the `client_id` was found (not traced on every path) | Depends on the deployment: low with one tenant's devices, real on a shared broker | **Now** for the will topic (a check at CONNECT or in `will__send()`) and to verify the `client_id` ↔ user binding before the ACL is called a security boundary. **Next cycle** for the A/B/C model and default-deny |
| **Schema editing: a removed column with data behind it** (`c_yui_schema_editor.js:4002-4021`, `:4433-4442`) | Yes: a generic confirm, no record read, no count | Not data loss (the records keep the field), a UX decision | **Next cycle**, as a decision |
| **Packaging: the sparse SDK serves one glibc** | Yes; `libc_guard.cmake` exists and works as described (stamp written by gobj-c, compared by every build, packagers refuse without it). A missing stamp or `getconf` is only a warning | Not while every node compiling is Debian 13 / Rocky 9 | **Before an event**: the first node of another distro that must compile |

### §2 Defects in test code (they make a test pass or hang without testing)

| Item | Real | When |
|---|---|---|
| `test_secret_attrs` no-fallocate case: when `prctl(SECCOMP)` fails, `deny_syscall()` only prints (`test_secret_attrs.c:555-557`), `_exit()` drops it, the parent checks the exit code only | Yes; the case passes untested (the ENOSPC case would fail loudly instead) | **Now**, one line (`_exit(2)` or an error return) |
| `do_test_reborn_behind_overflow()`: `size_t` loop `made/32/2 - 512` wraps (`test_rt_disk_overflow.c:1279`) and the "did not test" check does not return | Yes, but only when the test is already failing (with the default queue the threshold is ~245 KB): it hangs to the ctest timeout instead of failing | **Now**, trivial (return after the check) |
| Fixed test ports | A limitation. Note: `scripts/check_test_ports.py` is run by nobody (no build, ctest or CLI hook) and misses compile-time ports; two suites also share `$HOME/tests_yuneta` | **Next cycle**: wire the script into `yunetas test`. Run-time ports: never, unless parallel suites become a need |
| `test_c_node_link_events` intermittent | No race inside the test (synchronous, own directories); outside interference (a concurrent suite on `$HOME/tests_yuneta`, locked memory) fits better | Keep `LastTest.log` at the next failure, as the TODO says |
| `c_gss_udp_s_self_stop` phase 10 (case 11) | Accurate nit; `mt_stop` already makes the stale start a no-op | **Never**, unless that code is touched |
| "No red test" list (sampled) | Accurate; `deactivate-snap` needs a qualifier: `tr_treedb_snap` does test a failed save for activation (phase 11), only `__clear__` is untested | -- |

### Order

1. The negative `from_t` / `from_tm` (live data loss in three projects).
2. The CLI role-collision detection (already caused an incident).
3. `with_link_events` on in yunovatios (configuration, no code).
4. MQTT: the will topic and the `client_id` ↔ user check.
5. The two test-code defects.
6. Next cycle: rt_disk delete sequence, C_TRANGER reaping, collapse only with
   subscribers, MQTT ACL model, check-ports in the suite.
7. Before the next agent change: the lowercased `domain_dir`.

## 2. `mark-tm-order` and the tm markers

**What it is.** Since 7.25.5, timeranger2 keeps, per md2 file, whether its
rows arrived in `__tm__` order. A file that receives a row whose `tm` goes
back gets an empty side file `<file>.tm_unordered`, written by the master
before the md2 row (`mark_file_before_append()`, `timeranger2.c:11111-11132`);
topics created since then carry `"marks_tm_unordered": true` in
`topic_desc.json`. At load a marked file is read whole to get its exact tm
range; a query by `from_tm` / `to_tm` then skips the files whose range
misses it, and ends the scan of an unmarked file at its first row past the
range. `mark-tm-order` (C_TRANGER command; `tranger2_mark_tm_order()` in the
library) is the one-shot MIGRATION of a topic written before that: it reads
every md2 file, writes the markers it needs, and sets the topic flag.
Synchronous, master-only, it blocks the yuno while it runs (~19 ms for 30
files x 20 000 rows). The data are never changed, reordered or refused
because of `tm`: a row still goes, as received, into the file of its `__t__`.

**Who did it, and why.** A fixing session (Claude, under the `Core` author,
session `018Gxuekxy9Zq3kKfixMfSLd`) on 2026-09-23, commits `a729d9022`,
`b237e0af4`, `b5ee83d5c`, `a15dacac9`, `6357e4a8e`, `69cd45f96`, `f536b2857`,
`5830e20aa`. The defect behind it: since v7, a `tm` query assumed `tm` grows
with `t` — it ended the scan at the first row past the range, and left out a
file by a tm range taken from its first and last rows — so late or
out-of-order `tm` rows were missing from the answer (`CHANGELOG.md:3449-3462`).
Making the tm condition a plain filter fixed it, but a legacy topic then read
every row of the key: 12.7 ms (wrong) became 392 ms (right) on 30 files x
20 000 rows; the markers brought it to 0.09 ms once migrated.

**Against the principle you state** (tranger is a raw store; ordering by
`tm` is a user's concern, in another process with its own stores):

- The markers keep the data raw, but they make tranger hold a durable claim
  about `tm` order, with its own failure modes: a marker that cannot be
  written (retried only by the next append to that file), a rollback to a
  binary without markers that makes the next one trust wrong ranges until
  `mark-tm-order` runs again, a blocking migration per tranger service (three
  on the agent), two extra `file_exists()` per follower notification, the
  marker's correctness after a power cut resting on ext4 metadata order.
  About 600 lines of library and command code, ~1250 of tests, and five doc
  pages.
- **No code in this repo queries by `tm`**: `tr_treedb`, `tr_msg`,
  `tr_msg2db`, `tr_queue`, `c_treedb`, the modules and the C yunos only STORE
  it. The users are operator tools: C_TRANGER's pass-through, `tr2list`,
  `tr2search`, `tr2keys`, and gui_treedb's tm axis (bounded by the
  `list-keys` spans). External projects are not visible from here.
- Most of the 392 ms is per-row overhead, not reading: the iterator does one
  `lseek` + one `read` per 32-byte row (`read_md()`, `:14841`) plus json
  lookups per row, while the migration's `scan_md2_order()` reads 1024 rows
  per `read()` and walks the same 600 000 rows in ~16 ms. A plain filter that
  reads in blocks should land near 16-20 ms there (inferred, not measured).

**Recommendation (agrees with you): option (a), next cycle.** Keep
`from_tm` / `to_tm` as a plain ROW FILTER with no ordering assumption, and
remove the markers: no tm skip of files, no early end on tm, no migration
command. Correct always, rollback-proof, and in the spirit of a raw store;
fast tm access becomes the job of whoever needs it (a derived topic or an
in-memory index keyed by `tm`). Make it cheap by reading md2 rows in blocks
in the iterator, which helps every query, not only `tm`. What it involves:

- Delete `tranger2_mark_tm_order()`, its scan and command, the tm branches
  of append / load / `get_segments()` / the matcher, `topic_marks_tm()`,
  `segment_tm_ordered()`, `end_segment`'s tm use.
- Compatibility: stores already marked keep a stray flag and `.tm_unordered`
  files, harmless once ignored (the key listing takes only `*.md2`). No
  migration needed either way.
- Tests: `test_tm_order.c` keeps most of its assertions (the plain filter
  answers the same), `test_mark_tm_order.c` and the C_TRANGER part go,
  `marks_tm_unordered` leaves five expected jsons.
- Docs: `deploying-yunos.md` (the upgrade step), `data.md`,
  `timeranger2.md`, `performance.md`, `test_suite.md`, the two TODO lines,
  CHANGELOG. `list-keys`' `fr_tm` / `to_tm` become approximate (first and last
  rows): say so, or compute them while reading.

Not to remove in the same move: the **t** marker `<file>.unordered` (a
caller-supplied late `__t__`, since `e93761e9b`). Files are cut by `__t__`,
so the store's own order is `t`; that marker protects the store's own
structure, not a user's view. It is worth a separate look under the same
principle, but it is not the same case.
