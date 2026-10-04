# Cloud review of main

Reviewed up to `c76eddb85` (2026-10-03): the answer to the review of
`71ca430e6`, and the readiness of **7.26.0**. Read-only review (no build, no
suite).

## Verdict: nothing pending in the code

Every item of the previous report is resolved, and the round brings no new
defect:

- **`give_back_delete_seq()` removed**: a delete whose `rmrdir()` fails keeps
  its number taken (a gap the followers do not mind), so no number a follower
  may have read at an overflow is ever reused. The sweep's "last" is now the
  last number taken; the one case it costs (a leftover of the delete before a
  failed one, after a master death in a mirror) is removed unheard and
  logged — documented in the code. `delete_seq_record` expects the number
  kept and never reused, and skips as root.
- **`cmd_create_node`**: `json_incref()` paired with `json_decref()`;
  `hand_files_to_record()` copies with `kw_twin()` (a binary field of the
  record increfed), and when the kw's own gbuffer replaces one carried in the
  record, the twin's reference to that one is dropped (`GBUFFER_DECREF` is
  NULL-safe); a non-object record is refused before the twin, logged.
- `.stale_signal` names from a process-wide counter; a delete refused when
  the record cannot be written is in `deploying-yunos.md` with its log line;
  `will_acl` fails on a packet longer than its parse buffer.

Accepted, as answered: a miss in `key_of_delete_ref()` walks the cache (a
`strlen()` per key, no hashing). Known gaps, not defects: no ctest for the
agent's spare window, the C_UDP client, the websocket default max, a frame
of exactly `max-1`.

## Release 7.26.0: what is left (release work, not code)

1. `### Performance, against 7.25.22` and the report (A/B, `.html` + `.json`,
   rows in `reports/README.md`, `README.md`, `performance.md`): the tm query
   on a topic marked in 7.25.22 going 7.4 → ~14.5 ms, and the fsyncs per key
   delete on topics with rt_disk feeds.
2. `YUNETA_VERSION` 7.26.0, `RELEASE` **1** (it is 3), CLAUDE.md's
   "7.25.22".
3. gobj-ui 7.26.0 on npm (the pointer names `7e42bb7`, unpublished), the
   `^7.26.0` ranges in gui_agent / gui_treedb,
   `verify_js_api_coverage.py --repin` + `--write`.
4. The two-machine suite on the final HEAD (local under `ulimit -Sn 1024`,
   and wattyzer).
5. At tag time: `check_doc_line_refs.py --repin=7.26.0`, myst cache cleared,
   `deploy.sh`, live site checked; the GitHub release body from the
   CHANGELOG without the 4-space indent.

## Answer (2026-10-04): 7.26.0 released and deployed

| Item | Done |
|------|------|
| 1. Performance | A/B against 7.25.22 (8 alternated rounds, 24 for the ctest binaries), `performance/reports/7.26.0.{html,json}`, rows in `reports/README.md`, `README.md`, `performance.md`, the CHANGELOG section. The tm query on a topic 7.25.22 had marked goes 7.5 -> **12.8 ms** (measured; the ~14.5 of the texts was corrected). The A/B found one more price and it was fixed before the tag: a master that opened a topic wrote `delete_seq.json` durably, doubling the creation of topics and making the first open of 40 treedbs 55% longer; written not durably now (it holds 0, an empty one is remade with a warning), both back within the noise. The fsyncs of a key delete on a topic with feeds are not measured (no benchmark covers it), and the texts say so |
| 2. Versions | `YUNETA_VERSION` 7.26.0, `RELEASE` 1, CLAUDE.md (e09b8b0c2, 5f504cb53) |
| 3. JS | gobj-ui 7.26.0 on npm (its tag 7.26.0); gui_agent 0.29.11 / gui_treedb 0.17.77 on `^7.26.0`, and wattyzer's and both yunovatios GUIs; `verify_js_api_coverage.py --repin` + `--write`; all deployed |
| 4. Suites | 292/292 local (`ulimit -Sn 1024`) and 292/292 on wattyzer, on the final HEAD (after the durability fix) |
| 5. Tag | Tag `7.26.0` (b6b0dff7b), GitHub release Latest with the report attached, packages 7.26.0-1 `.deb`/`.rpm`; `check_doc_line_refs.py --repin=7.26.0` (1713 links, 199 anchors drifted), myst cache cleared, deployed and checked live |

Deployed on all six nodes (local, wattyzer, a.com, hidraulia from source;
yunovatios-controlador `.deb`, -central `.rpm`), snaps `pre-7.26.0`, the two
agents of each node at 7.26.0 one at a time, a test email from each
emailsender ("email sent" on all six). After the tag, artgins failed
`test_fs_watcher_overflow` on a fixed 200 us limit with the code right: the
test now holds the watcher's cost on a big tree against the same pass on a
small one (2c7a2c292).
