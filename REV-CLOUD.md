# Cloud review of main

## Review (2026-10-04): the C suite takes more than half an hour

Reviewed at `e589eb8` (2026-10-04).

Question: many ctest tests must run one after another (they depend on each
other, or used to share ports), and `yunetas test` takes more than 30
minutes. Can it be faster?

Status: **done, released in 7.26.1, 7.26.2 and 7.26.3** (see *Result* just
below), **and verified** (see *Verification*: one bug and some gaps left).
The analysis that follows them is kept as it was written: measured in a cloud
container (4 cores), plus an audit of what the tests share.

### Result (2026-10-05)

Every item of the proposal was applied, item 5 further than proposed, and
item 6 with option (ii). On the dev machine (8 cores), `yunetas test` with no
change in the sources went from ~31 min (1250 s of ctest alone at 7.26.0) to
**~4 min** (226-278 s in all, ~6 s of it building).

| Item | Done in | Commit | What changed from the proposal |
|---|---|---|---|
| 1. no `make clean`, `make -j` | CLI 0.21.0 | tui_yunetas `7ba50c9` | As proposed; `--clean` keeps the old behaviour. Also the `MarkupError` of `main.py:64-66`, fixed. |
| 2. `ctest -j` | CLI 0.21.0 | tui_yunetas `7ba50c9` | As proposed, gated on SDK >= 7.26.1 (`CTEST_PARALLEL_SINCE`); `--jobs`, `--serial`. |
| 3. declare what the tests share | 7.26.1 | `7c05be021` | The yunetas patch as written. The sibling of `check_test_ports.py` for shared directories came after: `scripts/check_test_databases.py` (`13e55722a`). |
| 4. `RUN_SERIAL` on the timed tests | 7.26.1 | `7c05be021` | As proposed. |
| 5. split `test_c_treedb_literal_wins` | 7.26.2 | `5385260a5`, `8181b9a7a`, `a91bcdc35`, `41dcc5a36` | Seven groups instead of ~four (`early`, `late1`, `late2`, `dcd`, `dcn`, `o4`, `o4xa`; `--group=<name>`): DC and O4X split by sweep. The seven at once: ~34 s instead of 218 s. |
| 6. end the ping-pong on `outputs/lib` | 7.26.2 | `5da4f6939` | Option (ii): the root `build/` builds the tests, performance and stress only (`ENABLE_SDK`, OFF by default; ON for the ASan tree). No CLI change. |

**What the proposal did not foresee, found after it was applied:**

- **Running the `literal_wins` groups at once exhausted the per-user inotify
  instances (4096).** The test ran whole sweeps in one step of the loop, and a
  closed treedb releases its watcher's instance only when the loop completes
  the cancel: one group held up to ~1900, the suite peaked at ~3860. 7.26.2's
  parallel suite failed on hidraulia (`ctest -j12`) and artgins (`-j8`) while
  it passed on wattyzer and locally. Fixed in 7.26.3 (`afc4d27ed`): one
  iteration per step, 10 ms apart (`C_TIMER0`; the `C_TIMER` it had ran on
  the 1 s tick). The suite peaks at ~700 now, and passed in parallel on
  hidraulia (164 s) and artgins (534 s).
- **A forked child of the same test could get a transient ENOMEM from
  `io_uring_queue_init()`** with memlock unlimited, under the load of a
  parallel run. It retries now as `yev_loop_create()` does (`dca8c5f79`,
  7.26.2).
- **A suite run as `yuneta` on a production node shares the per-user kernel
  budgets (inotify instances, memlock) with the yunos running there.** The
  hidraulia run that failed had exhausted them: a production yuno opening a
  watcher at that moment would have failed too.

### Verification (2026-10-05): the work applied

Checked at `344c6aa` (SDK 7.26.3 + `check_test_databases.py`, CLI 0.21.0
`7ba50c9`): the code of every commit read (three independent read-only
reviews, each finding below re-checked by hand), and the user's flow run for
real in the cloud container (4 cores), as `yuneta`, with CLI 0.21.0 installed
from `7ba50c9`.

**Verdict: the work does what the proposal asked, and it is fast; one real
bug (18 test binaries not relinked when a kernel library changes, item 1
below) and a few gaps.**

| Step (CLI 0.21.0, 4 cores) | Time | Result |
|---|---|---|
| `yunetas init` | 16 s | ok |
| `yunetas build` (module objects already compiled) | 128 s | ok |
| `yunetas test`, first after `init` | **456 s** | 298/298, `ctest -j4` 370 s; 291 executables relinked (the root tree configured anew) |
| `yunetas test`, nothing changed | **368 s** | 298/298, `ctest -j4` 360 s; **0 relinks**, ~8 s of build |

On this box a `yunetas test` with nothing changed went from ~31 min (7.26.0,
CLI 0.20.4) to **~6 min**. The ping-pong on `outputs/lib` is gone (0
relinks), and the version gate works (`Running 'ctest -j4 ...'` on 7.26.3).

