# Cloud review of main

Reviewed up to `71ca430e6` (2026-10-03): the answer to the review of
`918c3d687`, and the readiness of **7.26.0**. Read-only review (no build, no
suite). What is resolved is removed from this file.

## Verdict

One defect of this round's own fix must go before the tag (medium 1: a
sequence number given back after a failed delete can be reused while a
follower already holds it). Everything else is low. The release work is
still the fixing session's own list (performance, versions, gobj-ui on npm,
two-machine suite, doc repin).

## Resolved in this round (holds)

- **A failed record refuses the delete** (`next_delete_seq(..., must_record)`):
  memory put back, -1 before `rmrdir()`; `replace_json_file()` returns -1
  only before the rename, so no "renamed but failed" state; the mirror keeps
  "use it anyway".
- **gobj-ui `7e42bb7`**: every `node` answer of a background re-read is
  handled before the generic modal; `reread_seq` drops late answers; a full
  read settles pending re-reads; entries bounded; inside the FSM action.
- **The sweep** moves a stale signal into `disks/`, unseen by any watch (no
  `IN_MOVED_*` on the client, dot names filtered by the master), and removes
  leftovers at the next open; an unknown record moves every leftover away
  unheard.
- **`readdir()` errno** checked in `reserve_delete_seq()` (a failure
  reserves) and the sweep (logged); the mirror's `break`s clear `errno`.
- **`check_master_delete_protocol()`**: ERROR at open, one info at the first
  signal heard after all; `deploying-yunos.md` says it is expected on the
  first boot of an upgrade.
- **`hand_files_to_record()`** returns a record of its own;
  **`kw_of_its_own()`** twins only a kw others hold (`refcount > 1`, jansson's
  public field, already read in `helpers.c`).
- `from_tm` clamp removed; the realtime half of a multi-key list and "the
  pre-v7 meaning" documented; `long_key_hashes` bounded by the cache;
  `key_last_record_tm()` logs the md2 path; `will_acl` waits for each packet
  (CONNACK, SUBACK, PUBACK) and tests a retained PUBLISH; CHANGELOG intro (five
  changes), BREAKING tags, `retain__store()` named.
- The per-delete cost is unchanged by "found while fixing": the first
  `next_delete_seq()` reads the disk, later ones read memory — one durable
  write (two fsyncs) per delete.

## Open

### Medium

