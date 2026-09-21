# TODO

Only **open** work lives here. Anything shipped is deleted from this file the
moment it ships — its record is [`CHANGELOG.md`](CHANGELOG.md) (release notes),
the docs (`yunos/c/yuno_agent/YUNO_AUTH.md`,
`docs/doc.yuneta.io/yunos/mqtt_broker.md`,
`docs/doc.yuneta.io/guide/guide_tls.md`) and git history.

## `gobj_post_event()`: ESP32 still has its own contract

C and JS agree since 7.10.0 (JS aligned in gobj-js `7.10.0`). ESP32 does not:

| | C + JS | ESP32 (`c_esp_yuno.c`) |
|---|---|---|
| When | next turn of the loop, a snapshot at a time | an `esp_event` loop |
| Destroyed destination | entry dropped | not handled |
| Destroyed source | `src` cleared, event still delivered | not handled |
| Undeclared event | refused when posting, so the error names the caller | seen later |
| Ceiling | 10000, and reaching it is an error | the esp_event queue |
| Trace | a `machine` line | none |

The two that matter are the lifetime ones: an event delivered to a destroyed
gobj is a crash, and on ESP32 nothing stops it today.

## Schema editing: the admin console

The backend is done (see below for what it still lacks). The client is not, and
it does not belong in an application SPA: an end user edits data, not the shape
of it. `gui_treedb` also *cannot* apply a schema change — it connects to yunos
directly and `pause-yuno`/`kill-yuno` are agent commands — and that inability is
the form telling the truth about the function.

**Grow `gui_agent`, do not merge the two.** It already has the agent connection,
the yuno list and the lifecycle commands, which is most of what an admin console
needs; `gui_treedb` stays what it is, a data browser pointed at a backend.

**DONE (`yunos/js`, gui_agent):** the **Schemas** workspace, the routing
adapter it needed, and the discovery of which treedbs a yuno exposes
(`services`, filtering `C_NODE`, declared as a **tree of nodes** with
`treedb_system_schema` first, and probed per yuno on expanding a node in the
picker so a yuno with none is marked there). Each treedb is a `link` node of a
`C_YUI_NODE` rooted at the tab's route, so the depth (yuno → treedb → topic) is
navigation and not a `<select>`, and how it is drawn — strips, back, breadcrumb
— is a Preferences choice. `C_AGENT_TREEDB_LINK` implements `mt_command_parser`, takes
the view's command verbatim, re-wraps it as `command-agent` +
`cmd2agent="command-yuno id=<yuno> service=<treedb> command=<cmd>"`, and puts
the original command back on top of the `command_stack` before handing the
answer to the view; `C_AGENT_TREEDB` mounts the library's views with
`yui_mount_service_view()` against it. The two traps it had to be born knowing
(the whole kw is the yuno filter; the routed path loses the live node events,
echoed locally for its own writes) are in its header and in
`gui_agent/README.md`.

The **node's own AGENT** is an entry of the picker too (sentinel yuno id
`__agent__`): it never appears in `list-yunos`, but it runs the same services
and its treedbs — `treedb_yuneta_agent` included — were unreachable from the
console. One line differs, `command-agent service=<treedb>` instead of
`command-yuno id=<yuno> …`, and it lives in `cmd2agent_service()`. On the
`.ovh` plane the same row is **agent22**. Apply is disabled there: the agent is
not a managed yuno, so it is restarted on the node.

The **record GRAPH** is there too, as a position of the same url
(`<treedb>/graph[/<topic>]`, the third icon of every topic card): the treedb's
viewer hosts `C_YUI_TREEDB_GRAPH` on the same adapter and swaps the two bodies,
lazily, because G6 is the heaviest thing the workspace draws. A replica hands it
the same `readonly` the editor gets and it loses its `edition` mode (gobj-ui
5.16.0), and a write in it marks *Apply* — except a save of the graph LAYOUT,
which is the view's own bookkeeping in `__graphs__`.

**DONE (gobj-ui 6.2.1 + gui_agent 0.8.1):** the workspace stopped showing the
three topics a schema is STORED in and shows the schema they ARE.
`C_YUI_SCHEMA_EDITOR` is the landing of `treedb_system_schema` (segment `edit`;
the raw tables keep `raw` and their own names): treedb → topics → columns in
declared order, reordered by dragging, flags as checkboxes that say what they
do, the schema DRAWN from the records being edited (that button drew the meta
schema before — the same three cards on every yuno), a **check** of what the
treedb would refuse, an **export** as the C literal and an **import** shown as
a plan. Both versions travel with every write, so the operator is asked to
remember neither. This one IS a library gclass: the logic is pure and tested
apart from the view (`schema_model`, `schema_validate`, `schema_descs`,
`schema_to_c`, `schema_import`, `schema_flags`, `schema_write_options`), and
the app mounts it unchanged like the other two.

What that workspace still lacks:

- **Applying needs an agent carrying the `ac_final_count` fix of 7.13.0.** The workspace
    does it now (`kill-yuno` → `run-yuno play=0` → `play-yuno`, confirmed
    first), but those commands answer through `ac_final_count()`, which dropped
    the answer of any client behind a controlcenter until now. Against an older
    agent the sequence stops at the first step with the yuno KILLED and not
    restarted; the tab gives up after 30 s and says so, but the yuno stays
    down until somebody runs it. Deploy the agent before using Apply on a
    node.

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

- **Nothing is ever removed from a projection.** By design: an element that is
    in `__system__` and not in the incoming schema cannot be told apart from an
    operator addition. The consequence is that a topic or column dropped from
    the C literal lingers in `__system__` until somebody removes it explicitly,
    and with `impose_c_schema=0` it stays in the schema the treedb opens
    with. Telling the two cases apart needs state the projection does not carry
    (which side wrote each element), so decide that before adding a rule.

- **Not every treedb is projected at all.** `C_AUTHZ` creates its `C_NODE`
    directly instead of going through `C_TREEDB`'s `open-treedb`, so
    `treedb_authzs` never reaches `__system__` and cannot be edited. Any other
    direct `C_NODE` consumer is in the same position.