**Applied (2026-10-05): all eight findings**, in yunetas `c78516367` and
the CLI 0.21.1 (tui_yunetas `32bf3c9`, on PyPI). Checked after them:
`yunetas test` 298/298 with `ctest -j8` (log `<date>.j8.txt`), a marker in
`gobj.c` reaches the 286 test binaries that hold gobj code, and the three
checks (`check_test_ports.py`, `check_test_databases.py`, the new
`check_test_links.py`) pass. Left as they were, on purpose: the group list
of `literal_wins` is in three places, not one (`main.c` `groups[]`, the
guard of `mt_create`, the CMake loop: a gclass cannot see `main.c`, and CMake
cannot see either); `yev_loop_create()` still sleeps after its last attempt
(a kernel change, not a test's).

#### Third check (2026-10-05): `051fbba`

Checked at `57d7b38`, CLI 0.21.1, cloud container (4 cores), as `yuneta`:
the commit read, `check_test_links.py` tried on a scratch tree, `yunetas
build` (41 s) and `yunetas test` (504 s, `ctest -j4` 479 s) run.

**Verdict: the five points are applied and work; one new defect in the docs,
one flaky time window, and the record of a failure is erased by a check.**

1. **New: a code block that does not close, in `test_suite.md:86`.** The
   paragraph after the new `--serial` example starts on the closing fence's
   line (`` ``` A new test that ``). A closing fence may carry nothing but
   spaces, so it does not close: parsed with CommonMark (`markdown-it`), the
   block runs from line 83 to 103 (before `051fbba`: 90-93), and the
   paragraph on `check_test_databases.py` and its own command block render
   as code. Fix: put *"A new test that ..."* on the line after the fence.
   (No other `.md` of `051fbba` or `c785163` has a fence with text after
   it.)
2. **`yev_events/test_yevent_timer_once1` failed once** in the parallel suite:
   1.80 s in all, where every other run of this session took 1.00 s (it
   measures 1.0005 s alone, 10 runs; and 20 of 20 passed while
   `literal_wins`, `test_fs_watcher_overflow`, `test_rt_disk_overflow`,
   `test_yevent_reload_stress` and `test_c_tcp_s_stats` ran at `-j4`). Its
   window is 0.9-1.1 s (`test_yevent_timer_once1.c:244`); a stall of ~0.8 s
   on a VM a minute after boot pushed it out. Nothing of `051fbba` touches
   it. It is the class the first audit listed (time windows that CPU load
   can break); 1 failure in the ~9 parallel full runs of this session. Fix,
   to choose: `RUN_SERIAL` on the strict time-window tests
   (`test_yevent_timer_once1`, `_once2`, `_periodic1`, `_kept_after_post`,
   `test_c_timer`, `test_c_timer0`, `test_static_resolv_numeric`: ~18 s run
   alone), or windows that measure the timer against the loop's own clock
   rather than against the wall.
3. **The output of a failed test is lost by the check that the pre-tag audit
   runs after the suite.** `check_test_databases.py` runs `ctest
   --show-only=json-v1`, and ctest truncates `Testing/Temporary/LastTest.log`
   on any run (measured: 17239 lines → 3). `LastTestsFailed.log` survives, so
   `--rerun-failed` still works, but nothing keeps WHY it failed: `yunetas
   test` passes no `--output-on-failure`, so its `build/<iso>.j<N>.txt` holds
   only `[ERROR_MESSAGE]`. That is how the output of item 2 was lost here.
   Fix: `ctest --output-on-failure` in `yunetas test` (the failed test's
   output then lands in the kept log), and/or the check reading ctest with
   `--test-dir` of a copy, or saying it overwrites `LastTest.log`.
4. Nit: `check_test_links.py` reports in the order it scans (bare tokens
   first, then `target_link_libraries()`), not by line (`:2`, `:3`, `:5`,
   `:4` on the scratch tree).

Verified correct: `check_test_links.py` on a scratch tree with each form —
`set(LIBS libtimeranger2.a)`, `-lyunetas-gobj`, a bare `jansson` in
`target_link_libraries()`, a `libfoo.a` — reports the four, leaves `pthread`,
`dl`, a comment, the full paths and `${YUNETAS_KERNEL_LIBS}` alone, says
*"cannot list tests/c/locked"* for an unreadable directory, and exits 1; on
the tree: *"145 CMakeLists.txt checked: no archive named bare"*, exit 0.
`pkey_to_jwks` links `${EXT_LIB_DIR}/libssl.a`, `libcrypto.a`;
`dba_postgres` `${MODULE_POSTGRES}` (`project.cmake:307-308`; the `libpq`
gap noted in `TODO.md`); `test_tr2check` checks `access(TR2CHECK_BIN, X_OK)`
first, the `/yuneta/bin` default replaced by an `#error`, and passed (2.24 s);
the release rule reads `build/*.j1.txt` and points to `--serial`;
`test_suite.md` says how to compare times; the CHANGELOG says 14; the
pre-tag audit runs the three checks; the three checks pass.

#### Second check (2026-10-05): `c785163` and CLI 0.21.1

**Applied (2026-10-05): the five points below**, in `051fbba1a`: the release rule
reads timings from `build/*.j1.txt`; the CHANGELOG counts 14; the link check
reads every `CMakeLists.txt` of six roots, whole, fails on an unreadable
directory, and found and fixed `pkey_to_jwks` and `dba_postgres` (whose
static `libpq` link is a separate, older gap, in `TODO.md`); the pre-tag
audit runs the three checks; `test_tr2check` fails as *"tr2check not
found"*; the lines rewrapped.

Checked at `daf0f7c`, CLI 0.21.1 installed from `32bf3c9`, in the cloud
container (4 cores), as `yuneta`; the code of both commits read against each
finding, the CLI's own tests run (60 passed).

| Step (CLI 0.21.1, 4 cores) | Time | Result |
|---|---|---|
| `yunetas build` (now `make -j4`) | 54 s | ok |
| `yunetas test`, first after the pull | 510 s | 298/298, `ctest -j4` 477 s, log `<iso>.j4.txt` |
| marker in `gobj.c`, `yunetas test` | 448 s | 298/298; **the marker reaches the 286 binaries with gobj code, none left behind** (was 268 of 286) |
| marker removed, `yunetas build` | 29 s | ok |
| `yunetas test` after that change | 397 s | 298/298, `ctest -j4` 361 s (289 relinks: the reverted library, as it must) |
| `check_test_ports.py`, `check_test_databases.py`, `check_test_links.py` | — | exit 0, all three |

**Verdict: the eight findings are applied, seven completely; the bug of
item 1 is fixed and verified.** What is left:

1. **Item 6, half done.** The release rule in `CLAUDE.md:1904-1910` now says
   to compare runs of one job count, but its own example still greps
   `build/*.txt` (`CLAUDE.md:1906`), every job count mixed; and
   `test_suite.md` says nothing of `.j<N>` or of `--serial` for timing.
   Fix: `build/*.j1.txt` in the example (or `*.j<N>.txt` of one N), and one
   line in `test_suite.md`.
2. **A count of mine, carried into the CHANGELOG.** Item 1 below listed 14
   `tr_treedb_*` and called them 15 (14 + `tr_dt_unknown` + 3 benchmarks =
   18); the CHANGELOG's Unreleased entry repeats *"The 15"*. Fix: 14 there
   (corrected below).
3. **`scripts/check_test_links.py` is narrower than its job.** It matches a
   `lib*.a` only at the start of a line (`BARE`, line 22), so it misses
   `target_link_libraries(x libfoo.a)` on one line, `-lfoo`, a bare
   `timeranger2` (CMake makes it `-l` too) and a variable holding a bare
   name; `os.walk()` has no `onerror`, so an unreadable directory or a
   missing root is skipped and the script exits 0 (a clean run prints
   nothing, so nothing says what it looked at). And it covers `tests`,
   `performance`, `stress` only: the same shape stands in
   `yunos/c/dba_postgres/CMakeLists.txt:86` (`libyunetas-c_postgres.a`) and
   `utils/c/pkey_to_jwks/CMakeLists.txt:71-72` (`libssl.a`, `libcrypto.a`) —
   not new, and less likely to bite (a yuno is rebuilt by its own `make`),
   but the same staleness after a change of those archives. Nothing runs the
   three checks (neither `yunetas test` nor the release checklist).
4. **`test_tr2check` hides a missing tool.** Each of its 12 calls appends
   `2>/dev/null`, so a `tr2check` that is not there fails as *"did not answer
   json"* with exit 127, not as *"not found"*; and `test_tr2check.c:56-58`
   keeps `#define TR2CHECK_BIN "/yuneta/bin/tr2check"` as its `#ifndef`
   default — dead (CMake always defines it) and naming the path the fix
   moved away from. A plain `ctest` (not `yunetas test`) runs whatever
   `utils/c/tr2check/build/` holds.
5. Nits: `tests/c/c_treedb_literal_wins/README.md` ~186 and `CLAUDE.md:1906`
   are longer than 80 characters; `CLAUDE.md:1910` is a short line.

Verified correct: the 18 `CMakeLists.txt` link `${LIB_DEST_DIR}/lib*.a` in
the same order (timeranger2, yev_loop, ytls, gobj, then the external ones),
and no other bare form is left under `tests`/`performance`/`stress`; the CLI
gate is `(7, 26, 3)` with a test that 7.26.0 and 7.26.2 run serially;
`yunetas build --jobs/-j` (`min=1`) reaches the SDK and the projects loops;
`default_jobs()` reads `sched_getaffinity`; `sdk_version()` only when ctest
runs in parallel; the log name carries the jobs ctest really ran with and
`CTEST_LOG_NAME` keeps old and new names; `test_tr2check` falls back to
`utils/c/tr2check/build/tr2check`, built by `yunetas test` before the root
tree; `check_test_databases.py` now sees the eight tests it missed (185
directories); the `C_TIMER0` comment states the barrier; the ENOMEM retry no
longer sleeps after its last attempt; a run without `--group` says so; the
READMEs speak of the locks, `RUN_SERIAL` and the checks; the CHANGELOG
v7.26.2 sentences, `CLAUDE.md:724`; the perf `test4` lock removed; the
`utils/python/tui_yunetas` pointer at `32bf3c9`.

#### Findings, most severe first

1. **Bug: 18 test binaries are not relinked when a kernel library changes,
   and nothing says so.** They name four archives BARE in
   `target_link_libraries` (`libtimeranger2.a`, `libyev_loop.a`,
   `libytls.a`, `libyunetas-gobj.a`; e.g.
   `tests/c/tr_treedb_rowid/CMakeLists.txt:62-65`), which CMake turns into a
   `-l` search, not a file dependency: `build.make` of `test_tr_treedb_rowid`
   lists `libjwt-y.a`, `libjansson.a`, ... by full path and none of the four.
   Experiment on this tree: a marker string appended to `gobj.c`, then the
   build phase of `yunetas test` (module installs + root `make -j4
   install`): **268 binaries carry it; the 18 do not** — the 14
   `tr_treedb_*` (`_rowid`, `_relink`, `_snap`, `_snap_clone`, `_files`,
   `_immutable`, `_link_events`, `_load_failed`, `_failed_save`,
   `_update_instance`, `_schema_parse`, `_hook_hygiene`, `_hook_rename`,
   `_delete_instance`), `tr_dt_unknown`, and `perf_timeranger2`,
   `perf_tr_treedb`, `perf_rotatory`. (The 3 others without it hold no
   gobj code: `static_resolv/*`, `test_handshake_reject_mbedtls`.) So after
   a change in timeranger2 or gobj — exactly what these tests are for — they
   pass or fail on the OLD library. Until 0.21.0 the `make clean` hid it;
   then, until 7.26.2, the ping-pong did (they depend on `libjwt-y.a` by full
   path, reinstalled on every run). **This also corrects (a) below**, and
   three texts that state the opposite: CHANGELOG v7.26.2 (*"a changed
   library still relinks every test that links it"*), the message of
   `5da4f6939`, and `test_suite.md:483-500`, whose worked example is
   `test_tr_treedb_rowid` itself (*"the next build of the test relinks
   it"*). Fix: link the four by full path (`${LIB_DEST_DIR}/libtimeranger2.a`
   ..., or the sub-list of `YUNETAS_KERNEL_LIBS`) in those 18
   `CMakeLists.txt`; a check that no `CMakeLists.txt` under
   `tests`/`performance`/`stress` names a `lib*.a` bare keeps it fixed.
2. **Gap: the CLI's parallel gate lets in 7.26.2**, whose suite the 7.26.3
   CHANGELOG says failed in parallel on hidraulia and artgins
   (`CTEST_PARALLEL_SINCE = (7, 26, 1)`, tui_yunetas `main.py:899`). 7.26.1
   passed in parallel (validated in this review), 7.26.2 is the bad one.
   Fix: exclude 7.26.2, or raise the floor to `(7, 26, 3)`.
3. **Gap: `yunetas build` still runs `make` with no `-j`**
   (tui_yunetas `main.py:281` for the SDK, `:289` for the projects); only
   `test` got it. A full build is where it costs most (yesterday's figures:
   397 s serial vs 106 s at `-j4` for the root tree). Fix: the same
   `--jobs` option.
4. **Gap: `test_tr2check` now tests whatever `tr2check` is installed.** With
   `ENABLE_SDK` OFF the root tree has no `tr2check` target, so
   `tests/c/timeranger2/CMakeLists.txt:174-179` always falls back to
   `/yuneta/bin/tr2check`: a machine-wide path, so a worktree run (the A/B of
   the last tag, the wattyzer release suite) tests the last one installed,
   not its own. `yuno_skeleton` dropped the same fallback for this reason.
   Fix: fall back to `${YUNETAS_BASE}/utils/c/tr2check/build/tr2check`.
5. **Gap: `scripts/check_test_databases.py` does not see the most common
   shape of the `C_NODE`/`C_TREEDB` tests**: `build_path(path_database, ...,
   path_root, "<name>", NULL)` with `"database", "<name>"` (or a bare
   `"<name>"` argument after `path_root`). About ten tests are invisible to
   it — `c_subscription_authz`, `c_node_initial_load`, `c_node_paged_nodes`,
   `c_node_authz`, `c_agent_find_new_yunos`, `c_treedb_system_schema`,
   `treedb_schema_fidelity`, `c_controlcenter_scenarios`
   (`register_yuneta_environment(root, "<name>")`) — 146 of the 298 tests have
   a directory it sees. Their names are unique today, so there is no
   collision now; a new one would pass unseen. Fix: two more patterns,
   `"database"\s*,\s*"([^"%]+)"` and `register_yuneta_environment\([^,]+,\s*"([^"%]+)"`
   (under `tests_yuneta/store/`). Also, the example in `test_suite.md`
   (`tr_msg` vs `tr_msg2db/test_pkey2_empty`) is made up but names real tests
   (`test_pkey2_empty` uses `tr_msg2db_test`): say it is an illustration, or
   use invented names.
6. **Gap: `build/*.txt` now mixes serial and parallel timings.** The release
   checklist reads the trend of the timed tests from those logs; the name
   (`<iso>.txt`) does not say the job count. The `RUN_SERIAL` tests are run
   alone, but the rest are not comparable across `-j`. Fix: the jobs in the
   name (`<iso>.j4.txt`, and widen `CTEST_LOG_NAME`), or document `--serial`
   for comparisons.
7. **The reason given for `C_TIMER0` in `test_c_treedb_literal_wins` is not
   the real one** (`c_test_literal_wins.c:196-199`, and the message of
   `afc4d27ed`: *"10 ms apart, as set_timeout0() asks"*), and it reads like
   the deferral-by-timer CLAUDE.md forbids. The timer IS right, for another
   reason: with a posted event pending, `yev_loop_run()` takes at most ONE
   completion per cycle (`io_uring_peek_cqe`, `yev_loop.c:1829-1844`), while
   a step leaves many watcher cancels to reap; chaining steps with
   `gobj_post_event()` would release ~1 inotify instance per step and bring
   the leak back. The timer's completion queues behind the step's cancels, so
   it is a barrier ("after the ring has drained this step"), and 10 ms is a
   margin. Fix: say that in the comment.
8. **Nits.**
    - CLI: `-j 0` / `-j -3` silently become `-j1` (`typer.Option(..., min=1)`);
      the default `os.cpu_count()` ignores the container's affinity
      (`len(os.sched_getaffinity(0))`); `sdk_version()` runs (and may warn)
      with `--serial`; the CLI README still reads `yunetas test # ctest`; no
      unit test of `test()` / `sdk_version()`.
    - `test_c_treedb_literal_wins`: README:24-25 *"the longest alone (`o4xa`)
      takes ~47 s"* (41dcc5a measured 28 s); README:182-185 still says only
      the scenarios from LM on run one per step; with no `--group` the binary
      runs `all` without a word; the group list is written in four places
      (`main.c` `groups[]`, `mt_create`, the usage string, CMake); the
      ENOMEM retry sleeps 1.6 s after its last attempt (as
      `yev_loop_create()` does).
    - Docs: `tests/c/README.md` and `performance/c/README.md` say nothing of
      the locks / `RUN_SERIAL` (where a new test's author looks);
      `tests/c/README.md:112` *"37 binaries"* (42); CHANGELOG v7.26.2:44 says
      an old root `build/` stops building the SDK *"at its next `yunetas
      init`"* — it is at its next `make` (the changed `CMakeLists.txt`
      re-runs the configure); `CLAUDE.md:724` is a 129-character line;
      `perf_c_tcp/test4` and `perf_c_tcps/test4` hold the
      `perf_topic_integer` lock that only `test5` needs (harmless,
      `RUN_SERIAL` anyway).

#### Verified correct

- The ctest properties: every name in `set_tests_properties` exists, the G1
  fixture chain, the locks, `RUN_SERIAL` on the 11 benchmarks, the
  `perf_yev_ping_pong2` rename; no other pair of tests shares a directory
  without a lock (the seven `literal_wins` groups use
  `c_treedb_literal_wins_<group>` each); `check_test_databases.py` exits 0.
- The `literal_wins` split: the 41 early scenarios and the late ones each in
  exactly one group, in the same order; DC and O4X split by sweep with the
  same content; an unknown `--group` refused (`main.c:1166-1170` and
  `mt_create`); the per-step bounds match the old loops; the ENOMEM retry is
  bounded (5), says each retry, fails loud, and mirrors
  `yev_loop_create()`.
- `ENABLE_SDK`: correct; the hook (`yunetas build` before `cmake --build
  build`), `yuno_skeleton` and the ASan recipe (`-DENABLE_SDK=ON`) still
  work.
- CLI 0.21.0: `make -jN` in the module dirs and in `build/`; `--clean`,
  `--serial` (`make -j8`, `ctest -j1`), `-j abc` refused by typer; the gate
  serial on 7.26.0 with its message, parallel on 7.26.3; the ctest log name
  and the logs `init` keeps unchanged; the `MarkupError` fixed (a clean
  message and exit 1 without `YUNETAS_BASE`); its 51 tests pass; published
  as 0.21.0 on PyPI and the `utils/python/tui_yunetas` pointer of yunetas at
  `7ba50c9`.
- Releases: tags `7.26.1`, `7.26.2`, `7.26.3` on GitHub, `YUNETA_VERSION`
  7.26.3, `RELEASE` 1, a CHANGELOG section each, CLAUDE.md at 7.26.3.

### Findings so far

1. **Ports are no longer the obstacle.** Since 7.25.20 every binary has its
   own ports and `scripts/check_test_ports.py` passes (checked on
   `95dadcf`). What stops `ctest -j` now is shared FILESYSTEM state and
   ordering dependencies that ctest does not know about.
2. **No test declares what it shares.** No `CMakeLists.txt` under
   `tests/c` or `performance/c` uses `RESOURCE_LOCK`, `FIXTURES_SETUP` /
   `FIXTURES_REQUIRED`, `DEPENDS` or `RUN_SERIAL`; the only property set is
   `TIMEOUT` (`secret_attrs`, `yuno_skeleton/templates` 600 s,
   `c_ievent_srv_identity_card`). So the suite is correct only when it runs
   serially, in registration order.
3. **Example of a hidden dependency:** `timeranger2/test_topic_pkey_integer`
   and the six `test_topic_pkey_integer_iterator*` use the same store,
   `$HOME/tests_yuneta/tr_topic_pkey_integer` (`#define DATABASE` in each
   file; path built at `test_topic_pkey_integer.c:326-331`). Other shared
   names: `tr_msg` (`tr_msg/test_tr_msg1.c`, `test_tr_msg2.c`),
   `tr_delete_instance` (`timeranger2/test_delete_instance.c`,
   `tr_treedb_delete_instance`), `perf_topic_integer` (`perf_c_tcp/c_test5.c`,
   `perf_c_tcps/c_test5.c`, `perf_yev_ping_pong2`). The full audit is
   pending.
4. **Part of the half hour is probably not ctest, but `yunetas test`
   itself** (CLI 0.20.4, `yunetas/main.py`, `def test()`):

   ```python
   process_build_command(DIRECTORIES, ["make", "install"])   # every module
   process_build_command(["."], ["make", "install"])         # root build/
   process_build_command(["."], ["make", "clean"])           # <- throws it away
   ret = process_build_command(["."], ["make", "install"])   # <- full rebuild
   ...
   process_build_command(["."], ["ctest", "--output-log", filename])
   ```

   - Every run does a `make clean` + full rebuild of the root `build/` tree
     (every library, util, yuno, test and benchmark compiled again), right
     after a `make install` that had just brought it up to date.
   - Every `make` runs **without `-j`** (`init` configures with the default
     Unix Makefiles generator and the CLI passes no job count), so it uses
     one core.
   - `ctest` also runs **without `-j`**.

   The `make clean` is presumably there because of the stale-binary trap
   (*"A TEST BINARY LINKS THE INSTALLED LIBRARIES"*, CLAUDE.md): a test
   relinks only when its own sources change. That trap has a cheaper cure —
   make each test target depend on the library files it links — but even
   without it, `make -j$(nproc) clean install` already removes most of the
   cost.

### Audit: what the tests share (read from the code)

All paths are under `$HOME/tests_yuneta/` unless said otherwise.

**Why two tests on one store collide even when each wipes its own part:** the
first tranger opened as master takes an exclusive lock on
`__timeranger2__.json`; a second master on the same database falls back to
replica with *"Open as not master"* (`timeranger2.c:842`), which fails the
`test_json(NULL)` log check, and a replica refuses `create_topic` / append.

| Group | Shared path | Evidence | Order-dependent? |
|---|---|---|---|
| **G1** `timeranger2/test_topic_pkey_integer` → `_iterator`, `_iterator2`..`5` → `_iterator6`; and `perf_yev_ping_pong2` | `tr_topic_pkey_integer/` | `test_topic_pkey_integer` wipes and fills it (`:26,335,345`). The iterators never wipe; they reopen that data (`_iterator.c:21,55-57,89-97`); `_iterator2` asserts exactly 2x90000 rows (`:248`); `_iterator6` APPENDS 180000 more (`:308-320`) and asserts the start state left by `test_topic_pkey_integer` (`:298`). **`perf_yev_ping_pong2` wipes the same database** (`perf_yev_ping_pong2.c:28,486`). | **Yes.** Every iterator fails alone: on a clean `$HOME` (an INFO *"Creating __timeranger2__.json"*, then *"directory not found"*), and after a full run too (`perf_yev_ping_pong2`, registered later, removed the topic; or `_iterator6` doubled the rows). Works today only through the `SRCS` order under serial ctest. |
| **G2** `tr_msg/test_tr_msg1`, `test_tr_msg2` | `tr_msg/` | both wipe it (`test_tr_msg1.c:43,901`; `test_tr_msg2.c:24,459`) | No — they only collide at once |
| **G3** `timeranger2/test_delete_instance`, `tr_treedb_delete_instance` | `tr_delete_instance/` | `test_delete_instance.c:43` (wipes at 176, 313, ...); `test_tr_treedb_delete_instance.c:46,2753` | No |
| **G4** `perf_c_tcp/test5`, `perf_c_tcps/test5` | `perf_topic_integer/` | both open it as master (`c_test5.c:16,232,249` in each) | No |

Not shared (checked): every other `DATABASE` / env root under
`tests_yuneta` belongs to one binary; every fixed `/tmp` name belongs to one
binary; `c_mqtt` uses `$TMPDIR/<name>.<pid>.<n>`, `c_tcps/test6` `mkdtemp`;
the MQTT stores `/tmp/store/<role>/` are per role (never wiped, so they carry
over between runs); `/yuneta/agent/certs/localhost.*` and
`/yuneta/bin/tr2check` are only read; no fixed UNIX socket or pid file.

**Other things that matter under `-j`:**

- **inotify limits** (per user): `test_fs_watcher_overflow` watches ~20k
  directories and floods the queue, `test_rt_disk_overflow` floods it too,
  and the `rt_disk_*` tests, `_iterator6`, `perf_yev_ping_pong2` and
  `perf_*/test5` follow the disk. Together they risk `ENOSPC` →
  one `RESOURCE_LOCK`.
- **Timing assertions that CPU load can break:**
  `yev_events/test_yevent_timer_once1.c:244` (0.9-1.1 s),
  `test_yevent_timer_periodic1.c:210` (2.9-3.1 s),
  `test_yevent_timer_once2.c:291` (2.4-2.6 s),
  `test_yevent_kept_after_post.c:231` (<= 1 s),
  `c_timer/src/c_test_timer.c:290` and `main.c:162`,
  `c_timer0/src/c_test_timer0.c:345` and `main.c:176` (5000-5500 ms),
  `test_fs_watcher_overflow.c:1914,1936` (the 6x ratio; the loop silent
  <= 1000 ms), `static_resolv/test_static_resolv_numeric.c:79,103` (< 200 ms),
  `c_controlcenter_scenarios/src/c_test_cc.c:1197` (one 1500 ms tick). Lower
  risk: `gobj_post_event` (10 ticks of 1 ms in 200 ms), the 2-3 s connect
  guards of `c_tcp`, `c_tcps`, `c_tcp_s_stats`, `c_auth_bff/test10`.
- **The benchmarks** saturate cores, so they skew each other's figures and
  the timing tests above.
- **Two checkouts on one machine** share `~/tests_yuneta` and the `/tmp`
  names, so two suites at once (e.g. the A/B worktree of the last tag from
  the release checklist) break G1-G4 even without `-j`.

**Long tests (from the code; to be confirmed by the measurement):**
`c_tcp_inactivity/test1`, `test2` (>= 20 s each, `TIMEOUT_INACTIVITY
20000`), `test3`, `test4` (~5 s); `yuno_skeleton/templates` (three cmake+make
builds plus two 3 s runs); `perf_yev_ping_pong`, `perf_yev_ping_pong2`,
`perf_auth_bff` (10 s each by `alarm`/`run_seconds`); `perf_c_tcp[s]/test4,5`
(~5 s); `test_fs_watcher_overflow`, `test_rt_disk_overflow`,
`test_delete_key_propagation` (floods, waits up to 30 s - 5 min);
`test_topic_pkey_integer` + iterators (180000 appends/reads each);
`c_llhttp_parser` (~7 s of `sleep`), `c_timer`, `c_timer0` (5 s),
`c_controlcenter_scenarios`. Most of these are WAITING, not computing — which
is exactly the time `ctest -j` gives back.

### Measured: the suite, serial vs `ctest -j4`

Cloud container, 4 cores, 15 GB, `CONFIG_FULLY_STATIC`, OpenSSL, no memory
tracking (the hook's default `.config`); run as `yuneta` under
`ulimit -Sn 1024`, HEAD `95dadcf`, 292 tests.

| Run | Wall | Result |
|---|---|---|
| `ctest` (what `yunetas test` runs) | **1273 s** (21 min) | 292/292 |
| `ctest -j4`, nothing changed | **348 s** (5.8 min), x3.7 | 287/292: `timeranger2/test_topic_pkey_integer_iterator` .. `_iterator5` fail (group G1) |

The five failures are exactly G1, and for the reason the audit gave:
`_iterator6` (started earlier by the parallel scheduler) had appended its
180000 rows, so `_iterator` found `to_t` 946857599 where it expects
946774799 (*"compare: value mismatch at
'topics`topic_pkey_integer`cache`...`to_t'"*). G2-G4 did not collide in this
run (luck of the schedule, not safety), and no timing-sensitive test failed
at `-j4` on 4 cores.

**Where the serial time goes** (per test, serial run):

| Test | s | Note |
|---|---|---|
| `test_c_treedb_literal_wins` | **220** | 17% of the suite, ONE binary: ~60 scenarios run one after another, with the crash sweeps (CR, DC: 99 sequences, DTK, O4X, FP) forking a child per kill point. At `-j4` it took 238 s — it is the critical path: **the suite can never go below it** while it is one test |
| `yev_events_tls/test_yevent_reload_stress` | 53 | |
| `timeranger2/test_fs_watcher_overflow` | 40 | 71 s at `-j4` (inotify / CPU contention), still within its limits |
| `timeranger2/test_rt_disk_overflow` | 34 | |
| `test_c_tcp_s_stats` | 27 | |
| `c_tcp_inactivity/test2`, `test1` | 24, 22 | waiting on `timeout_inactivity` |
| `emailsender/*` (47 tests) | 245 in total | mostly waiting on SMTP timeouts / pacing |

73 tests take >= 5 s and add up to 1019 s (80% of the suite); 114 take
< 1 s. Most of the long ones WAIT (timeouts, pacing, `alarm`) rather than
compute, which is why 4 jobs on 4 cores give x3.7.

### Measured: the build phase of `yunetas test`

The same four steps as the CLI (`make` with no `-j`, as it runs them), on a
tree where nothing changed since the last `yunetas test`:

| Step | `make` (today) | `make -j4` |
|---|---|---|
| 1. `make install` in every module build dir | 36 s | 36 s |
| 2. `make install` in the root `build/` | 155 s | 50 s |
| 3. `make clean` in `build/` | 12 s | 12 s |
| 4. `make install` in `build/` (everything compiled again) | **397 s** | 106 s |
| **Build phase** | **600 s** | 204 s |

So today a `yunetas test` with no change in the sources costs, on this
machine, **~600 s of build + 1273 s of ctest = ~31 min** — the "more than
half an hour" of the question. **A third of it is compiling, and almost all
of that compiling is useless:**

**(a) The `make clean` is not needed.** (Corrected on 2026-10-05: not true
for 18 binaries, which name the kernel archives bare and were relinked in
this experiment only by the ping-pong of (b); see item 1 of
*Verification*.) Experiment: a global with a marker
string appended to `kernel/c/gobj-c/src/gobj.c`, then ONLY steps 1 and 2
(`-j4`, no clean). **287 of the 290 ELF test binaries carry the marker**;
the other three do not contain `gobj.o` at all (`static_resolv/*` link only
libc and `#include` `static_resolv.c`; `test_handshake_reject_mbedtls` was
relinked but pulls no symbol of `gobj.c`), so for them the marker is not
expected. The reason it works: the tests link the installed archives BY FULL
PATH (`tools/cmake/project.cmake:229-241`), so the Makefiles generator writes
each `.a` as a dependency of the link (`build.make`:
`tests/c/c_timer/test_c_timer: .../outputs/lib/libyunetas-gobj.a`), and a
changed installed library relinks every test that uses it. The stale-binary
trap of CLAUDE.md (*"A TEST BINARY LINKS THE INSTALLED LIBRARIES"*) is real
for `cmake --build build --target <test>` alone, but step 1 already installs
the fresh libraries before step 2 links the tests. (gobj.c restored, tree
rebuilt, marker gone.)

**(b) Steps 1 and 2 relink everything on EVERY run, even with nothing
changed: the two trees ping-pong on `outputs/lib`.** The module build dirs
(step 1) and the root `build/` (step 2, which `add_subdirectory()`s the same
kernel libraries) each build their OWN copy of `libyunetas-gobj.a`,
`libtimeranger2.a`, ... and both install it into the same `outputs/lib`.
CMake's install compares the files, so each tree's install finds the other
tree's copy and replaces it (*"Installing:"*, not *"Up-to-date:"*), and the
new mtime relinks every executable of the other tree. Measured: after step 1
installs `kernel/c/gobj-c`'s copy, the root `make install` relinks 135+
executables; a second root `make install` straight after relinks nothing.
That is the 36 + 155 s of steps 1-2 with no change.

### Proposal (as written for approval; applied, see *Result*)

Ordered by gain / risk. Items 1-2 live in the CLI repo
(`artgins/tui_yunetas.py`, `yunetas/main.py`, `def test()`), which is NOT
attached to this cloud session; items 3-6 are in yunetas.

1. **`yunetas test`: drop the `make clean` (keep it behind `--clean`), and
   build with `-j$(nproc)`.** Proven unneeded by (a). Saves ~400 s serial;
   with `-j` the whole build phase goes 600 → ~90 s on this box. Zero risk
   to the tests' correctness; the ctest log name/format stays as it is.
2. **`yunetas test`: run `ctest -j$(nproc)`** (with a `--serial` / `-j 1`
   option), **only after items 3-4 have landed**. Measured x3.7 on 4 cores
   (1273 → 348 s). Do NOT go above the core count: a dozen tests assert
   time windows (see the audit) and passed at `-j4` on 4 cores only because
   nothing was oversubscribed.
3. **Declare what the tests share, in their `CMakeLists.txt`** (ctest
   properties only, no test code changed):
   - G1: `test_topic_pkey_integer` `FIXTURES_SETUP tr_pkey_integer`;
     `_iterator` .. `_iterator6` `FIXTURES_REQUIRED tr_pkey_integer` +
     `RESOURCE_LOCK tr_topic_pkey_integer`; `_iterator6` (it appends)
     `DEPENDS` on `_iterator` .. `_iterator5`. Side benefit: `ctest -R
     iterator3` alone then runs its setup first and passes, which today it
     cannot.
   - `perf_yev_ping_pong2`: rename its `DATABASE`
     (`perf_yev_ping_pong2.c:28`) — it only borrows the name and wipes the
     G1 store (a one-line change in a benchmark; its figures do not change).
   - G2 `RESOURCE_LOCK tr_msg`, G3 `RESOURCE_LOCK tr_delete_instance`,
     G4 `RESOURCE_LOCK perf_topic_integer`.
   - `RESOURCE_LOCK inotify` on `test_fs_watcher_overflow` and
     `test_rt_disk_overflow` (the two that flood the per-user queue).
   - `scripts/check_test_ports.py` gets a sibling check (or a mode) that
     fails when two binaries name the same `DATABASE` under `tests_yuneta`
     without a shared `RESOURCE_LOCK`, so the next collision is caught like a
     port.
4. **Keep the timing trend readable.** The release checklist reads test
   times from `build/*.txt`; under `-j` those times include contention. Mark
   `RUN_SERIAL` the tests whose time is tracked: the 11 `perf_*` (81 s in
   total) and the G1 chain (7 s). Cost: ~90 s run alone; they also stop
   skewing the time-window tests. (Alternative: release runs use
   `--serial`.)
5. **Split `test_c_treedb_literal_wins`** (220 s, one binary). At `-j4` it
   is not yet the limit (238 s < 348 s), but at `-j8` and above it is the
   floor of the whole suite. Register it several times with an argument
   that picks a group of scenarios (e.g. the early ones / CR / DC / DTK+O4x+FP),
   each group under its own database subdirectory. A change to the test's
   code, so a separate step.
6. **End the ping-pong (b), so a `yunetas test` with nothing changed
   compiles nothing.** Two ways, to be decided: (i) `yunetas test` skips
   step 1 (the root tree builds every module) and the tests link the
   library TARGETS when they exist in the same tree (`if(TARGET
   yunetas-gobj)` in `project.cmake`), so a test depends on the library it
   links and not on the installed copy; or (ii) the root tree stops
   installing the libraries the module dirs already install. Touches the
   build of every consumer: needs its own review.

**Expected result on this 4-core box:** with 1-4, `yunetas test` with nothing
changed goes from ~31 min to **~90 s build + ~6 min ctest ≈ 7-8 min**; with 6
the build phase is a few seconds; with 5, machines with 8+ cores scale
further (the remaining floor is `test_yevent_reload_stress`, 53 s).

Not proposed: shortening the waits of the slow tests (`c_tcp_inactivity`'s
20 s window, the SMTP timeouts of `emailsender`). They are what the tests
check, and under `-j` waiting costs almost nothing.

### Patches for items 1-4 (applied in 7.26.1 and CLI 0.21.0)

Both patches were tried and then removed: nothing of them is committed in
any repository. Apply with `git apply` from the root of each repo.

#### yunetas: what the tests share (items 3 and 4)

ctest properties only, plus the database name of `perf_yev_ping_pong2` (and
its line in `performance/c/README.md`). What each part does:

- `timeranger2/test_topic_pkey_integer` is the `FIXTURES_SETUP` of
  `tr_topic_pkey_integer`; the six iterators `FIXTURES_REQUIRED` it, all
  seven hold `RESOURCE_LOCK tr_topic_pkey_integer` (each opens the
  database as master), and `_iterator6`, which appends, `DEPENDS` on the
  other five. ctest adds the setup itself when a single iterator is asked
  for.
- `RESOURCE_LOCK tr_msg` (both `tr_msg` binaries), `tr_delete_instance`
  (`timeranger2/test_delete_instance`, `test_tr_treedb_delete_instance`),
  `perf_topic_integer` (`perf_c_tcp/*`, `perf_c_tcps/*`), `inotify_flood`
  (`test_fs_watcher_overflow`, `test_rt_disk_overflow`).
- `RUN_SERIAL` on the 11 `perf_*` and the seven tests of the
  `tr_topic_pkey_integer` chain (the times the release checklist reads).
- `perf_yev_ping_pong2` writes `~/tests_yuneta/perf_yev_ping_pong2`
  instead of wiping the chain's database.

Validation (4 cores, `ulimit -Sn 1024`, inotify limits of
`99-yuneta-core.conf`):

| Check | Before | With the patch |
|---|---|---|
| `ctest -R 'iterator3$'` on a clean `~/tests_yuneta` | fails (no data) | passes: ctest runs `test_topic_pkey_integer` first, then `iterator3` |
| `ctest -R 'iterator6$'` | fails | passes, the same way |
| `ctest --show-only=json-v1` | no property | every property above, as written |
| `ctest -j4`, full suite, run 1 | 287/292, 348 s | **292/292, 444 s** |
| `ctest -j4`, full suite, run 2 | — | **292/292, 435 s** |

The ~90 s more than the unpatched `-j4` is the price of `RUN_SERIAL` (the
benchmarks add up to 81 s run one after another), as estimated. The log
confirms `test_topic_pkey_integer` ran alone (nothing started between its
start and its end). Its times across the runs of this session (2.70 s in the
serial baseline, 2.88 / 3.08 / 3.17 s later) moved with the VM, which was
restarted between the baseline and the patched runs: the trend of such a
figure is to be read on one machine, as the release checklist already says.

**Found on the way: `ctest -j` needs the inotify limits of
`99-yuneta-core.conf`.** The container restarted during the session and came
back with the Linux defaults (`max_user_instances` 128,
`max_queued_events` 16384). The first patched `-j4` run then failed 20
tests (*"inotify_init1() FAILED: The user limit on the total number of
inotify instances has been reached"* in the treedb, timeranger2 and c_mqtt
tests, then the time windows of `emailsender`, `c_timer`,
`test_c_controlcenter_scenarios` on the loaded machine). The serial baseline
had run with the raised limits; with several treedb tests at once, 128
instances per user is not enough. A machine that runs
`yunetas test` in parallel must carry those limits (nodes do, from the
package; the cloud hook sets them only at session start).

```diff
diff --git a/performance/c/README.md b/performance/c/README.md
index 0eb339c..7be776a 100644
--- a/performance/c/README.md
+++ b/performance/c/README.md
@@ -64,7 +64,7 @@ Source: `src/perf_yev_ping_pong.c` (single file, no GClasses)
 
 Same as `perf_yev_ping_pong` with the addition of `tranger2_append_record()` on every received client message, measuring the persistence overhead on the raw event loop.
 
-- **Timeranger2 DB:** `~/tests_yuneta/tr_topic_pkey_integer/topic_pkey_integer_ping_pong`
+- **Timeranger2 DB:** `~/tests_yuneta/perf_yev_ping_pong2/topic_pkey_integer_ping_pong`
 - **Client message:** JSON `{"hello": "AAA..."}` (serialized via `json2gbuf`)
 
 Source: `src/perf_yev_ping_pong2.c` (single file, no GClasses)
diff --git a/performance/c/perf_auth_bff/CMakeLists.txt b/performance/c/perf_auth_bff/CMakeLists.txt
index 431fd22..0bfe09f 100644
--- a/performance/c/perf_auth_bff/CMakeLists.txt
+++ b/performance/c/perf_auth_bff/CMakeLists.txt
@@ -86,6 +86,9 @@ install(
 )
 
 add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
+# A benchmark runs alone under ctest -j: its figures and the time windows
+# of the tests would skew each other
+set_tests_properties(${PROJECT_NAME} PROPERTIES RUN_SERIAL TRUE)
 
 # compile in Release mode :
 #
diff --git a/performance/c/perf_c_tcp/CMakeLists.txt b/performance/c/perf_c_tcp/CMakeLists.txt
index bc7aa4a..abc6f0c 100644
--- a/performance/c/perf_c_tcp/CMakeLists.txt
+++ b/performance/c/perf_c_tcp/CMakeLists.txt
@@ -81,6 +81,12 @@ foreach(test ${SRCS})
         ${DEBUG_LIBS}
     )
     add_test("${current_directory_name}/${test}" ${binary})
+    # Alone under ctest -j (figures); test5 of perf_c_tcp and perf_c_tcps
+    # both open the database perf_topic_integer as master
+    set_tests_properties("${current_directory_name}/${test}" PROPERTIES
+        RUN_SERIAL TRUE
+        RESOURCE_LOCK perf_topic_integer
+    )
 
     install(
         TARGETS ${binary}
diff --git a/performance/c/perf_c_tcps/CMakeLists.txt b/performance/c/perf_c_tcps/CMakeLists.txt
index 0bfbc45..1e5004b 100644
--- a/performance/c/perf_c_tcps/CMakeLists.txt
+++ b/performance/c/perf_c_tcps/CMakeLists.txt
@@ -80,6 +80,12 @@ foreach(test ${SRCS})
         ${DEBUG_LIBS}
     )
     add_test("${current_directory_name}/${test}" ${binary})
+    # Alone under ctest -j (figures); test5 of perf_c_tcp and perf_c_tcps
+    # both open the database perf_topic_integer as master
+    set_tests_properties("${current_directory_name}/${test}" PROPERTIES
+        RUN_SERIAL TRUE
+        RESOURCE_LOCK perf_topic_integer
+    )
 
     install(
         TARGETS ${binary}
diff --git a/performance/c/perf_c_treedb/CMakeLists.txt b/performance/c/perf_c_treedb/CMakeLists.txt
index daac8eb..c37c3a6 100644
--- a/performance/c/perf_c_treedb/CMakeLists.txt
+++ b/performance/c/perf_c_treedb/CMakeLists.txt
@@ -96,3 +96,6 @@ install(
 # The figures of a release come from a run with the default sizes.
 add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME}
     --config-file=${CMAKE_CURRENT_SOURCE_DIR}/small.json)
+# A benchmark runs alone under ctest -j: its figures and the time windows
+# of the tests would skew each other
+set_tests_properties(${PROJECT_NAME} PROPERTIES RUN_SERIAL TRUE)
diff --git a/performance/c/perf_rotatory/CMakeLists.txt b/performance/c/perf_rotatory/CMakeLists.txt
index 3accde8..e564f6c 100644
--- a/performance/c/perf_rotatory/CMakeLists.txt
+++ b/performance/c/perf_rotatory/CMakeLists.txt
@@ -94,3 +94,6 @@ install(
 # ctest runs it small: it checks that the benchmark builds and runs.
 # The figures of a release come from a run with the default sizes.
 add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME} --small)
+# A benchmark runs alone under ctest -j: its figures and the time windows
+# of the tests would skew each other
+set_tests_properties(${PROJECT_NAME} PROPERTIES RUN_SERIAL TRUE)
diff --git a/performance/c/perf_timeranger2/CMakeLists.txt b/performance/c/perf_timeranger2/CMakeLists.txt
index e095d47..f8a04a8 100644
--- a/performance/c/perf_timeranger2/CMakeLists.txt
+++ b/performance/c/perf_timeranger2/CMakeLists.txt
@@ -97,3 +97,6 @@ install(
 # ctest runs it small: it checks that the benchmark builds and runs.
 # The figures of a release come from a run with the default sizes.
 add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME} --small)
+# A benchmark runs alone under ctest -j: its figures and the time windows
+# of the tests would skew each other
+set_tests_properties(${PROJECT_NAME} PROPERTIES RUN_SERIAL TRUE)
diff --git a/performance/c/perf_tr_treedb/CMakeLists.txt b/performance/c/perf_tr_treedb/CMakeLists.txt
index 5889fb5..4d74d8c 100644
--- a/performance/c/perf_tr_treedb/CMakeLists.txt
+++ b/performance/c/perf_tr_treedb/CMakeLists.txt
@@ -97,3 +97,6 @@ install(
 # ctest runs it small: it checks that the benchmark builds and runs.
 # The figures of a release come from a run with the default sizes.
 add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME} --small)
+# A benchmark runs alone under ctest -j: its figures and the time windows
+# of the tests would skew each other
+set_tests_properties(${PROJECT_NAME} PROPERTIES RUN_SERIAL TRUE)
diff --git a/performance/c/perf_yev_ping_pong/CMakeLists.txt b/performance/c/perf_yev_ping_pong/CMakeLists.txt
index f283564..258c209 100644
--- a/performance/c/perf_yev_ping_pong/CMakeLists.txt
+++ b/performance/c/perf_yev_ping_pong/CMakeLists.txt
@@ -97,6 +97,9 @@ install(
 )
 
 add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
+# A benchmark runs alone under ctest -j: its figures and the time windows
+# of the tests would skew each other
+set_tests_properties(${PROJECT_NAME} PROPERTIES RUN_SERIAL TRUE)
 
 # compile in Release mode :
 #
diff --git a/performance/c/perf_yev_ping_pong2/CMakeLists.txt b/performance/c/perf_yev_ping_pong2/CMakeLists.txt
index 24cec4e..35a7f25 100644
--- a/performance/c/perf_yev_ping_pong2/CMakeLists.txt
+++ b/performance/c/perf_yev_ping_pong2/CMakeLists.txt
@@ -90,6 +90,9 @@ install(
 )
 
 add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
+# A benchmark runs alone under ctest -j: its figures and the time windows
+# of the tests would skew each other
+set_tests_properties(${PROJECT_NAME} PROPERTIES RUN_SERIAL TRUE)
 
 # compile in Release mode :
 #
diff --git a/performance/c/perf_yev_ping_pong2/src/perf_yev_ping_pong2.c b/performance/c/perf_yev_ping_pong2/src/perf_yev_ping_pong2.c
index c8052b2..7bda5f4 100644
--- a/performance/c/perf_yev_ping_pong2/src/perf_yev_ping_pong2.c
+++ b/performance/c/perf_yev_ping_pong2/src/perf_yev_ping_pong2.c
@@ -25,7 +25,7 @@
 /***************************************************************
  *              Constants
  ***************************************************************/
-#define DATABASE    "tr_topic_pkey_integer"
+#define DATABASE    "perf_yev_ping_pong2"
 #define TOPIC_NAME  "topic_pkey_integer_ping_pong"
 
 BOOL dump = FALSE;
diff --git a/tests/c/timeranger2/CMakeLists.txt b/tests/c/timeranger2/CMakeLists.txt
index 604966f..c02f8b3 100644
--- a/tests/c/timeranger2/CMakeLists.txt
+++ b/tests/c/timeranger2/CMakeLists.txt
@@ -182,3 +182,50 @@ foreach(test ${SRCS})
     add_test("${current_directory_name}/${test}" ${binary})
 
 endforeach()
+
+##############################################
+#   What the tests share, for ctest -j
+##############################################
+#   test_topic_pkey_integer fills tr_topic_pkey_integer, the iterators read
+#   it (each opens it as master: one at a time) and _iterator6 appends to
+#   it, so it goes last. RUN_SERIAL: their times are the trend the release
+#   checklist reads, which other tests running at once would skew.
+set_tests_properties("${current_directory_name}/test_topic_pkey_integer" PROPERTIES
+    FIXTURES_SETUP tr_topic_pkey_integer
+    RESOURCE_LOCK tr_topic_pkey_integer
+    RUN_SERIAL TRUE
+)
+set(PKEY_INTEGER_READERS
+    test_topic_pkey_integer_iterator
+    test_topic_pkey_integer_iterator2
+    test_topic_pkey_integer_iterator3
+    test_topic_pkey_integer_iterator4
+    test_topic_pkey_integer_iterator5
+)
+set(PKEY_INTEGER_READER_TESTS "")
+foreach(test ${PKEY_INTEGER_READERS})
+    set_tests_properties("${current_directory_name}/${test}" PROPERTIES
+        FIXTURES_REQUIRED tr_topic_pkey_integer
+        RESOURCE_LOCK tr_topic_pkey_integer
+        RUN_SERIAL TRUE
+    )
+    list(APPEND PKEY_INTEGER_READER_TESTS "${current_directory_name}/${test}")
+endforeach()
+set_tests_properties("${current_directory_name}/test_topic_pkey_integer_iterator6" PROPERTIES
+    FIXTURES_REQUIRED tr_topic_pkey_integer
+    RESOURCE_LOCK tr_topic_pkey_integer
+    DEPENDS "${PKEY_INTEGER_READER_TESTS}"
+    RUN_SERIAL TRUE
+)
+
+#   tr_delete_instance is tr_treedb_delete_instance's database too
+set_tests_properties("${current_directory_name}/test_delete_instance" PROPERTIES
+    RESOURCE_LOCK tr_delete_instance
+)
+
+#   Both flood the per-user inotify queue
+set_tests_properties(
+    "${current_directory_name}/test_fs_watcher_overflow"
+    "${current_directory_name}/test_rt_disk_overflow"
+    PROPERTIES RESOURCE_LOCK inotify_flood
+)
diff --git a/tests/c/tr_msg/CMakeLists.txt b/tests/c/tr_msg/CMakeLists.txt
index 901994f..8381f25 100644
--- a/tests/c/tr_msg/CMakeLists.txt
+++ b/tests/c/tr_msg/CMakeLists.txt
@@ -81,5 +81,9 @@ foreach(test ${SRCS})
     target_link_options(${binary} PUBLIC LINKER:-Map=${PROJECT_NAME}.map)
 
     add_test("${current_directory_name}/${test}" ${binary})
+    # Both binaries wipe and use the database tr_msg
+    set_tests_properties("${current_directory_name}/${test}" PROPERTIES
+        RESOURCE_LOCK tr_msg
+    )
 
 endforeach()
diff --git a/tests/c/tr_treedb_delete_instance/CMakeLists.txt b/tests/c/tr_treedb_delete_instance/CMakeLists.txt
index e44957b..0e64618 100644
--- a/tests/c/tr_treedb_delete_instance/CMakeLists.txt
+++ b/tests/c/tr_treedb_delete_instance/CMakeLists.txt
@@ -79,3 +79,5 @@ target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--wrap=write")
 #   Test
 ##############################################
 add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
+# tr_delete_instance is timeranger2/test_delete_instance's database too
+set_tests_properties(${PROJECT_NAME} PROPERTIES RESOURCE_LOCK tr_delete_instance)
```

#### tui_yunetas.py: `yunetas test` (items 1 and 2)

- `make` with `-j<jobs>` (default: the number of cores) in the module dirs
  and in `build/`.
- `make clean` only with `--clean` (proved unneeded, see (a) above); the
  pointless `make install` of `build/` before the clean is dropped.
- `ctest -j<jobs>`, or `-j1` with `--serial`; and `-j1` when the SDK is
  older than `CTEST_PARALLEL_SINCE` (the first SDK that carries the yunetas
  patch, written as 7.26.1 here: set it to the real release), with a
  message saying why. The CLI and the SDK ship apart, so a new CLI on an
  older tree must not run its suite in parallel.
- `sdk_version()` reads `YUNETA_VERSION=<x.y.z>` from `YUNETA_VERSION` and
  says when it cannot.

Validated: `yunetas test --help`; a harness with `process_build_command`
replaced (the commands each combination builds:
`make -j4 install` x2 and `ctest -j1` on SDK 7.26.0 with the message;
`ctest -j4` when the SDK reaches the threshold; `make clean` and `ctest -j1`
with `--clean --serial`; `sdk_version()` = `(7, 26, 0)` on this tree); the
repo's own tests, 51 passed, as on `main`.

Side finding in the CLI (not in the patch): when `YUNETAS_BASE` cannot be
resolved, the error at `yunetas/main.py:64-66` is itself an error — `rich`
reads `[/yunetas]` in *"ensure /yuneta/development[/yunetas] exists"* as a
closing tag and raises `MarkupError`, so the user gets a traceback instead
of the message (and `pytest tests` fails at collection on a machine without
`/yuneta/development`). Escape it (`\[/yunetas]`) or print it with
`markup=False`.

```diff
diff --git a/yunetas/main.py b/yunetas/main.py
index 5d9cacd..a2034ff 100644
--- a/yunetas/main.py
+++ b/yunetas/main.py
@@ -893,18 +893,63 @@ def _create_new_yuno_rows(ycommand, url, dry_run, to_create, registered):
             print(f"[green]{upgrade_rows_summary(len(to_create), len(registered))}.[/green]")
 
 
+# The first SDK whose tests declare what they share (RESOURCE_LOCK,
+# FIXTURES_*, RUN_SERIAL), so that ctest can run them at once. An older tree
+# runs them one after another: in parallel its timeranger2 iterators fail.
+CTEST_PARALLEL_SINCE = (7, 26, 1)
+
+
+def sdk_version():
+    """The (major, minor, patch) of YUNETA_VERSION, or None (said)."""
+    path = os.path.join(YUNETAS_BASE, "YUNETA_VERSION")
+    try:
+        with open(path) as f:
+            text = f.read()
+    except OSError as e:
+        print(f"[yellow]Cannot read {path}: {e.strerror}[/yellow]")
+        return None
+    m = re.search(r"^YUNETA_VERSION\s*=\s*(\d+)\.(\d+)\.(\d+)", text, re.M)
+    if not m:
+        print(f"[yellow]No YUNETA_VERSION=<x.y.z> line in {path}[/yellow]")
+        return None
+    return tuple(int(x) for x in m.groups())
+
+
 @app.command()
-def test():
+def test(
+    jobs: int = typer.Option(
+        os.cpu_count() or 1, "--jobs", "-j",
+        help="Jobs of make and ctest (default: the number of cores)."
+    ),
+    serial: bool = typer.Option(
+        False, "--serial", help="Run ctest one test after another."
+    ),
+    clean: bool = typer.Option(
+        False, "--clean",
+        help="make clean before building. Not needed: a test is relinked when "
+             "an installed library it links changes."
+    ),
+):
     """
-    Run ctest in yunetas
+    Build the SDK and run ctest in yunetas
     """
-    process_build_command(DIRECTORIES, ["make", "install"])
-    process_build_command(["."], ["make", "install"])
-    process_build_command(["."], ["make", "clean"])
-    ret = process_build_command(["."], ["make", "install"])
+    make_jobs = f"-j{max(jobs, 1)}"
+    process_build_command(DIRECTORIES, ["make", make_jobs, "install"])
+    if clean:
+        process_build_command(["."], ["make", "clean"])
+    ret = process_build_command(["."], ["make", make_jobs, "install"])
     if ret == 0:
+        ctest_jobs = 1 if serial else max(jobs, 1)
+        version = sdk_version()
+        if ctest_jobs > 1 and (version is None or version < CTEST_PARALLEL_SINCE):
+            since = ".".join(str(x) for x in CTEST_PARALLEL_SINCE)
+            print(f"[yellow]ctest runs serially: the tests of this SDK declare "
+                  f"what they share only since {since}.[/yellow]")
+            ctest_jobs = 1
         filename = datetime.now().isoformat().replace(":", "-") + ".txt"
-        process_build_command(["."], ["ctest", "--output-log", filename])
+        process_build_command(
+            ["."], ["ctest", f"-j{ctest_jobs}", "--output-log", filename]
+        )
 
 
 def version_callback(value: bool):
```