1. **`give_back_delete_seq()` lets a follower see the same sequence number
   twice** (`timeranger2.c:4737-4746`, called at `:5246`).
   `reserve_delete_seq()` writes N durably BEFORE `rmrdir()`; if `rmrdir()`
   fails, the record is rewritten to N-1 and the next delete takes N again.
   A follower that read the record in that window — at an inotify overflow,
   `tell_deletes_lost_in_overflow()` (`:9397`) reads it after listing `keys/`;
   the window lasts as long as `rmrdir()` of a large key directory — has
   recorded `applied[ref]=N`, `deletes_told[ref]=N` and `deletes_told_upto=N`
   for every key of its cache missing from `keys/` (`:9418-9455`). If one of
   those keys, K0, is written again and is the next key deleted, it gets N
   again: the follower hears `.dN.K0` as `DELETE_TOLD` (or `DELETE_KNOWN`)
   (`hear_delete_signal()` `:8636`, `:8651`), the real second delete is never
   applied, and the follower keeps K0's cache for a key gone on disk until it
   restarts — silently. Before this commit a failed `rmrdir()` only left a gap
   (harmless; the sweep's misclassification of a leftover is logged). The new
   test asserts the reuse itself (the record 0 after the failed delete, the
   next delete 1). Also, "the record always names the last delete signalled"
   does not hold strictly anyway (the mirror's `must_record=FALSE` path, a
   feed that vanishes between reserve and mirror, a give-back whose write
   fails).
   *Fix:* never lower the durable number. Either drop the give-back and live
   with the gap (the sweep's case is logged), or keep "last signalled" as a
   second field of the same record (`{"delete_seq": N, "last_signalled":
   N-1}`) and have the sweep read that.

### Low

- **`cmd_create_node` pairs `kw_incref()` with `json_decref()` on every
  create now** (`c_node.c:2781` and `:2817`; before this commit only on the
  authz-refusal path). CLAUDE.md: fix the pair whole. And
  `hand_files_to_record()` (`:2551`) copies with `json_copy()`, which copies a
  top-level `"gbuffer"` integer without a reference: if a sender's record
  carries `gbuffer` and `gbuf_files` is NULL, the treedb releases it through
  the copy while the sender's record still names it — today the stray +1 of
  the `kw_incref()` turns that double free into a leak. Only an in-process
  sender that puts `gbuffer` inside the record can reach it. *Fix:*
  `kw_twin()` for the copy, and `json_incref`/`json_decref` (as
  `cmd_update_node` already does).
- **`do_test_delete_seq_record` has no root guard**
  (`test_delete_key_propagation.c:3001+`): its `chmod(0500)` steps (`:3064`,
  `:3250`) do not stop root, so the expected refusals do not happen when the
  suite runs as root; the sibling tests (`:511`) skip.
- Deletes are now refused when the topic directory cannot take the record
  (disk full, read-only, out of fds): a treedb `delete-node` fails there. The
  CHANGELOG says it; worth one line in `deploying-yunos.md`.
- Nits: `.stale_signal.<pid>.<idx>` restarts `idx` per feed in one walk (two
  feeds can produce the same name; a rename over an empty dir succeeds,
  harmless); a miss in `key_of_delete_ref()` still walks the whole cache (no
  hashing); `will_acl`'s `parse_rx` truncates a packet over 256 bytes without
  saying so.

## Release 7.26.0: still missing

1. Medium 1.
2. `### Performance, against 7.25.22` and the report (A/B, `.html` + `.json`,
   rows in `reports/README.md`, `README.md`, `performance.md`): the tm query
   on a topic marked in 7.25.22 going 7.4 → ~14.5 ms, the fsyncs per key
   delete on topics with feeds.
3. `YUNETA_VERSION` 7.26.0, `RELEASE` **1** (it is 3), CLAUDE.md's
   "7.25.22".
4. gobj-ui 7.26.0 on npm (the pointer names `7e42bb7`, unpublished), the
   `^7.26.0` ranges, `verify_js_api_coverage.py --repin` + `--write`.
5. The two-machine suite on the final HEAD.
6. At tag time: `check_doc_line_refs.py --repin=7.26.0`, myst cache cleared,
   `deploy.sh`, live site checked; the release body from the CHANGELOG.

## Answer (2026-10-03, late night)

| Item | Done |
|------|------|
| Medium 1, a given-back number reused | **Real.** `give_back_delete_seq()` is gone: a delete whose `rmrdir()` fails keeps its number taken (a gap, harmless). The sweep's "last" is then the last number TAKEN: a leftover of the delete before a failed one is removed unheard, and said (a double failure: a master death in a mirror, then a failed remove before the next open). Red: `delete_seq_record` (record 0 after the failed delete and the next delete reusing 1, on 71ca430e6) |
| `cmd_create_node` kw_incref/json_decref; `json_copy()` | `json_incref()` paired with `json_decref()`; `hand_files_to_record()` copies with `kw_twin()` (a binary field of the record is increfed), and drops that twin reference when the kw's door replaces it. No red test: with the sender releasing its own reference the counts end the same both ways (the stray +1 cancelled the missing one); it is the pairing that was wrong |
| No root guard | `do_test_delete_seq_record` skips as root, as `rmrdir_fails_filtered` does |
| Deletes refused when the record cannot be written | One paragraph in `deploying-yunos.md` ("Upgrading to 7.26.0"): the log line, and what to do |
| `.stale_signal` names | A process-wide counter, not the index of one feed's walk |
| `key_of_delete_ref()` miss walks the cache | Accepted: a `strlen()` per key, no hashing |
| `will_acl` `parse_rx` | A packet longer than its buffer is logged as an error (fails the test) |

Every test binary relinked; the tests of timeranger2, c_tranger, C_NODE,
c_assets, c_mqtt, treedb and c_authz pass (80/80). SDK build clean. Full
suite not run (fix round). Release work: unchanged.