- **The ESP32 persistence (`esp_persistent.c`) has the defects fixed in
    `dbsimple.c`, and more.** `save_json()` returns 0 when `nvs_set_str()` or
    `nvs_commit()` fails, and `dbesp_save/remove_persistent_attrs()` drop its
    result anyway, so a value that was not saved reports success.
    `load_json()` returns on `ESP_ERR_NVS_NOT_FOUND` without `nvs_close()` (a
    handle lost on every load of a gobj with nothing saved), and uses raw
    `malloc`/`free`. `dbesp_load_persistent_attrs()` leaks `keys` when nothing
    is saved. Not changed with the Linux fix: the `ESP_PLATFORM` code is not
    built or tested on the development node.

- **A removed column with data behind it is still nobody's problem.** The
    write guard refuses what cannot produce a working schema, but dropping a
    column that has values in the topic's records is a legal schema and a data
    decision: the records keep the field, every reader stops showing it, and
    nothing says so. Deciding what the GUI does here (warn, refuse, offer to
    keep it hidden) needs the record side, not the schema side.

- **Applying an edit costs a `pause-yuno` + `play-yuno`.** That is the whole
    yuno, not just the treedb: its gate goes down for the cycle, and any client
    connected to it — the editor included — has to reconnect. Reopening only the
    treedb is not available to a third party and should not be: `close-treedb`
    destroys services whose handles the owner has cached (it now refuses while
    the yuno plays). A true in-place reload would live in the owner, as a local
    method it implements — "reload your schema" — using
    `tranger2_write_topic_cols()` (written, called by nobody) plus
    `parse_schema_cols()` + `parse_hooks()` and a link reload when hooks or
    fkeys change.

## TreeDB / timeranger2: open findings of the 2026-09-15 review

A read-only review of timeranger2, tr_treedb, their gclasses, gobj-ui's treedb
views and the docs. Every finding marked "high" was checked by hand in the
code. What shipped is in `CHANGELOG.md` (`str2system_flag`, flag-less column,
`rowid`, re-link of a string fkey, hook+fkey refused, and the rest of the
highs). This is what is open.
Line numbers are those of `main` on 2026-09-15.

**High**: none open. A1-A11 shipped on 2026-09-15 (see `CHANGELOG.md`).

**Found while fixing (2026-09-15)**

- **`test_c_node_link_events` fails now and then** -- twice on 2026-09-15, both
  inside runs of several suites, never alone (15/15 twice). Its message was
  not kept. Next time it fails, keep `build/Testing/Temporary/LastTest.log`
  before running anything else: ctest overwrites it.

**Medium**

- **timeranger2**: nothing open from the review. (Fixed on 2026-09-15: the
  propagation of `delete_key` to a follower, a feed closed from inside its own
  fs_watcher callback, and an append whose `__t__` belongs to an earlier file
  -- see `CHANGELOG.md`.)
- **tr_treedb**: `treedb_delete_instance()` does not unlink, and
  that matters in ONE case only (analysed 2026-09-15): the loader links only the
  `id` index (`load_all_links()`) and hooks dedup by child id, so a non-primary
  CHILD instance never sits in a parent's hook. A non-primary PARENT instance
  can hold children linked during the session; deleting it leaves them under no
  visible parent until the reload re-hangs them from the primary (their fkey
  names only the id). The agent's `delete-config` / `delete-binary` refuse a
  version still in use ("Using in N yunos") unless `force=1`. Low impact; a
  fix would move those children to the primary's hook. (Fixed on 2026-09-15:
  a pkey2 value changed by an update, and the OLD tag the snapshot clone left
  in memory -- see `CHANGELOG.md`. Decided and fixed on 2026-09-16 and
  2026-09-19: a save is always untagged -- only shoot-snap tags a record,
  active snap or not -- so every snap freezes what it shot; the delete guard asks the key's records and the asset gc
  holds what a shot record names while the snap exists.)
