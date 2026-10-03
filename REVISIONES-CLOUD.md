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