- **tr_treedb snaps -- reviewed 2026-09-17, CLOSED 2026-09-19/20.**
  A snap is a PHOTO of an instant and must never be written into. The two
  gaps and the design decision of (1) are all answered; the layout of the
  graph (`__graphs__`) joined the photo on 2026-09-20.
  1. **CLOSED 2026-09-19 (the user's rule): a record takes a snap's tag
     exactly once, from shoot-snap.** `treedb_save_node()` /
     `treedb_create_node()` write tag 0 even while a snap is ACTIVATED (they
     used to take its tag and write into the photo: a binary installed during
     a rollback became part of the snap). With a snap active the primary
     index shows the snap's records, and the pkey2 indexes every other
     instance (all but the one in the primary) -- that is intended. An edit
     made while a snap is active reaches the primary index after the
     deactivation. **And the last question of this section is answered too
     (the user, 2026-09-20): an activation IS a filtered load and stays one
     -- the old option B, activation as a RESTORE, is discarded.** What it is
     for: going back to a state marked as good, to look at it or to carry on
     from it; and working from it ignores what was written after the shot,
     for every key touched, without destroying it. Written up in
     `YUNO_TREEDB.md` 3.9 ("Working from a snap"), on the API page of
     `treedb_activate_snap()` and beside the rollback recipe of
     `YUNO_LIFECYCLE.md` 6.6. **Nothing is open in this section.**
  2. **CLOSED 2026-09-20: `treedb_delete_instance()` asks the RECORDS, not
     the tag in memory.** `instance_held_by_a_snap()` walks the key's records
     keeping only those of this instance (the pkey2 value is a FIELD, so that
     walk reads the content, not only the metadata) and refuses when one of
     them carries the tag of a snap that exists: "cannot delete instance, a
     snapshot still holds it". `force` overrides. Before, an instance updated
     after a shot had tag 0 in memory and the delete tombstoned every md2 row
     of that (id, pkey2), the frozen one included.
     (The side effect noted with it -- meta-topics tagged while a snap is
     active -- went with (1).)
- **C_NODE / C_TREEDB**: nothing open from the review. (Fixed on 2026-09-15:
  every C_NODE command asks a permission; and on 2026-09-16: `mt_treedbs`
  answers a list, not an envelope -- see `CHANGELOG.md`.)
- **gobj-ui (treedb views)**: nothing open from the review. (Fixed on
  2026-09-16 and shipped as gobj-ui **7.23.169**, deployed to all six
  consumers: the Op-column pencil, the confirm dialogs, the `setTimeout`
  deferral, the kws carrying G6 event objects and the writes run from DOM
  callbacks all cross the FSM now; the table's search no longer matches the
  COUNT of a hook; `ac_unselect_rows` reads its own attr; the toolbar buttons
  and the search box carry `title`/`aria-label`; the form dialog's title
  re-translates; and the first `graph.render()` guards against its own view
  being gone. See that repo's `CHANGELOG.md`.)

  One item of the report was NOT a defect and was left alone: it asked that
  `C_YUI_TREEDB_TOPICS` declare `EV_SELECT_ROWS`/`EV_UNSELECT_ROWS`. The
  library settles that shape the other way round -- the event is opt-in and
  the host that TURNS IT ON declares it, as `with_node_click` says -- and
  nothing turns these on, so a declaration there would be the no-op action
  `CLAUDE.md` forbids. The contract went into the attr descriptions.

  **Found while fixing, not from the review and not fixed** (all LATENT, a
  static scan of who hosts whom): three hosts do not declare an output event
  of a gobj they subscribe to. `C_YUI_NODE` creates `C_YUI_NAV` as a pure
  child (`c_yui_node.js:705/760/969`) and declares neither
  `EV_NAV_ITEM_CLOSE` nor `EV_DRAWER_CLOSE_REQUESTED`, which `c_yui_nav.js`
  really does publish -- the likeliest of the three to bite.
  `C_YUI_TREEDB_TOPIC_WITH_FORM` creates `C_YUI_JSON` as a pure child
  (`:2522/2651`) without declaring `EV_EXPAND_PATH`, which only fires on a
  `__collapsed__` sentinel and those come from the backend, not from a schema
  or a cell. `C_YUI_TREEDB_TOPICS` hosting `C_YUI_TREEDB_SCHEMA` is the
  documented opt-in case, not a defect.
- **Docs**: nothing open from the review. (Fixed on 2026-09-16:
  `YUNO_TREEDB.md` shows `schema_version` at the treedb level, says
  `sf_zip_record` / `sf_cipher_record` are not implemented, and seeds its
  `initial_load` example with the agent's real `roles` / `users`. The
  `kernel/js/gobj-ui/README.md` item was stale: its asset section already
  described the `get-asset` / `yui_asset_*` model.)

**Tests nobody has** (in order of damage): `delete_instance` with links.
C_NODE commands with no ctest: `node`, `instances`, `pkey2s`, `jtree`,
`parents`, `children`, `hooks`, `links`, `treedb-info`, the snap commands,
`import-db` / `export-db`, `print-tranger`, and the refusals on a replica. In
gobj-ui, no gclass of the treedb views has a test: the save kw as it leaves
`publish_treedb_write` would have caught A8 (its column rule is unit-tested
since 7.23.170, the kw itself is not). Covered since 2026-09-16, no longer
missing: changing a pkey2 value (`tr_treedb_update_instance`), two hooks on
one fkey (`tr_treedb_schema_parse`), the snapshot clone followed by updates
(`tr_treedb_snap_clone`).

## TreeDB / timeranger2: open findings of the 2026-09-21 review

A second read-only review, of everything done since the base of the first one
(7.20.0 / gobj-ui 7.23.167): 39 C commits, 39 of gobj-ui, 4 of gobj-js, 43 of
yunos-js, read at yunetas 7.24.1, gobj-ui 7.23.192, gobj-js 7.22.2. Every high
and medium went through an adversarial verifier told to refute it, most were
reproduced against `outputs/lib`, and the highs were read again by hand. The
suites were all green while it ran (ctest 137/137, gobj-ui 823, gobj-js 146,
gui_treedb 60): **no test catches any of this.** Nothing is fixed yet. Line
numbers are those of `main` on 2026-09-21. **s/v** marks the five findings
whose verification did not run.

Six causes explain most of the list, and fixing the cause closes several
entries at once:

1. **The replica is the path nobody walks.** The master guard went into C_NODE
   command by command; its siblings were left out. No test opens a treedb as a
   replica to write. The guard belongs in `tranger2_*`.
2. **`on_critical_error=2` turns a failed READ into a dead yuno that is not
   relaunched** (exit 0 means "no relaunch" to `ydaemon`). A3, A5 and M10 share
   it. A file that is no longer there is not a reason to leave the process.
3. **Names from the wire: keys are validated, topics are not.**
4. **Fixes of the first review that moved the defect** instead of closing it:
   d60ec78, 441937134, 3fea635f3, 3664eb55e, c46c820a0, 141277953, af1489c66,
   and gui_treedb's 9cdd16b, which does nothing.
5. **`force` means two things**: "unlink the children" and "skip the snapshot
   guard" -- and `delete_node()` says the opposite for assets (*"force means
   'unlink the children', never 'ignore what a snapshot needs'"*).
6. **Three sites unlink BEFORE knowing they can link**, and the -1 is dropped
   on the way, so the caller is told it worked.

**High**

- **A1, A2, A3 -- SHIPPED (unreleased, see `CHANGELOG.md`).** A topic name is
  confined to its database at the tranger2 boundary (empty, `.`, `..`, `/` and
  `` ` `` refused; a leading `.` stays legal for the MQTT queues),
  `tranger2_open_topic()` answers NULL for a directory without
  `topic_desc.json` instead of a critical, and `tranger2_delete_topic()` /
  `tranger2_backup_topic()` / `treedb_delete_topic()` are master-only. Test:
  `tests/c/timeranger2/test_topic_path_traversal.c`. Still open from their
  text: C_NODE's `snap-content` does not ask `treedb_is_treedbs_topic()` -- it
  can no longer leave the database, but it can read a topic of the tranger that
  is not a topic of that treedb.
- **A4 -- C_TRANGER judges a handle alive by the NAME of its topic.**
  `live_handle()` / `iterator_is_live()` (`c_tranger.c:548-571`) ask only
  `tranger2_topic_is_open(name)`. `tranger2_close_topic()` frees every iterator
  and rt of the topic, and anything that opens that name again makes the stale
  pointer "live": `delete-topic` + `create-topic`, a `gobj_stop` + `gobj_start`
  of the service (C_TREEDB does it to `tranger_system_schema`), or
  `tranger2_backup_topic()`. The range made it worse: a multi-key entry holds N
  raw pointers, and `ac_on_close` -> `reap_handles_of()` (af1489c66)
  dereferences the handle by itself when the client's tab closes. Reproduced as
  a SIGSEGV in `tranger2_close_iterator`. Fix: resolve by identity at each use
  (`tranger2_get_iterator_by_id` and the rt twins), clear the three registries
  in `mt_stop`, purge a topic's entries in `cmd_delete_topic`.
- **A5 -- first half SHIPPED (unreleased, see `CHANGELOG.md`; gui_treedb
  0.17.52).** Every iterator C_TRANGER opens registers the `key_deleted`
  callback, which marks it; its next `get-page` closes it and says why, so a
  deleted key no longer reaches `get_topic_rd_fd()` through an open iterator,
  whoever deleted it. gui_treedb re-opens its whole-topic Rows card on the
  delete-key answer. **Second half SHIPPED too (unreleased): a failed READ
  never exits the process** -- the user's option C. `get_topic_rd_fd()` and
  the three criticals of `get_md_record_for_wr()` no longer apply
  `on_critical_error`, and a read no longer creates a missing md2. The short
  md2 write (`:3019`) KEEPS its exit, the user's call. Test:
  `test_read_never_exits`. The classification it was decided on:
  - The only READ path that can exit is `get_topic_rd_fd()`
    (`timeranger2.c:2512`, `master?on_critical_error:0`); the rest of the read
    side (`read_md`, `read_record_content`, `load_first_and_last_record_md`)
    already passes 0.
  - `get_md_record_for_wr()` (`:3461/3481/3502`) is read-before-modify, the
    trigger is caller input (a bad rowid or `__t__`), it returns before any
    write, and it is also reached by the pure read `tranger2_read_user_flag()`.
    Safe to pass 0. Hidden side effect to fix with it: on a master,
    `get_topic_wr_fd()` -> `create_file()` makes an EMPTY md2 for a `__t__`
    whose file does not exist, from a read.
  - The one write that must keep exiting, or grow a rollback: `:3019`, a SHORT
    write of the 32-byte md2 row leaves every later append misaligned and the
    file unreadable at the next load (`:6438`). With 0 it needs an
    `ftruncate(md2_fd, offset)` before returning.
  - Also reached through helpers that take `on_critical_error`
    (`load_persistent_json` / `save_json_to_file`): `timeranger2.c` 449, 460,
    519, 529, 886, 1207; `tr_treedb.c` 671, 720, 1660; `tr_msg2db.c` 150, 190.
    Loads are reads; saves are writes.
- **A6 -- gobj-ui: the delete of ONE row is resolved by POSITION after the
  confirm dialog.** `c_yui_treedb_topic_with_form.js:1553` sends `{index:
  getPosition(), row}`, `:4015` builds the question from `kw.row` and hands
  `confirm_then` only the index, and `:4062` resolves
  `getRowFromPosition(index)` when the person answers. A Tabulator position is
  the index inside the DISPLAYED rows of the page and is regenerated by every
  `addData` / `deleteRow`, and the view applies `EV_TREEDB_NODE_*` of every
  writer while the dialog is open -- so the record deleted can be another one
  than the dialog named, with `force: true`. Regression of **d60ec78**: to keep
  the kw plain json it replaced the record by its position. Found by two
  reviewers, reproduced with the real Tabulator. Fix: carry the identity (`id`,
  and the pkey2 values where the topic has them), resolve with `getRow(id)`,
  refuse loudly when the row is gone.
- **A7 -- C_AUTHZ `disable-user` hands the NODE to `EV_REJECT_USER`**
  (`c_authz.c:2036`). The event reads `username`, the node keys on `id`, so
  the handler logs *"User not found"* and frees its kw: the user's live
  sessions are never dropped (`disabled` is only read at login), and the same
  pointer then goes into the response as `jn_data` -- a use after free, SIGSEGV
  in 2 of 7 runs with a kw that carries `__md_iev__`. It is the bug 4595afeca
  fixed in `delete-user`, whose comment at `:2128` describes it. Predates the
  range. Fix: `json_pack("{s:s}", "username", username)` as the kw, and check
  the NULL of `gobj_update_node()`.
- **A8 -- four agent deletes hand a BORROWED list element to
  `gobj_delete_node()`**, which owns its kw: `c_agent.c:2578` (public_service),
  `:2945` (realm), `:3752` (binary), `:4180` (config). Each element has
  refcount 1, so it is freed inside the list and `JSON_DECREF(iter)` then
  decrements freed memory. Every other caller increfs (`c_node.c:2828`) or
  builds a kw (`delete-yuno`). Predates the range. Fix: `json_incref(node)` at
  the four sites, and `kw // owned` written on `gobj_delete_node` in `gobj.h`.
- **A9 -- `shoot-snap` while a snap is ACTIVE restores the whole treedb**
  (`tr_treedb.c:13803`; the verifiers rated it medium). With snap A active and
  reloaded, the primary index holds A's records, all tagged, so EVERY key takes
  the clone branch and gets the photo's content as its newest record. After the
  deactivation the reload picks the newest: everything A knew is back to its
  content at the shot and what was written since is buried. That is the
  "activation as a restore" discarded on 2026-09-20. Fix: refuse `shoot-snap`
  while a snap is active.

**Found while fixing A2 (2026-09-21), not from the review:**
`tranger2_list_topic_names()` skips every entry that starts with `.` and
returns the `<topic>.bak` backups as topics. So the MQTT broker's orphan-queue
clean-up (`c_mqtt_broker.c:829`) never sees a `.foo-IN` queue (legal: the
broker accepts `.foo` as a client id), and it takes `X-OUT.bak` for a queue
with no session and deletes it -- the backup `tr2q_mqtt.c:726` just made.

**Medium -- tr_treedb, C_NODE, the agent**

- **M1 -- `treedb_replace_links()` still unlinks first** (`tr_treedb.c:8927`).
  A refused new parent (the cycle check, "parent node not found", a hook that
  does not link that column) leaves the child orphaned ON DISK with UNLINKED
  published; `mt_update_node` drops the -1 (`c_node.c:1099`) and `update-node`
  answers *"Node update!"*. The comment that justifies the order stopped being
  true with 3fea635f3. Reached by gobj-ui's form (`autolink: true` always).
- **M2 -- the `fkey: {parent: hook}` mark of `parse_hooks()` is persisted in
  the CHILD's `topic_cols.json`** (`tr_treedb.c:2479`, regression of
  441937134). A hook rename raises only the parent's `topic_version`, so the
  child reloads the stale mark: *"Only can be one fkey"* on every open, and the
  links made through the new hook are lost at every restart.
- **M3 -- a ref to a hook that no longer exists is not treated as stale**
  (`tr_treedb.c:8432`). Since 3fea635f3 `_link_nodes()` unlinks the old ref
  first and returns -1 when that fails, so the node can be neither re-linked,
  cleaned, nor deleted with force. It used to repair itself at the next link.
- **M4 -- the rowid counter is seeded from the SNAP-FILTERED index**
  (`tr_treedb.c:5288`, 3664eb55e). A store with no `last_rowid_id` yet, a snap
  active and a create without id give an id that exists on disk, and
  `exist_primary_node()` reads the same index. Real case: `__graphs__`. Seed
  from the keys of the topic. (`find-new-yunos create=1` does not reach it: it
  sends the id.)
- **M5 -- `now`: the census in the 7.24.0 CHANGELOG is false.** Besides the two
  meta-topic columns there are 7 in the SDK and about 20 in the projects,
  `['time','now','persistent']` with no `writable`, most headed "Update Time".
  The SDK's are stamped because their writers carry the column; the projects'
  stay frozen at the create. Untested shapes of the gate: `now` + `writable` on
  a string column becomes `""`; a `required` integer `time` refuses the update.
- **M6 -- `create-topic` persists a topic whose columns failed validation and
  answers *"Topic created!"*** (`tr_treedb.c:1626`): `parse_schema_cols()` runs
  after `tranger2_create_topic()`. A topic with no `id` column, or no cols, is
  accepted without a log.
- **M7 -- `check_system_schema_write()` skips two per-column rules**
  (`tr_treedb.c:3660`): `file` needs `fkey` + string, and the hook/fkey type
  rule. A bad column stored loses the whole topic at the next open.
- **M8 -- "a change to a schema publishes itself" is false for a DELETE and for
  a column created by autolink** (`tr_treedb.c:3510`): only update and link
  call `publish_schema_change()`. `YUNO_TREEDB.md:1616` says otherwise.
- **M9 -- the two snapshot guards fail OPEN** (`tr_treedb.c:12766`, `:12907`):
  when `tranger2_open_list()` fails they log and answer FALSE, and the delete
  goes on.
- **M10 -- the three snap commands of C_NODE have no replica guard**
  (`c_node.c:4332`, 141277953 rewrote their preambles). `shoot-snap` on a
  replica ends in *"Cannot save record tag"* critical and `exit(0)` (every
  C_AUTHZ with `master=0`); activate / deactivate change memory only and
  deactivate answers *"Snap deactivated"*.
- **M11 -- the agent's `delete-yuno` guards by the tag in MEMORY and then sets
  `force = 1`** (`c_agent.c:5121`, `:5143`), so the guards that read the
  records (08ba69dcb, 114dfb339) never run for `yunos`, and the memory tag is 0
  for anything saved after the shot.
- **M12 -- gobj-ui's topic table always deletes with `force: true`**
  (`c_yui_treedb_topics.js:2755`), so no snapshot guard can fire from the GUI
  and the dialog speaks only of unlinking children. The graph sends no options
  and the guards do fire there. Needs the owner's decision on the two meanings
  of `force`.
- **M13 -- SHIPPED with A1**: `delete-treedb` on a replica answers READ-ONLY
  before `delete_client_treedb_schema()` touches memory.
- **M14 -- regression of 141277953: every `update-node` WITHOUT `options` logs
  an ERROR with a stack** (`c_node.c:2713`): `kw_get_bool()` on a NULL dict.
  `mt_update_node` guards it, the command does not. It is the form the docs use
  with ycommand, and it breaks a whitelist log assertion.
- **M15 (s/v)** -- `update-binary` / `update-config` answer result 0 when
  `gobj_update_node()` returns NULL, and `sync-binaries` reads that as OK
  (`c_agent.c:3530`); `delete-realm` always answers 0 (`c_agent.c:2956`);
  C_AUTHZ's `update-user` with a role that cannot be linked strips every role
  and answers *"User updated"* (`c_authz.c:4127`, the mechanism of M1); a DICT
  hook takes the newest child instance, so a deleted instance stays in the
  hook and a forced delete of the parent resurrects it on disk
  (`tr_treedb.c:4582` -- this contradicts the premise the `delete_instance`
  bullet of the first review was closed on); a child held by the hooks of
  several instances of one parent is unlinked from ONE (`tr_treedb.c:8167`).

**Medium -- timeranger2, C_TRANGER**

- **M16 -- after a late record lands in an earlier md2 file, time-range queries
  hide records** (`timeranger2.c:6274`, c46c820a0). A cell's `[fr_t, to_t]` is
  rebuilt from the FIRST and LAST md2 rows, and the late record is the last row
  with a lower `t`. A follower gets it wrong at once, the master after a
  reload. The CHANGELOG claims the opposite (*"A reload says the same"*).
- **M17 -- `find_cache_cell()` makes every append O(md2 files of the key)**
  (`timeranger2.c:6076`, c46c820a0): measured 3.7 us flat, 32 us at 365 files,
  318 us at 3650. Look at the LAST cell first and walk back only when it is not
  the one. Queues (fixed mask) and treedbs (`%Y`) do not feel it.
- **M18 -- a follower with two rt_disk feeds on one key** loses, for the feed
  that fires second, the records of BOTH files when two files of the key arrive
  in one batch (`timeranger2.c:5629`): one watermark per (feed, key), not per
  file as the doc says. From 7.8.0; c46c820a0 adds a second supported way in.
- **M19 -- closing the LAST Live card of a session also reaps its paging
  iterators** (`c_tranger.c:3177`): af1489c66 added the right signal (the
  session's `EV_ON_CLOSE`) and `mt_subscription_deleted` still calls
  `reap_handles_of()`. The Rows card answers *"Iterator not found"* and
  `c_tranger_view.js:3641` never re-arms.
- **M20 -- `open-iterator backward=1` does nothing** (`c_tranger.c:2437`): the
  direction belongs to `get-page`. The docs list it as honoured and
  gui_treedb's "newest first" checkbox sends only that
  (`c_tranger_view.js:3436`).
- **M21 -- a stateful `open-list` is outside the session reaper**
  (`c_tranger.c:1638`): no `src_gobj`, no `watch_owner()`, and
  `reap_handles_of()` never walks `priv->lists` -- and such a list collects
  every append in memory after its client is gone. It is the open half of
  #2 in the realtime-feed section below.
- **M22 -- `rkey=.*` opens one iterator per key and keeps them all**
  (`c_tranger.c:2270`, 7b3fba479): O(N^2) through the linear scan of
  `tranger2_get_iterator_by_id`, memory by keys x files (1000 keys x 60 daily
  files = +147 MB). High from about 12k keys. gui_treedb restores the card by
  itself on every visit.

**Medium -- gobj-ui, gui_treedb, gui_agent**

- **M24 -- A10 of the first review is incomplete**
  (`c_yui_treedb_topic_with_form.js:3886`): `ac_edition_mode` still
  dereferences the New and Delete buttons unguarded.
- **M25 -- a REFUSED Save still closes the form and throws the edit away**
  (`:4191`); the README and a comment say it stays open. The range multiplied
  the refusals (pkey2, authz, snapshot, cycle).
- **M26 -- A8 is incomplete** (`treedb_write_plan.js:112`): a `writable` time
  column still loses its seconds on every Save of any other field
  (`datetime-local` without seconds, and `get_form_values()` reads every
  field). The column class was fixed, not the cause.
- **M27 -- `EV_REQUEST_JSON`, a new output event on by default, breaks the
  in-repo demo host** (`test-app/src/c_demo_treedb.js:438`, 20759fc): *"Event
  NOT DEFINED in state"*. It does not declare `EV_UPDATE_FIELD` either. Private
  hosts not checked.
- **M28 -- +New with an id that exists is a silent upsert**
  (`c_yui_treedb_topics.js:2503`): it overwrites and, through autolink with the
  empty selects, UNLINKS the existing record.
- **M29 -- `max_col_width` is a HARD ceiling** (`:1485`, 9111ec1): Tabulator's
  `maxWidth`, so the reader cannot widen the column, although the attr, the
  comment and the CHANGELOG say so.
- **M30 -- the row search skips a hook's COUNT but not its children as node
  events deliver them** (`yui_row_search.js:84`): `{id, topic_name}` fails
  `is_fkey_ref()`, so `topic_name` is a wildcard. The test uses a fixture the
  backend never sends.
- **M31 -- string cells go in through `innerHTML`** (`:2093`, predates the
  range): text with `<` is mangled. The plugin's CSP keeps script from running.
- **M32 -- 7.23.186: a `__graphs__` echo re-takes the snapshot from the view's
  LIVE objects** (`c_g6_nodes_tree.js:3063`), so what is unsaved in OTHER
  topics counts as saved and the next Save skips it.
- **M33 -- `ac_node_updated` reads an UNDECLARED `graph` inside a try/catch**
  (`c_g6_nodes_tree.js:10633`): `update_topic_node()` is dead code and a card
  on screen keeps its old record after any UPDATED.
- **M35 -- "a replica opens without its write buttons" (gui_treedb 0.17.36,
  9cdd16b) does nothing** (`c_treedb_links.js:697`, `:765`): `treedb-info` is
  sent without `__md_command__`, the only thing `C_IEVENT_CLI` echoes back, so
  the answer cannot be matched to a service and `master` is never stored.
- **M36 -- gui_agent offers Edit and Apply of a schema (kill, run, play) on
  yunos that IMPOSE the C schema** (`c_agent_treedb.js:1036`): every consumer
  passes `impose_c_schema=1`, the edit can never apply, and no screen says so.

**Medium -- docs and tests**

- **M37 -- the rollback recipe tells the reader to do the opposite**
  (`deploying-yunos.md:376`, `YUNO_LIFECYCLE.md` 6.6): *"when you decide to
  stay on the old one, remove the pin: deactivate-snap"* -- `deactivate-snap`
  re-promotes the NEW release and bounces the node onto it. Fix this one first.
- **M38 -- `treedb_delete_instance()` is documented three contradictory ways**
  (`YUNO_TREEDB.md:391`, the header, the API page): the code tombstones every
  md2 row, never looks at links, and borrows the node.
- **M39 -- the public header `tr_treedb.h:335`, four docs and the comment of
  `delete_node()` (`tr_treedb.c:6448`) still say a save inherits the snap
  tag**, or date the fix to 7.22.0 (08ba69dcb ships in 7.23.0, 12e0c762d in
  7.24.0).
- **M41 -- `test_c_node_authz` is a hand-written list, not the command table**:
  create-node, delete-node, import-assets, gc-assets, set-link-events and
  `schema-file` have no refusal test, and `schema-file` (17cde8a9a) has no test
  at all.
- **M42 -- no test opens a treedb as a REPLICA to write**; and the kwid fix of
  gobj-js 7.21.0 stays green with the fix reverted (`tests/kwid.test.js:106`).

**Low, worth keeping** (three mediums were lowered by their verifiers and live
here; their ids, M23, M34 and M40, stay unused so the others keep theirs):
the `EV_TREEDB_NODE_*` feed is outside the `read` permission, because the subscription authz is commented out
(`c_ievent_srv.c:1373`, `gobj.c:8754`); a refused `__graphs__` write is
recorded as saved and never retried (`c_g6_nodes_tree.js:3282`);
`tranger2_write_topic_var()` answers 0 whatever
`save_json_to_file()` did; refs `topic^id^hook` are written with `snprintf`
into `char[NAME_MAX]` and truncate silently; hook membership is tested by bare
id, so two children of different topics with one id collide; an id holding `^`
is accepted and makes every ref to the node undecodable; JS `kw_get_str()`
stringifies its default (`0` becomes the truthy `"0"`); `cmd_treedbs` /
`cmd_links` / `cmd_hooks` still pair `json_incref(kw)` with the wrong decref;
`treedb_activate_snap()` returns the PREVIOUS snap's tag; the warning *"Parent
ref already in child fkey"* still fires in the legitimate case of 4e4dcdc00,
once per `create-yuno`; `schema-file` is missing from `api/gclass/data.md`;
`CLAUDE.md`'s link rule under "Persistence Rules" is incomplete since
98e69ab8a / 4e4dcdc00, and it names `YUNETA_VERSION` 7.23.0.

## ESP32: `gobj_post_event()` is not in the port

`kernel/c/root-esp32/components/esp_gobj/` carries its own copy of the gobj
sources, so the call added to `kernel/c/gobj-c/` does not reach it. A gclass
that uses it does not build for ESP32.

Porting it is two pieces: the queue and its API in the copy of `gobj.c`, and
the delivery point in whatever drives the ESP-IDF side, which is where the
decision is. On Linux `yev_loop_run()` owns the cycle and the drain goes at
the top of it; the ESP32 port has no equivalent single loop to hang it on.

Until then, a deferral there stays a `C_TIMER0`, and the two implementations
of the same framework differ on a documented call. Nothing uses it on ESP32
today, which is why this is a note and not a blocker.

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

**Done (2026-09-16): the REPORT half.** `tools/agent/audit-agents.sh` says
whether both agents run the binaries on disk — read-only, exit 2 if one is not
running, 1 if one is stale, 0 otherwise, so it drops into a cron unchanged. It
ships in the packages (it is `tools/`, not `scripts/`: it has to run on a bare
installed node). Documented in `tools/README.md`; checked on the dev node and on
wattyzer / yunovatios-central / yunovatios-controlador, all four clean.

Worth knowing, found writing it: **the one-line test is exact, not a
heuristic.** Linux refuses to write into a binary that is being executed
(`ETXTBSY`), so `cp` over a running agent fails outright and `install` / `mv` /
a package succeed only by UNLINKING first — which leaves the running process
holding an inode with no name. Every replacement that can happen while the
agent runs is one the kernel marks ` (deleted)`, so there is no in-place
overwrite case to miss.

What is left is the AUTOMATION: the `yunetas` CLI restarting the agents itself
after a build that replaced `/yuneta/agent/*`, main first and the spare only
once the main one is confirmed healthy.

## gui_treedb: leftovers from the 2026-07-13 audit

The four gclasses whose runtime lived outside the automaton are done
(`C_TRANGER_VIEW`, `C_TREEDB_CONFIG`, `C_TREEDB_LOGIN`; `C_TREEDB_LINKS` was
already fine), the raw `setTimeout`s and the row actions no keyboard could
reach (gui_treedb **0.17.35**), and `treedb-info` — a replica now opens without
its write buttons, asked once by the connection's DISCOVERY and not at the
mount, because the library reads `readonly` once when it draws a topic's
toolbar (**0.17.36**; both 2026-09-16, see that repo's `CHANGELOG.md`).

What the audit left open is one item, and it is a feature project rather than a
leftover:

- **Backend features with no UI** (the SPA uses 15 of ~45 C_NODE/C_TRANGER
  commands). Highest operator value, in order: **snapshots** (`snaps`,
  `snap-content`, `shoot-snap`, `activate-snap`, `deactivate-snap` — tag a
  version, browse it read-only, roll back: all there, zero UI); **backup /
  restore** (`export-db` / `import-db`, a base64 payload maps straight to a
  browser download / file input); `backward` on
  `open-iterator` (newest-first paging); and the relationship inspector
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

Event-level authz (`EVF_AUTHZ_INJECT` / `EVF_AUTHZ_SUBSCRIBE`) is still
**declared but not enforced** — no gate exists for `gobj_send_event` /
`gobj_subscribe_event`.

## Security: ytls TLS posture — per-gate rollout

Remaining is **per-gate deployment config** (validate on staging):

- raise high-level gates explicitly where wanted; set the IoT-compat profile
  (`ssl_min_version` + `ssl_ciphers "@SECLEVEL=0"`, OpenSSL backend) on legacy
  gates;
- turn on peer verification per high-level gate (`ssl_trusted_certificate` or
  `ssl_use_system_ca`); IoT gates opt out with `ssl_allow_insecure_client=true`.
  **Done in 7.6.0:** TLS *clients* now fail closed — a no-CA client is
  *refused* at ctx/state build time (not just logged), and the `C_AUTH_BFF`
  `crypto` / `c_idp_keycloak` `kc_crypto` IdP clients default to a verifying
  posture.
  Remaining is the per-gate **deployment** config: set the CA (or the explicit
  `ssl_allow_insecure_client` opt-out) on each client crypto block in the realm
  config, and raise the server-side gates.

## Security: `denied_ips` is never consulted at accept

`c_yuno` keeps the two lists — `allowed_ips` and `denied_ips` (`SDF_PERSIST`,
with their `list-`/`add-`/`remove-` commands) — and exports both readers,
`is_ip_allowed()` and `is_ip_denied()`. But the accept path
(`c_tcp_s.c`, `yev_callback` on `fd_clisrv`) asks only **`is_ip_allowed`**, and
only when the gate carries `only_allowed_ips`. So:

- **the allow-list works** (whitelist mode, loopback exempted);
- **the deny-list does not stop a connection.** `is_ip_denied()` has exactly
  one caller, `c_authz.c:802`, so a denied ip is refused at *authentication* —
  which means an unauthenticated gate (an IoT/mqtt field port) accepts it,
  builds its channel tree, and lets the protocol run.

Found while banning an internet scanner off a yunovatios mqtt gate: adding the
scanner's ip to `denied_ips` would have changed nothing on that port, so the
ban had to be keyed by the identity the peer announces instead (project-side,
`gate_energia.denied_clients`).

Open decisions before wiring `is_ip_denied()` into the accept path:

- **cost per accept**: it is a json dict lookup per connection, the same one
  `is_ip_allowed` already pays, so only on the gates that have a list;
- **whether the check belongs in `c_tcp_s` or in `c_iogate`** — dropping at
  accept never spends a channel of the pool, which is the point;
- **`only_allowed_ips` is a badly named door**: it gates the whole ip check,
  so a gate that wants a deny-list today has to turn on whitelist mode. The
  deny-list should apply unconditionally when non-empty.

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

- **Backport `jwks_*` keyring NULL-safety** (`jwks_item_get(NULL)` /
  `jwks_free(NULL)`) at the next re-vendor — the vendored v3.2.1+2 copy derefs
  `jwk_set->head` (`jwks.c:201`). Low-sev: not reachable from `c_authz` (keyring
  always valid); the regression test documents and skips it.
- **Periodic re-vendor from upstream** — procedure in
  `kernel/c/libjwt/README.md` (§ Re-vendor procedure).

## Agent: deploy UX — find-new-yunos preview

- **`find-new-yunos` preview lists rows that are already registered.**
  `cmd_find_new_yunos` (`yunos/c/yuno_agent/src/c_agent.c`) iterates **every**
  yuno row and emits a `create-yuno …` line whenever a newer binary/config
  version exists for that role. On a **resumed upgrade** (a prior run already
  ran `find-new-yunos create=1`, so the new-version rows exist, but never
  promoted them) the OLD primary rows survive and still match, so the preview
  re-lists all of them as "would be created". `create=1` then fails per row with
  "Yuno already exists". Harmless now — the CLI 0.11.1 fall-through treats that
  as idempotent and proceeds to `deactivate-snap` — but the preview is
  misleading. **Fix:** skip a row in the preview when a yuno instance at the
  target (`yuno_role`, `yuno_name`, new `role_version`/`name_version`) already
  exists (the same `gobj_list_nodes` check `create-yuno` does at its
  "already exists" guard). Consolidated project — read in depth, preserve the
  `create=1` semantics, before touching.

- **`create-yuno` / `delete-yuno`: the confusion is the DEFAULT, not the name.**
  *(Filed 2026-07-26 as a rename proposal; corrected 2026-07-27 after reading
  the implementations — the original entry was wrong and is kept here only as
  the reasoning trail.)*

  Both commands are in fact symmetric, and both are correctly named: they
  operate on a yuno **or** on one of its releases, and the discriminator is a
  parameter.

  - `cmd_create_yuno` takes `role_version` (binary) + `name_version` (config).
    With no matching row it creates the yuno; with a newer pair it registers a
    new **release** of an existing one.
  - `cmd_delete_yuno` keys on `yuno_release` (the pkey2): given it, that exact
    release is deleted; **omitted, the in-memory primary — the yuno itself —
    goes.**

  So the trap is not the verb, it is that the *destructive* reading of
  `delete-yuno` is what you get by **omitting** a parameter. Working on
  `yunovatios` (2026-07-26) a config version bump was "adopted" with
  `delete-config` + `delete-yuno` + `create-yuno` instead of the real flow
  (`find-new-yunos create=1` + `deactivate-snap`, bundled as
  `yunetas upgrade-yunos`); the bare `delete-yuno` took the primary, cascaded
  onto the config row, and left the realm **without its `auth_bff`** until both
  were recreated from the repo.

  **Done (2026-07-27):** the three help lines now name the discriminator
  (`"Delete a release (yuno_release=...) or the WHOLE yuno (without it)"` and
  friends), which is what `ycommand -c 'help delete-yuno'` shows.

  **Done (2026-07-27):** `delete-yuno` now requires `whole=1` to delete the
  yuno and all of its releases; the bare form is refused with both options
  named. `force=1` still means only "bypass the snap-tag guard", and its help
  line says so.

## Observability: source-IP attribution in decoder logs — remaining pass

The `peername` roll-out across the protocol/decoder error logs shipped
2026-06-21 (kernel + hidraulia + estadodelaire); its record is `CHANGELOG.md`
and git history. What was intentionally skipped and is still open:

- Done and in `CHANGELOG.md` 7.22.0: `c_prot_mqtt2.c` (its 137 protocol
  warnings carry `peername`; its 71 `gobj_log_error` were left alone on
  purpose) and `c_prot_http_cl.c` (nothing to migrate: its logs are ours,
  the one about a request carries the `url`). The scope rule for what is
  left is `CLAUDE.md`'s decoder-severity question, *"could a remote peer
  trigger this with bad bytes?"* — an internal invariant is not the peer's
  doing, and a field naming a peer for a fault that is ours reads as an
  accusation.

- **Still open: wattyzer `C_GATE_PVPC`** — an outbound client too, and so
  likely the same answer as `c_prot_http_cl.c` (nothing a peer can trigger; the
  url or the endpoint name is the useful field). Unlooked-at. It lives in the
  wattyzer repo, not here.

- **Out of scope, do not migrate:** `C_PROT_MQTT` (`modules/c/mqtt`) is
  deprecated but still in Hidraulia production.

The canonical read pattern is the one in `c_websocket.c` / `c_prot_mqtt2.c`:
read `peername` off the bottom gobj once, in the cold error branch.

## C_TRANGER: realtime feed (Live cards) — inotify scalability + leak

Context: `open-rt`/`close-rt` + `EV_TRANGER_RECORD_ADDED` (public) power
gui_treedb's Live records card. On a **non-master (reader)** C_TRANGER —
e.g. `db_history_wz`, `master:false` — each `open-rt` opens a
`tranger2_open_rt_disk` feed = **one inotify instance**. Two problems surface
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

**#2 — Tie the feed to the ievent session.** Shipped on 2026-09-16 for
`open-rt` and `open-iterator` (see `CHANGELOG.md`). Two pieces are still open,
both from the 2026-09-21 review above: a stateful `open-list` is neither
stamped with `src_gobj` nor reaped (M21), and `mt_subscription_deleted` still
reads a session's LAST unsubscribe as its death, so closing the last Live card
closes that session's paging iterators too (M19).

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

