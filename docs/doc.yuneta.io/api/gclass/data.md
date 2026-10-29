# Data GClasses

Time-series, graph database, and resource persistence.

**Source:** `kernel/c/root-linux/src/c_tranger.c`, `c_treedb.c`,
`c_node.c`, `c_resource2.c`

---

(gclass-c-tranger)=
## C_TRANGER

Time-range database manager — wraps **timeranger2** for CRUD operations
on time-series topics.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |

**`master` answers what the tranger IS.** It is configured (`SDF_RD`) and read
at create, but read afterwards it answers the tranger's own flag: a master that
lost its lock while stopped (another process took the store) is a replica, and
`gobj_read_bool_attr(gobj_tranger, "master")` says `FALSE` (new after 7.25.4;
7.25.4 answered the configuration). `open-rt` and `open-list` follow it: a
replica's feed is an `rt_disk`, which the other master's appends reach through
the disk.
The flag moves when the tranger REVIVES, which happens at the first write or the
first topic open after `tranger2_stop()`. Between the stop and that moment it
still says what it was, so an `open-list` on a topic that is already open can
still get an `rt_mem` feed. Read `master` again after the restart has opened its
topics.

**`archive_directory`** (`string`, `SDF_RD`, default empty) is where
`archive-key-files` keeps the old files it takes out of a topic, a batch per
key and purge ([`tranger2_archive_key_files()`](#tranger2_archive_key_files)):
an absolute path of a directory that exists, on the filesystem of the store,
neither inside the tranger's directory nor holding it. It is configuration, and
never a parameter of the command: a peer that chose it could have the files of
the store linked wherever the yuno can write. A yuno that archives sets it in
its config of the service:

```json
"kw": {
    "path": "/yuneta/store/raw_tracks/yunovatios",
    "archive_directory": "/yuneta/store/archive"
}
```

### Commands

| Command | Description |
|---------|-------------|
| `topics` | List all topics: their names, or with `expanded=1` a desc each (`{topic_name, system_flag, pkey, tkey, topic_version}`). `system_flag` is what tells whether the topic's `t`/`tm` are seconds or **milliseconds** (`sf_t_ms` / `sf_tm_ms`). |
| `create-topic` | Create a topic, or answer the one that is there (the command is idempotent): `topic_name`, `pkey` (default `id`), `tkey` (default `tm`), `system_flag` (default `sf_string_key`; names joined by `\|`), `jn_cols` and `jn_var` (json objects; `null` is none). Permission `create`. A NEW topic is created by the master only: a replica answers `-1` *"\<role^name>: tranger '\<name>' is READ-ONLY, this yuno is not its master: create-topic of a new topic runs on the master"*. A `jn_var` with a higher `topic_version` changes the version of a topic that is there: the tranger writes its `topic_cols.json` from `jn_cols` and its `topic_var.json` again. That change is refused for a system topic, for a topic of a treedb or a msg2db open on the tranger (its schema is theirs: `save-schema` + `apply-schema`), and without `jn_cols`; a topic with no columns by design sends `jn_cols={}`: `command-yuno id=<id> service=<tranger> command=create-topic topic_name=pp jn_cols='{"id":"","tm":0}' jn_var='{"topic_version":2}'`. What is refused, and what only a peer is refused, is in [*What a command is handed*](#c-tranger-peer-data). |
| `open-topic` | Open an existing topic. A topic that is not there answers `-1` *"\<role^name>: Topic not found: '\<topic>'"*. |
| `delete-topic` | Delete a topic. **Irrecoverable and master-only**: a replica answers `-1` *"\<role^name>: tranger '\<name>' is READ-ONLY, this yuno is not its master: delete-topic runs on the master"*, with no log (up to 7.26.9 an ERROR *"Only master can delete"*). A topic that still holds records needs `force=1`, and the refusal says so: `command-yuno id=<id> service=<tranger> command=delete-topic topic_name=frames force=1`. Before 7.21.0 the guard never fired and a topic with records was deleted on the first call. A system topic is refused (`force` does not lift it), and so is, from a peer, a topic of a treedb or a msg2db open on the tranger (see [*What a command is handed*](#c-tranger-peer-data)). |
| `delete-key` | Delete a whole key (primary key) of a topic and every record it holds. **Irrecoverable and master-only** (a replica answers `-1` *"... READ-ONLY ... delete-key runs on the master"*, with no log; up to 7.26.9 an ERROR); the delete propagates to the in-process subscribers and to the `rt_by_disk` followers. A key that still holds records needs `force=1` — the refusal names the record count. A key that is not there is an error, not a silent success. From a peer, a key of a topic of a treedb or a msg2db open on the tranger is refused: delete the node through the treedb (`delete-node`). |
| `delete-key-files` | Retention by hand: delete the OLD FILES of a key, the ones whose records were all appended before `before`, and keep every `g_rowid` where it was ([`tranger2_delete_key_files()`](#tranger2_delete_key_files)). `before` is an integer in the topic's unit of `t`, or a date; without `key` every key of the topic is purged, in this one call (about 1-2 ms per key). It answers `{rows, keys_purged, keys, keys_failed}`: `command-yuno id=<id> service=<tranger> command=delete-key-files topic_name=raw_tracks key=dev1 before=2026-09-21T00:00:00` and `... command=delete-key-files topic_name=raw_tracks before=1790035200` (every key). **Irrecoverable and master-only**, permission `delete`, like `delete-key`; the followers hear it. Refused before the tranger (see [*What a command is handed*](#c-tranger-peer-data)): a `before` that is not in the past, or a text that is no date (read as now, it would cut everything before today); a topic whose `filename_mask` gives ids that do not sort as time (a queue, `%d-%m-%Y`, `%Y-%b`); a topic of a treedb or a msg2db, from ANY caller (its keys are its nodes). A key not there answers `-1` *"\<role^name>: Key not found in topic '\<topic>': '\<key>'"* with no log, as `delete-key` does: it is no data of a wrong shape. After it, `list-keys` gives the key its `purged`. The pages of an open iterator keep their positions across a purge (the ones of the rows purged come back empty); a multi-key iterator with a FILTERED part whose key was purged since its open is closed at its next `get-page`, which answers `-1` *"iterator 'it1' closed, old rows of its key 'K' were purged since it opened: open it again"*. On a REPLICA that had not counted some rows of the files purged (a md2 file it could not read), the purge moves its numbering of the key to the master's (the WARNING *"The numbering of the key moved to the master's"*; `renumbered` in [`tranger2_topic_key_range()`](#tranger2_topic_key_range)): an iterator with absolute rowid bounds pages the SAME rows after it, under their new g_rowids -- one opened with `from_rowid=3 to_rowid=4` over `Qc1:3 Qc2:4` pages `Qc1:5 Qc2:6`, a multi-key one too (its parts are opened with the bounds moved; before this fix they paged `Qb1:3 Qb2:4`, other rows, with no error). A negative bound is counted from the end of the key at each open of a part. |
| `archive-key-files` | As `delete-key-files` (the same parameters, cut, refusals, permission `delete`, master-only, and the same answer `{rows, keys_purged, keys, keys_failed}`), and the files taken out of the topic are KEPT: each purge of a key hard-links them into a batch `<archive_directory>/<database>/<topic>/<key>/<upto>@<t>/` with the schema of the topic and a `manifest.json`, for another process to send or read and then remove ([`tranger2_archive_key_files()`](#tranger2_archive_key_files)). The directory is the `archive_directory` attribute: with it empty the command answers `-1` *"\<role^name>: tranger '\<name>' has no archive_directory configured: archive-key-files keeps the files there"*, with no log. `command-yuno id=<id> service=<tranger> command=archive-key-files topic_name=raw_tracks key=dev1 before=2026-09-21T00:00:00` answers `0` *"\<role^name>: topic 'raw_tracks': 1440 rows archived from 1 keys of 1"*. About three times the cost of a delete per key. The archive frees no disk until its reader removes a batch. |
| `open-list` / `close-list` | Open or close a record list (one-shot snapshot with `return_data=1`, else a live list collecting realtime appends). A live list opened by a remote session is that session's, like its iterators: it is closed when the session closes, and survives the session closing its last Live card. A **keyless** list accepts `rkey` (PCRE2 regex over the keys), and it governs both the disk load **and** the realtime feed. A list with no realtime feed (`to_rowid` set) is not its topic's, so it outlives it: `close-list` frees it after its topic closed, and a `delete-topic` frees the ones of that topic (after 7.25.3 they leaked, 10 KB each). The time bounds `from_t`, `to_t`, `from_tm` and `to_tm` are `DTP_JSON` (they were `DTP_STRING`, and an integer from a kw was refused): an integer in the topic's unit (negative: from the last record), or a string -- a date when it has a `T`, a number otherwise. `open-list list_id=l1 topic_name=pp return_data=1 from_t=2026-01-01T00:00:00` (a date) and `... from_t=1767225600` (a number) are both taken; any other type answers `-1` *"\<role^name>: from_t must be an integer or a date string"*. The iterators of a one-shot list are opened under a creator of their own (`<service>^oneshot`): a `list_id` that is the name of the service no longer collides with an iterator open on that key (*"Iterator already exists"*, and the key was left out of the answer). |
| `get-list-data` | Retrieve an open list's data. |
| `list-keys` | List a topic's keys with their record counts **and their time span on both axes**: `[{key, records, fr_t, to_t, fr_tm, to_tm}]`, plus `purged` (the rows `delete-key-files` took; the key's `g_rowid`s start after them) for a key that has it. Lets a client bound a time picker to what the key really holds without reading a record. Filters, sorts and pages **in the server**: `rkey` (PCRE2 regex), `order=key\|records` + `desc`, and `from`/`limit` (with `limit>0` the answer is a page `{total_rows, pages, data}`, and `limit=0` keeps the plain full list). |
| `open-iterator` / `close-iterator` | Open/close a stateful per-key iterator (row index only, no upfront load) for cursor pagination. The handles a remote session opens are stamped with it and reaped when the session closes, whether or not it subscribed to anything (the service watches the session's `EV_ON_CLOSE`, once per session). The handles of a command relayed by the agent (`command-yuno`) are not reaped: they stay until closed (see *A handle is its opener's* below). Takes the match conditions below. A filtered iterator indexes the matching rows at open, so `total_rows` and the pages count only those. Give `key` for one key, or `rkey` (PCRE2 regex) for several keys: see *Several keys in one iterator* below. |
| `get-page` | Get a page `{total_rows, pages, data}` from an open iterator (`limit`, optional `backward`). `from_rowid` is 1-based and, on a **filtered** iterator, is a position among the MATCHING rows (a global rowid only when the iterator does not filter). **`backward` counts from the END and returns the newest rows first**, for every kind of iterator — one key filtered or not, several keys: `get-page iterator_id=it1 from_rowid=1 limit=100 backward=1` is the newest 100. A `get-page` that does not say takes the direction the iterator was OPENED with (`open-iterator … backward=1`). Before 7.25.0 an unfiltered one-key iterator kept the window counted from the start and only reversed it, and `open-iterator backward=1` did nothing. An iterator whose key was deleted since it opened (by `delete-key`, or by a treedb sharing the tranger) is closed at its next `get-page`, which answers `-1` with *"iterator 'it1' closed, its key 'D' was deleted: open it again"*; asked again, it is *"Iterator not found"*. A multi-key iterator closes when ANY of its keys goes. |
| `open-rt` / `close-rt` | Open/close a realtime feed on a topic key (no history load). New appends are published as `EV_TRANGER_RECORD_ADDED` to subscribers. An `rt_id` longer than `NAME_MAX` (255 bytes) is refused with `-1` (a replica names a directory after it), and so is any feed the tranger cannot open: `open-rt` answers `-1` and names the feed, it never answers "opened" for a feed that does not deliver. |
| `add-record` | Append a record to a topic: `tranger2_append_record()`. **Master-only** (a replica answers `-1` *"... is READ-ONLY, this yuno is not its master"* before the library is called); permission `write`. `record` is a dict with the topic's `pkey` (and its `tkey`, when the topic has one), or its json text; `__t__` is the time of the record in the topic's unit (`0`: now), `user_flag` its user flag (`0`). Both are `DTP_INTEGER`: the text of `__t__=1758772800` is read as a number by the command parser (since 7.27.0), and a value of another shape (`{"__t__": {}}`) is refused before the handler, `-1` *"\<gobj>: command 'add-record', parameter '__t__' must be an integer, not an object"* (see [`command_parser()`](#command_parser)). `command-yuno id=<id> service=<tranger> command=add-record topic_name=pp record='{"id":"1","tm":1700000000}'` answers `0: <role^name>: record added to topic 'pp', rowid 4`, with `data: {"topic_name": "pp", "rowid": 4, "t": 1758772800, "tm": 1700000000}` (`rowid` is the rowid of the key, the one `get-page` counts). A topic that is not there answers `-1` *"\<role^name>: Topic not found: '\<topic>'"*, and an append that fails `-1` *"\<role^name>: cannot add the record to topic 'pp' (see the log)"*. A record that is not a dict, nor the json text of one, `add-record topic_name=pp record='[1]'`, answers `-1` *"\<role^name>: What record? It must be a dict with the pkey of topic 'pp'"*, and is logged too: the WARNING *"record of a wrong shape, refused"* with `why` *"the record is not a json object"*. Up to 7.25.4 it was a stub that always answered `-1` *"Pending to review"* and logged an ERROR with a stack trace. The record is checked before the append: its pkey is what the key type of the topic takes, and `__t__` and the time of its tkey fit the 44 bits of an md2 row ([`TRANGER2_MAX_TIME`](#timeranger2-max-time)). The time of the tkey is the one the append keeps, asked with [`tranger2_record_tkey_time()`](<#tranger2_record_tkey_time>): an integer as it is, a string read as a date (`approxidate()`, times 1000 on an `sf_tm_ms` topic), so the date `"@99999999999999 +0000"` in the tkey is refused like the integer `17592186044416` there: `add-record topic_name=pp record='{"id":"1","tm":"@99999999999999 +0000"}'` answers `-1` *"\<role^name>: record refused by topic 'pp': a time in its tkey that a record cannot keep (negative, or past 44 bits)"*. The same past the 44 bits in `__t__`, `add-record topic_name=pp __t__=17592186044416 record='{"id":"1","tm":1700000000}'`, answers `-1` *"\<role^name>: record refused by topic 'pp': a __t__ larger than the time a record keeps (44 bits)"*. A pkey that is not one directory component, `add-record topic_name=pp record='{"id":"../x","tm":1700000000}'`, answers `-1` *"\<role^name>: record refused by topic 'pp': a pkey with a '/' or a leading '.'"*. A record whose `__t__` falls in a purged period of its key ([`tranger2_record_in_purged_period()`](#tranger2_record_in_purged_period): its old files were deleted by `delete-key-files`), `add-record topic_name=raw_tracks __t__=1790035200 record='{"id":"dev1","tm":1790035200}'`, answers `-1` *"\<role^name>: record refused by topic 'raw_tracks': a __t__ in a purged period of its key: its old files were deleted"*, with the WARNING *"record of a purged period of its key, refused"* (an ERROR from C code); up to 7.27.6 the tranger refused it with an ERROR and the peer read *"see the log"*. A key flagged `unlisted` (its `.purged` could not be read when the topic opened) is asked from the base on disk, as the append reads it. Every other refusal of the record, the *"What record?"* one included, logs the WARNING *"record of a wrong shape, refused"* with its `why` (an ERROR from C code). From a peer a topic of a treedb or a msg2db open on the tranger is refused: write it through the treedb (`create-node`, `update-node`). See [*What a command is handed*](#c-tranger-peer-data). |
| `print-tranger` | Dump tranger state as bounded JSON (`expanded`, `lists_limit` and `dicts_limit`. Unexpanded containers answer as `[[size]]`). `path` (a string, declared since 7.27.0: a number was an ERROR) drills into one subtree, segments joined by `` ` ``, a list indexed by number. A value that is not a dict or a list at the end of the path is answered as it is: `print-tranger path=directory` answers `data: "/yuneta/store/..."` (up to 7.26.9 an ERROR of `kw_collapse()`). A path that names nothing answers `-1` *"\<role^name>: path not found: '\<path>'"*, and logs nothing: a key that is not there, a segment under a value that is not a dict or a list (`` path=directory`x ``), whatever its length or its number of segments ([`kw_find_path()`](<#kw_find_path>) walks it in a loop; up to 7.26.9 a segment longer than 255 bytes or more than 64 segments were refused with a WARNING *"print-tranger path refused"*). Permission `read`. |
| `check-json` | Check the refcounts of the tranger's json, and ANSWER what it finds ([`json_refcounts_report()`](<#json_refcounts_report>)): `data` holds the counts -- `jsons` walked, `corrupted`, `over_max`, `truncated` -- and the first 20 with their path, and the answer is `0` *"\<role^name>: check refcounts of tranger: 0 corrupted"*, or `-1` for a FAULT: a corrupted json, or a walk that stopped short with no memory left (`truncated`; the comment says so). The jsons held more than `max_refcount` times are the answer to what was asked, a `0` (up to 7.26.9 they answered `-1`, so a sound treedb asked with `max_refcount=1` read as an error). By default (`max_refcount=0`) it looks only for CORRUPTED jsons (a refcount of 0), which are also logged, an ERROR (our own invariant is broken); `max_refcount=N` also reports the jsons held more than N times, in the answer only. Permission `read`, as `print-tranger`. A tranger with treedbs holds each node in more than one place (its indexes, its parents' hooks), so ask with the count it is expected to have: `command-yuno id=<id> service=<tranger> command=check-json max_refcount=2`. Up to 7.26.9 it asked no permission, its default was `1`, and every json held more often was an ERROR in the log, with a trace -- every node of a treedb, for any peer. |
| `desc` | Describe topic schema. A topic that is not there answers `-1` *"\<role^name>: Topic not found: '\<topic>'"*. |

**Who gets the realtime feed.** `EV_TRANGER_RECORD_ADDED` is published to
every subscriber: a remote one keeps to its own feed with a `__filter__` on its
`rt_id`. The appends a LIVE list pushes (`open-list` without `return_data`)
carry the `list_id` as their `rt_id`, the same way: `__filter__: {rt_id:
"pepe"}` takes the pushes of list `pepe`. Up to 7.25.4 they carried no
`rt_id`: a filtered subscriber never got them, and an unfiltered one got every
append once per live list of the topic. Since 7.25.5 the event is `EVF_AUTHZ_SUBSCRIBE`, and `read` carries the
`__subscribe_event__` alias. So when the yuno sets `enable_subscription_authz`,
a remote subscription to the feed needs `read` on the tranger's service, the
permission that `open-rt` asks. Up to 7.25.4 a user without `read` could not
open a feed, but could subscribe to the event with no filter and get every
record of the feeds that other users opened. See
[`YUNO_AUTH.md`](../../../../yunos/c/yuno_agent/YUNO_AUTH.md) §4.6.

```bash
# the feed, and the subscription of the same session to its own records
command-yuno id=1911 service=tranger command=open-rt rt_id=rt1 topic_name=pp key=1
```

```js
gobj_subscribe_event(gobj_remote_tranger, "EV_TRANGER_RECORD_ADDED", {
    __filter__: {rt_id: "rt1"}
}, gobj);
```

**A handle is its opener's: the channel it was opened by, and the user.** The
feeds, iterators and live lists that a remote session opens are that
session's. `close-rt`, `close-iterator`, `close-list`, `get-page` and
`get-list-data` from ANOTHER session answer `-403` *"\<role^name>: iterator
'it1' is not yours: another session opened it"*, and log a warning, *"Handle of
another session, refused"* (with the `src` and the `user`); the handle is left
as it was. A gobj of the yuno itself (a local caller) is not refused. In
7.25.4 any session could close or read any handle by its id, and
`print-tranger` shows the ids to every `read` user.

What "session" means depends on how the command arrives:

| The command comes | It comes by | The owner is |
|---|---|---|
| from a client connected to the yuno itself (a GUI, `ycommand` on the yuno's own port) | its own `C_IEVENT_SRV`, one per connection | that connection (and its user) |
| relayed by the agent (`command-yuno`) | the yuno's ONE link to the agent, a `C_IEVENT_CLI`, whoever the operator is | the user the agent stamped in the command (`__username__`) |

So through the agent another USER is refused, and a relayed command of no user
is refused a handle that a user opened; two sessions of the SAME user through
the agent are one owner, and each can close what the other opened -- the
channel cannot tell them apart. The handles of a relayed command are not
reaped when the operator's session closes (only a direct session is watched,
see `open-iterator`): close them.

```bash
# user alice, through the agent
ycommand -c 'command-yuno id=1911 service=tranger command=open-iterator iterator_id=it1 topic_name=pp key=1'
# user bob, through the agent, the same id
ycommand -c 'command-yuno id=1911 service=tranger command=close-iterator iterator_id=it1'
# -> -403: <role^name>: iterator 'it1' is not yours: another session opened it
```

The same holds, per connection, for two clients connected to the yuno
directly: a GUI that opened `it1` keeps it from another GUI, even one of the
same user.

**`open-iterator` match conditions** (all optional, and `0` or empty means unset):
`from_t`/`to_t`, `from_tm`/`to_tm`, `from_rowid`/`to_rowid`, `backward`, and the
user_flag conditions (`user_flag`, `not_user_flag`, `user_flag_mask_set`,
`user_flag_mask_notset`). They are ANDed, and every one is honored **per
record**.

**Several keys in one iterator.** Give `rkey` instead of `key`. The iterator
lays the matching keys end to end, sorted by key, in the same order `tr2list`
prints a topic. `get-page` positions are positions in that concatenation. Each
record gives its key in `__md_tranger__.key`, because one page can hold more
than one key. The order is **not** a time order: a rowid counts inside one key
only. To see the records by time, sort the page on the client. The match
conditions apply to each key separately. `key` and `rkey` together is an
error.

```
command-yuno id=<id> service=<tranger> command=open-iterator iterator_id=all1 topic_name=binaries rkey=.*
command-yuno id=<id> service=<tranger> command=get-page iterator_id=all1 from_rowid=1 limit=100
command-yuno id=<id> service=<tranger> command=close-iterator iterator_id=all1
```

The answer of `open-iterator` also gives `keys`, the number of keys it found.

**A key deleted under it is a deleted key, born again or not.** A multi-key
iterator holds no tranger2 iterator between pages, so it keeps one realtime
feed over the topic (`only_md`, fed to nobody) for its `key_deleted` callback:
the next `get-page` after a `delete-key` of one of its keys answers -1 *"was
deleted"* and closes the iterator, like a one-key iterator does, even when the
key was created again in between. Open it again to page the reborn key.
**Only in the process that deleted the key** (the master): the callback fires
in the delete itself, so a REPLICA's feed never hears it. There the next
`get-page` reads files that are gone: it fails on the read, logs *"Key gone
from disk while its iterator was open: deleted (by the master?)"*, and the
iterator stays open. Close it and open it again.

**An id is free again the moment its handle is gone.** A handle goes with its
topic (a close and reopen, `delete-topic` + `create-topic`, a stop/start of the
service): the entry is dropped at the stop, after the `delete-topic`, when a
`get-page` answers *"closed with its topic"*, or when `open-iterator` /
`open-rt` / `open-list` meet the id again — so the same id opens a new handle,
with data, instead of *"already open"* and no data.

**The keys watch is not a client's feed.** It is named
`<iterator_id>^__keys__` and opened under a creator of its own, so a client's
`open-rt rt_id=all1^__keys__` is another feed: until after 7.25.3 the two
collided, `open-iterator` answered `-1`, and a `get-page` of a dead iterator
closed the client's feed. It records only the deletes of the keys its iterator
pages.

**What it costs.** The open counts the rows of each key once (for a filtered
iterator that is building and dropping each key's index) and keeps only a
number per key; a `get-page` opens the keys its page touches, reads, and closes
them. Until after 7.24.1 it kept one tranger2 iterator per key for its whole
life — 1000 keys with 60 daily files was +147 MB for one card. A key the
iterator does not filter is counted LIVE at every `get-page`, like a one-key
iterator: a backward page (newest first) shows the rows appended since the open.
A filtered key keeps the count of the rows its filter matched at open (after
7.25.3; before, every key kept the count of the open, so the whole-topic card
never showed a new row): a row appended to it after the open is never paged,
although each page indexes the key again -- counting it would mean indexing
every filtered key on every page. A key CREATED after the open is not in the
iterator: open it again to see it.

**Positions move under appends.** Because an unfiltered key is counted live, a
row appended to a key moves the positions after it by one. Paging forward after
an append to an EARLIER key (or backward after an append to a LATER key), the
next page repeats the last row of the page before; an append never makes a page
skip a row. Every record carries its key and rowid, so a client that must not
show a row twice drops a repeated `(key, rowid)`. For example, with key `I`
holding 3 rows and key `J` 2:

```
command-yuno id=<id> service=<tranger> command=open-iterator iterator_id=ij topic_name=t rkey=^[IJ]$
command-yuno id=<id> service=<tranger> command=get-page iterator_id=ij from_rowid=1 limit=4
#   I#1 I#2 I#3 J#1
#   (a row is appended to I)
command-yuno id=<id> service=<tranger> command=get-page iterator_id=ij from_rowid=5 limit=4
#   J#1 J#2         <- J#1 again
```

Bound the reading of a large topic anyway: a negative `from_rowid` reads the last
N records of EACH key, and is what gui_treedb's whole-topic card starts with:

```
command-yuno id=<id> service=<tranger> command=open-iterator iterator_id=all1 topic_name=binaries rkey=.* from_rowid=-100
```

A tranger record carries **two independent timestamps**, and a browser of raw
records needs both:

| Axis | Meaning |
|------|---------|
| `t`  | **Persistence** time — when the record was appended to the topic. |
| `tm` | **Message** time — when the event it carries happened (the record's `tkey` field, set by the producer). |

They diverge whenever data is backfilled or a device uploads a buffered batch
late. Both are expressed in the **topic's** unit — seconds, unless its
`system_flag` sets `sf_t_ms` / `sf_tm_ms` (ask `topics expanded=1`).

(c-tranger-peer-data)=
### What a command is handed is the caller's data

**Every answer starts with the yuno that answers** (`<role^name>: `), a
`-403` too: `-403` *"\<role^name>: No permission to 'read' in service
'tranger'"* (new after 7.26.9; *"What topic_name?"*, *"What list_id?"* and the
`-403`s named nobody).

**The data a command is handed is checked BEFORE the tranger**, which logs an
ERROR with a stack for it (a CRITICAL for a topic name longer than `NAME_MAX`:
its `mkdir` failed, and the default `on_critical_error` EXITS the yuno). A
refusal answers `-1` with the cause, and its log depends on who sent the
command, as in [`command_parser()`](#command_parser):

| The kw | Who | The refusal is logged as |
|---|---|---|
| carries a `__username__` (`C_IEVENT_SRV` puts it there; the agent stamps the operator in a `command-yuno`) | a peer: the mistake is the peer's | a WARNING, `msgset: MSGSET_PARAMETER`, no stack |
| carries none | C code of the yuno: a broken contract | an ERROR with a stack |

The authz refusal *"Handle of another session, refused"* stays a WARNING for
both. A name an answer repeats is capped at 64 bytes, never in the middle of
a character.

| The command is handed | Commands | Logged | Answer |
|---|---|---|---|
| a topic that is not there (not open, and no directory with a `topic_desc.json`), or a name that breaks the topic rule ([`tranger2_name_is_identifier()`](<#tranger2_name_is_identifier>)) or is longer than `NAME_MAX` | `desc`, `open-topic`, `delete-topic`, `delete-key`, `delete-key-files`, `archive-key-files`, `open-list`, `list-keys`, `open-iterator`, `open-rt`, `add-record` | *"topic not found, refused"* | `Topic not found: '<topic>'` |
| a `key` that is not ONE directory component: a `/`, a leading `.`, or longer than `NAME_MAX` (`../../<topic>/keys/<k>` read another topic's records under the permission asked for this one) | `open-list`, `open-iterator`, `open-rt`, `delete-key-files`, `archive-key-files` | *"key that is not one directory component, refused"* | `key '<key>' is not one directory component` |
| a time bound that is neither an integer nor a string | `open-list` | *"time bound of a wrong type, refused"* | `from_t must be an integer or a date string` |
| a `before` that is not a time in the past: none, in the future, or a text that is no date | `delete-key-files`, `archive-key-files` | *"before that is no time in the past, refused"* | `What before? A time in the past: an integer in the unit of t of topic '<topic>', or a date` |
| a topic whose `filename_mask` gives ids that do not sort as time ([`tranger2_mask_sorts_as_time()`](#tranger2_mask_sorts_as_time): `queue`, `%d-%m-%Y`, `%Y-%b`) | `delete-key-files`, `archive-key-files` | *"topic whose files do not sort as time, refused"* | `the files of topic '<topic>' do not sort as time (filename_mask '<mask>'): its old files cannot be told apart` |
| a topic of a treedb or a msg2db open on the tranger, from ANY caller | `delete-key-files`, `archive-key-files` | *"purge of a topic of a treedb or a msg2db, refused"* | `'<topic>' is a topic of a treedb: it is written through it, not by delete-key-files` |
| an `rkey` that PCRE2 does not compile | `list-keys`, `open-iterator`, and a one-shot `open-list` (`return_data=1`, no `key`) | *"Bad rkey regex"* | `Bad rkey regex '<rkey>' at <offset>: <error>` |
| an `rkey` that PCRE2 does not compile, in a LIVE `open-list` (no `return_data`): timeranger2 compiles it when it opens the list | `open-list` | *"Bad rkey regex"*, by timeranger2: a WARNING for every caller, C code too | `cannot open list '<list_id>' on topic '<topic>' (see the log)` |
| an `rt_id` longer than `NAME_MAX` | `open-rt` | *"rt_id too long"* | `rt_id longer than 255 bytes` |
| a system topic | `delete-topic` | *"system topic, delete refused"* | `'<topic>' is a system topic, it cannot be deleted` |
| a topic of a wrong shape (below) | `create-topic` | *"topic of a wrong shape, create refused"*, with `why` | `cannot create topic '<topic>': <why>` |
| a record that is not a json object (or its json text) | `add-record` | *"record of a wrong shape, refused"* | `What record? It must be a dict with the pkey of topic '<topic>'` |
| a record of a wrong shape (below) | `add-record` | *"record of a wrong shape, refused"*, with `why` | `record refused by topic '<topic>': <why>` |
| a record whose `__t__` falls in a purged period of its key ([`tranger2_record_in_purged_period()`](#tranger2_record_in_purged_period): its old files were deleted by `delete-key-files`) | `add-record` | *"record of a purged period of its key, refused"*, with `why` | `record refused by topic '<topic>': a __t__ in a purged period of its key: its old files were deleted` |
| FROM A PEER ONLY: a write into a topic of a treedb or a msg2db open on the tranger | `add-record`, `delete-key`, `delete-topic` | *"write of a topic of a treedb or a msg2db, refused"* | `'<topic>' is a topic of a treedb or a msg2db: it is written through it, not by <command>` |

A peer writes a treedb's topics through the treedb, never through its
tranger: `C_NODE` validates the record, keeps the immutable seeds and asks its
own permissions. A raw record appended to `users` WAS that node at the next
load (the newest record of a key wins), a `delete-key` took a node behind the
treedb's back, and a `delete-topic` took a topic of a running treedb. A C
caller is its own owner, and its writes are taken. The gui_treedb tranger
view's *delete key* button is refused on a treedb's own topics for this
reason.

```bash
command-yuno id=<id> service=tranger_treedb_x command=add-record topic_name=users record='{"id":"raw"}'
# -> -1 <role^name>: 'users' is a topic of a treedb or a msg2db: it is written through it, not by add-record
command-yuno id=<id> service=tranger command=open-iterator iterator_id=it1 topic_name=pp key=../../other/keys/K
# -> -1 <role^name>: key '../../other/keys/K' is not one directory component
```

**A topic of a wrong shape** (`create-topic`). The `why` of the answer:

| Refused | `why` |
|---|---|
| a name that breaks the topic rule, or is longer than `NAME_MAX` | `` the topic name breaks the name rule (empty, '.', '..', '/' or '`') ``, *"a topic name longer than NAME_MAX"* |
| `jn_cols` or `jn_var` that is not a json object (`null` is none) | *"jn_cols is not a json object"*, *"jn_var is not a json object"* |
| a `topic_version` of `jn_var` that is a dict or a list | *"the topic_version of jn_var is not a number"* |
| a `system_flag` name the tranger does not know (up to 7.26.9 an ERROR, and the topic was created without it) | *"an unknown system_flag name"* |
| a FILE of the database with that name (`__timeranger2__.json`): its `mkdir` was a CRITICAL | *"a file of the database has that name, it cannot be a topic"* |
| more than one key type (`sf_string_key\|sf_int_key`): such a topic refused every append | *"more than one key type in system_flag"* |
| a new topic with no `pkey` that is not keyed by rowid | *"no pkey for a topic that is not keyed by rowid"* |
| a directory that is there and is not a topic (no `topic_desc.json`: `saved_schemas/` under `__system__`) | *"a directory of the database that is not a topic"* |
| a new `topic_version` of a system topic (it took the mark that protects it from `delete-topic`) | *"a system topic: its topic_version cannot be changed"* |
| a new `topic_version` of a topic of a treedb or a msg2db open on the tranger: written here, it lost its columns and pkey2s, and nothing wrote them back | *"a topic of a treedb: its schema is the treedb's, changed by save-schema + apply-schema, not by create-topic"* (or *"a topic of a msg2db: ..."*) |
| a new `topic_version` of a topic that is there, without `jn_cols` | *"a new topic_version of a topic that exists needs its jn_cols (an empty dict for a topic with no columns)"* |
| a new `topic_version` of a topic WITH columns, with an empty `jn_cols`: its columns were written empty | *"a new topic_version of a topic that exists needs its jn_cols: without them its columns were written empty"* |
| FROM A PEER ONLY: a `jn_var` that [`tranger2_topic_var_fault()`](<#tranger2_topic_var_fault>) refuses: a field the tranger keeps itself, `system_topic`, a `topic_version` that is not an integer from 0 to 2147483647, a `last_rowid_id` that is not an integer, 0 or more | its cause, e.g. *"jn_var marks a system topic, which nothing deletes"* |
| FROM A PEER ONLY: a NEW topic named as what a store keeps ([`tranger2_reserved_name_fault()`](<#tranger2_reserved_name_fault>)): a leading `.` or `__`, a `.json` or `.bak` suffix | its cause, e.g. *"a name starting with '__' is of the system"* |
| FROM A PEER ONLY: a NEW topic in the store of a treedb or a msg2db open on the tranger (beside its topics the store keeps `saved_schemas/`, `.blobs`): the treedb makes its topics (`C_TREEDB` `create-topic`) | *"a new topic in the store of a treedb or a msg2db: its topics are made by it"* |

```bash
command-yuno id=<id> service=tranger command=create-topic topic_name=__x jn_cols='{"id":"","tm":0}'
# -> -1 <role^name>: cannot create topic '__x': a name starting with '__' is of the system
command-yuno id=<id> service=tranger command=create-topic topic_name=pp system_flag='sf_string_key|sf_int_key'
# -> -1 <role^name>: cannot create topic 'pp': more than one key type in system_flag
command-yuno id=<id> service=tranger command=create-topic topic_name=pp jn_var='{"topic_version":3}'
# -> -1 <role^name>: cannot create topic 'pp': a new topic_version of a topic that exists needs its jn_cols (an empty dict for a topic with no columns)
command-yuno id=<id> service=tranger command=create-topic topic_name=events jn_cols='{}' jn_var='{"topic_version":3}'
# -> 0 <role^name>: Topic created: 'events'    (a topic with no columns by design)
```

**A record of a wrong shape** (`add-record`). The `why` of the answer:

| Refused | `why` |
|---|---|
| a NUL in a string of the record, at any depth (an ievent frame is decoded with `JSON_ALLOW_NUL`, and C reads a string up to its NUL: the pkey `"K\u0000evil"` was appended under the key `K`; up to 7.26.9). A record given as its json text with a NUL in the text is refused as *"What record?"*, with the `why` *"the json text of the record holds a NUL"* (it was parsed up to the NUL) | *"a string with a NUL in it"*, *"a key with a NUL in it"*, *"a json nested deeper than a parser builds"* ([`json_nul_fault()`](#json_nul_fault)) |
| a topic keyed by string (`sf_string_key`): no pkey, a pkey that is not a string, longer than `NAME_MAX`, or with a `/` or a leading `.` (a key is a directory) | *"no pkey, or a pkey that is not a string"*, *"a pkey longer than NAME_MAX"*, *"a pkey with a '/' or a leading '.'"* |
| a topic keyed by integer (`sf_int_key`): a pkey that is not an integer (up to 7.26.9 appended in silence under the key 0) | *"no pkey, or a pkey that is not an integer"* |
| a negative `__t__` (up to 7.26.9 appended in silence) | *"a negative __t__"* |
| a `__t__` above [`TRANGER2_MAX_TIME`](#timeranger2-max-time): the md2 row keeps 44 bits, and the flags were written over the rest (up to 7.26.9; a `__t__` too large for a date, `9223372036854775807`, was an ERROR *"gmtime() FAILED"*). Every `__t__` of 44 bits is a date (the last is in the year 559444), so this is the only check of the date | *"a __t__ larger than the time a record keeps (44 bits)"* |
| a `__t__` that falls in a purged period of its key: its file is at or before the `upto` of the key's base (`delete-key-files`). The log says *"record of a purged period of its key, refused"*, not *"record of a wrong shape"* (up to 7.27.6 the tranger refused it with an ERROR, and the peer read *"see the log"*) | *"a __t__ in a purged period of its key: its old files were deleted"* |
| a time in its tkey that the md2 row cannot keep (it is the `__tm__` of the row): an integer that is negative or above `TRANGER2_MAX_TIME`, or a string whose date is past it -- read by `approxidate()`, and times 1000 on an `sf_tm_ms` topic, as the append reads it ([`tranger2_record_tkey_time()`](<#tranger2_record_tkey_time>)). A date has no year limit: `"@99999999999999 +0000"` is any number of seconds (up to 7.26.9 appended with its time cut, without a word) | *"a time in its tkey that a record cannot keep (negative, or past 44 bits)"* |

```bash
command-yuno id=<id> service=tranger command=add-record topic_name=pp record='{"id":"../x","tm":1}'
# -> -1 <role^name>: record refused by topic 'pp': a pkey with a '/' or a leading '.'
command-yuno id=<id> service=tranger command=add-record topic_name=pp record='{"id":"1","tm":1}' __t__=17592186044416
# -> -1 <role^name>: record refused by topic 'pp': a __t__ larger than the time a record keeps (44 bits)
command-yuno id=<id> service=tranger command=add-record topic_name=pp record='{"id":"1","tm":"@99999999999999 +0000"}'
# -> -1 <role^name>: record refused by topic 'pp': a time in its tkey that a record cannot keep (negative, or past 44 bits)
command-yuno id=<id> service=tranger command=add-record topic_name=pp_ms record='{"id":"1","tm":"@20000000000 +0000"}'
# -> the same refusal: pp_ms is sf_tm_ms, and 2e10 seconds are 2e13 milliseconds, past 44 bits
command-yuno id=<id> service=tranger command=add-record topic_name=pp_ms record='{"id":"1","tm":"@1700000000 +0000"}'
# -> 0 <role^name>: record added to topic 'pp_ms', rowid 1
```

A NUL reaches a record only in the kw of a frame, which is decoded with
`JSON_ALLOW_NUL`; the json reader of the command line refuses a `\u0000`. The
kw of an `add-record` of a peer, and its answer:

```json
{"topic_name": "pp", "record": {"id": "K\u0000evil", "tm": 1}}
```

```text
-1 <role^name>: record refused by topic 'pp': a string with a NUL in it
```

**A replica answers READ-ONLY to the master-only commands**: `add-record`,
`delete-key`, `delete-key-files`, `archive-key-files`, `delete-topic`, and the `create-topic` of a NEW topic (no
directory of that name). Each one asks at its own place, and the READ-ONLY
answer logs nothing (*"\<role^name>: tranger 'tranger' is READ-ONLY, this yuno
is not its master: delete-key runs on the master"*):

| Command | What it asks before it asks the master |
|---|---|
| `add-record` | nothing: it asks the master first, before anything is read |
| `delete-key`, `delete-topic` | the topic name (a topic that is not there answers `Topic not found`, with its WARNING, an ERROR from C code), a peer's write into a topic of a treedb or a msg2db (refused, logged), and the open of the topic |
| `create-topic` | the shape of the topic (the table above, logged); then it asks the master only for a topic that is not there |
| `delete-key-files`, `archive-key-files` | as `delete-key`, and the key (one directory component) before the topic is opened; `archive-key-files` asks for its `archive_directory` first, right after the permission |

Up to 7.26.9 the tranger refused them with an ERROR (*"Only master can
delete"*, *"Not found and no master"*).

```bash
# on a replica
command-yuno id=<id> service=tranger command=delete-key topic_name=pp key=K force=1
# -> -1 <role^name>: tranger 'tranger' is READ-ONLY, this yuno is not its master: delete-key runs on the master   (no log)
command-yuno id=<id> service=tranger command=delete-key topic_name=nosuch key=K force=1
# -> -1 <role^name>: Topic not found: 'nosuch'   (a WARNING "topic not found, refused")
```

---

(gclass-c-treedb)=
## C_TREEDB

Hierarchical tree database manager — manages **TreeDB** instances on top
of timeranger with JSON schema support.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `path` | `string` | Storage path. |
| `filename_mask` | `string` | Filename pattern. |
| `master` | `bool` | `TRUE` for master, `FALSE` for read-only replica. |
| `exit_on_error` | `integer` | Log options for a critical error of a treedb, handed to its tranger as `on_critical_error`. Default `"2"` = `LOG_OPT_EXIT_ZERO`: a critical error EXITS the yuno. |
| `with_link_events` | `bool` | `SDF_RD`, default `1` since 7.26.0 (`0` up to 7.25.22). Copied to the `C_NODE` of every treedb it opens (and of `__system__`): a link/unlink publishes `EV_TREEDB_NODE_LINKED` / `UNLINKED`; `0`, the parent's `EV_TREEDB_NODE_UPDATED`, what a v1 SPA reads. A yuno served by a v1 SPA creates it with `"with_link_events", 0` (see [TreeDB crash course](../../../../yunos/c/yuno_agent/YUNO_TREEDB.md) §4.13). |
| `impose_c_schema` | `bool` | `SDF_RD`, default `1`, **not persistent**. Open every treedb with its schema from C, over a newer schema file on disk. `0`: open from the schema FILE, which `apply-schema` replaces. A literal is installed only when its `schema_version` is higher than the file's, and then WHOLE. It replaces the whole file, as in 7.25.4. `__system__` is projected from it whole, and that half is NEW after 7.25.4: a topic or column the literal does not declare is DELETED from `__system__` (7.25.4 only created and updated, so a removed topic stayed there and the next `save-schema` published it again). A delete that a **snapshot** of `__system__` holds is refused (*"cannot delete node, a snapshot still holds it"*); a write that fails (create, update, link; an ERROR) is the same case. The projection is then UNFINISHED and says so: a WARNING *"Schema projected into `__system__` only in part: its version is not recorded, and every open of the treedb retries it"* (`not_removed`, `not_written`, `how`), `c_schema_version` 0, and a RECORD, `saved_schemas/<treedb>.unfinished.json` under the `__system__` tranger, for example `{"schema_version": 3, "not_removed": ["treedb_x.departments"], "not_written": [], "leftovers": ["treedb_x.departments", "treedb_x.departments.id", "treedb_x.departments.name"], "draft_kinds": {}, "replaced_kinds": {}, "leftover_nodes": {"treedb_x.departments.name": {"value": "name", "order": 1, "header": "Name", "type": "string", "__parents__": ["treedb_x.departments"], ...}, ...}, "system_schema_version": 18}` (`draft_kinds`: the kind of each operator's draft the projection could not replace, for example `{"departments": "saved"}`; `replaced_kinds`: always written, `{}` when empty: the drafts a projection that DIED half way was replacing, carried forward by a retry that fails too, so the open that completes the projection reports them once, for example `{"users": "saved"}`; `leftover_nodes`: what the projection left at each leftover id, only the attributes a projection writes -- for a topic `value`, `order`, `pkey`, `pkey2s`, `system_flag`, `tkey`, `topic_version`, `system_topic`, `main_topic`; for a column `value`, `order` and the attributes of the column descriptor -- and its place, `__parents__` (the ids of the parents it hangs from, sorted), `null` where it left nothing, and nothing for an id in `not_written`: its write failed, and it is taken as left; `system_schema_version`: the meta-schema those nodes were kept under). The record is written whole (a `.new` file created `O_EXCL\|O_NOFOLLOW`, flushed, renamed over the old one). The numbers of the treedb's node are written LAST, on full success only: the node of a new treedb is created with `schema_version` and `c_schema_version` 0 and stamped at the end, so a process that dies half way leaves no projection that says it is complete. And a projection RECORDS what it is about to write BEFORE its first write: the same record, with `"in_progress": true`, `planned` (every id it will write, link, unlink or delete), `target_nodes` (what it writes at each, `null` for a delete or an unlink) and `replaced_kinds` (the drafts it will replace). The projection that completes removes it after its WARNING; one that fails writes over it what it could not do. So a process that dies between two writes always leaves a record, and the next open knows what was under way: a node that is as it was or as the projection writes it is the projection's, never reported; only a node that is NEITHER can be the operator's work. That open completes the projection and reports `replaced_kinds` once, whether the dead process had replaced those drafts already or not. For example, killed after `treedb_x.users.email` is created and before it is linked, the next open links it, completes and answers `withdrawn_at_open: {}`. The record, not `c_schema_version` 0, says that the projection is unfinished (a projection seeded from a dynamic file has `c_schema_version` 0 too, and it is complete). While it is there: every open retries the projection -- from what runs when no newer literal is installed (INFO *"Completing the projection into __system__, left unfinished by an earlier open"*, `source` `schema from C` or `schema file in use`), from the literal when one is installed or imposed (*"Updating TreeDB schema in __system__"*); the ids in `leftovers` are nobody's draft on any path (not in `draft_changed`, never in `withdrawn_at_open`) as long as they stay as the projection left them (`leftover_nodes`), and such a leftover stays one at every retry; `unfinished_projection` in `treedbs` and `saved-schema` lists `not_removed` and `not_written` (and `planned` after a process died half way); `save-schema` refuses. A projection that succeeds removes the record. An operator's draft is reported ONCE, by the open that REPLACES it in `__system__`, made before the failure or after it: a part of a draft that a projection cannot replace (a refused delete, a failed write) is not a leftover, it stays a draft (`draft_changed` shows it), its topic is not reported at that open, and the record keeps its kind in `draft_kinds`: the open that replaces it reports that kind, so a SAVED draft is `"saved"` although the first open already withdrew the saved schema (that open reports `saved_schema_version` and no topic). For example, with `departments.budget` added by the operator, a literal that drops `departments` and a snapshot that holds it: the open answers `draft_changed: {"departments": true}` and `withdrawn_at_open: {}`, the record's `leftovers` name `departments`, `.id` and `.name` but not `.budget`, and the open after the snapshot is deleted reports `"topics": {"departments": "unsaved"}`. An EDIT of a leftover IS a draft: a node at a leftover id that is not what `leftover_nodes` kept (an attribute a projection writes changed, its place changed -- a column moved to another topic, linked to one more, or left in no topic --, the node deleted, or created where the projection left nothing) is the operator's work, wherever the node is now; a change of `_geometry` or of the metadata is not. A MOVE is a draft of both topics: the leftover column `treedb_x.departments.name` moved to `users` answers `draft_changed: {"departments": true, "users": true}`, and the open that removes `departments` reports `{"topics": {"departments": "unsaved", "users": "unsaved"}}`. `saved-schema` shows it in `draft_changed`, a retry that cannot finish keeps it a draft (not a leftover in the new record, its kind in `draft_kinds`), and the open that replaces it reports it (the WARNING and `withdrawn_at_open`, `"unsaved"`, or `"saved"` when a pending save carries it). For example, with `departments` a leftover, a new header `"Operator name"` on `treedb_x.departments.name` answers `draft_changed: {"departments": true}`, and the open that removes `departments` reports `{"topics": {"departments": "unsaved"}}`; the same for an attribute of the topic itself (`main_topic`), and for `treedb_x.departments.name` unlinked from `departments` (the open that removes the topic removes the column too). A record without `leftover_nodes` takes every leftover as left, and so does a record whose `system_schema_version` is not the running meta-schema, with a WARNING (*"Leftovers of an unfinished projection were kept under another meta-schema: every leftover is taken as left, an edit of one made meanwhile is not told apart"*). Which treedb a node of `__system__` belongs to is read from the node -- a topic's id is its treedb's name and its `value`, a column's id is its topic's id and its `value` -- never from the start of its id: the treedbs `m2` and `m2.b` never touch each other's nodes (`m2.b.c` is the topic `c` of `m2.b`, not a topic `b.c` of `m2`), and `delete-treedb` deletes only the nodes of its treedb. A column whose topic node is gone and that more than one treedb could own is owned by the one that SAYS it (the record of its unfinished projection names the id, or else its literal, schema file or saved schema declares the topic and the column), and by the treedb projected now when none says it; only the treedb that takes it logs, ONE WARNING per node, *"Node of __system__ that no tree reaches and more than one treedb could own: taken by this treedb"* (`how`: `record`, `schema` or `none says it`; `candidates`); the others say nothing. An open reads of `__system__` only the treedb nodes and the topics and columns whose id starts with the name of the treedb and a dot (a node of another treedb that it needs, for example a column the operator moved into one of its topics, is read when it is needed): the cost of an open does not grow with the other treedbs of the store (with 40 treedbs of 200 columns, the open of one reads its 220 topics and columns, not 8 800). **A schema whose elements get one id is refused**: a name with a dot can give two elements the same qualified id (the column `x.y` of the topic `u` and the column `y` of the topic `u.x` are both `<treedb>.u.x.y`; the topic `b.c` of `m2` and the topic `c` of `m2.b` are both `m2.b.c`), and such a treedb does not open (`-1` *"cannot open treedb '\<name>': no valid treedb_schema (see the log)"*), a draft that collides is not saved and a saved schema that collides is not applied, each with ONE ERROR naming both, for example `{"msg": "Schema refused: two elements have the same qualified id in __system__ (a name with a dot), rename one of them", "id": "m2.b.c", "first": "topic 'c' of treedb 'm2.b'", "second": "a topic of treedb 'm2', a node of __system__"}` (in 7.25.4 the two were one node with no error, or the second treedb opened and logged *"Node already exists"* for its columns). No id changes, and no schema of the SDK or the projects has a dot in a name. What the operator linked differently is replaced in ONE open: a node the schema declares that is linked elsewhere (a column moved to another topic) is taken where the schema declares it, never created again (*"Node already exists"*), and a column linked to a topic that does not declare it is only UNLINKED from it when the schema declares it elsewhere or it belongs to another treedb (a topic of another treedb linked here is unlinked too, INFO *"Topic of another treedb, not declared by the schema from C: unlinked from the treedb in __system__"*); for example a column `departments.name` the operator moved to `users` is reported as `{"topics": {"departments": "unsaved", "users": "unsaved"}}`. A topic whose write or link fails leaves what it holds untouched: its columns are not written under a topic no tree reaches, and nodes the operator left for it are not deleted. A topic or column of the treedb that no tree reaches (unlinked by the operator, or by a link of a projection that failed) never blocks a projection: one the schema declares is TAKEN (written as the schema says and linked again), the rest is removed (INFO *"Node of the treedb that its tree does not reach: removed from __system__"*); when it is the operator's work its topic is reported as `"unsaved"` (no save carries a node in no topic), for example `{"topics": {"users": "unsaved"}}` for a column `treedb_x.users.email` the operator created and linked to nothing, taken by a literal that declares `users.email`. A record that cannot be WRITTEN (the disk refuses `saved_schemas/`) does not make the projection read as complete: the record is kept in memory and written again at every open (INFO *"Record of an unfinished projection written, the disk refused it before"*), and the node says it, `c_schema_version: -1`; after a restart the record is LOST, and says so, ONE WARNING at every read (*"Record of an unfinished projection is lost ..."*), `unfinished_projection: ["<treedb>"]`, `save-schema` refuses, the open retries, and what `__system__` holds over the file is taken for a draft (reported, never deleted in silence). A record that cannot be READ still means unfinished: ONE WARNING at every read (*"Record of an unfinished projection cannot be read: ..."*, the cause in `error`) and no other line, `unfinished_projection: ["<treedb>"]`, `save-schema` refuses, the next open retries and writes it again; its leftovers are unknown, so what `__system__` holds over the file is taken for a draft and reported by the open that replaces it, as `"unsaved"` (the kinds it kept are lost with it). A projection that was never STAMPED -- the treedb's node at `schema_version` 0, no record, a file in use at `schema_version` 1 or more -- is a seed that died half way: the next open completes it on every path. When the file runs, the INFO is *"Completing the projection into __system__, left unfinished by an earlier open"* with `why`, for example `{"treedb_name": "treedb_x", "why": "never stamped (schema_version 0), no record", "source": "schema from C", "schema_version": 1, "stored_version": 0}`. When a newer literal is installed or the literal is imposed, the INFO is *"Updating TreeDB schema in __system__"*, with no `why`, for example `{"treedb_name": "treedb_x", "schema_version": 2, "stored_version": 0, "in_use_version": 1}`. Nothing in it is a draft and nothing is reported. A node that says the literal (`schema_version` and `c_schema_version` equal to it) while `__system__` differs from the literal, with no file or a file behind it, with `impose_c_schema` or without it, is a projection STAMPED FIRST by 7.25.4 or earlier (which wrote the numbers, then each topic, then its columns) and a crash: the open completes it (INFO *"Completing the projection into __system__: it says it is of the schema from C, and part of it is not ..."*), and only a NODE (a topic or a column, never a topic with all its columns) that differs from both the old file and the literal is a draft: with the headers of `users.username` and `users.email` changed by the literal 2 and the process dead between the two writes, nothing is reported (before: `{"users": "unsaved"}`). A node that no tree reaches is the dead projection's when it is what the literal writes there and the old file does not declare it (a create whose link never happened: before, `{"roles": "unsaved"}`). An open that completes it and dies too keeps that literal in its record (`stamped_base`), and the open after it compares with both again. With no file, only a missing topic or column, or a topic whose `topic_version` is behind, counts: nothing then tells a draft from a gap (in 7.25.4 it was taken as done at every open). A store keyed by rowid (an older meta-schema) moves to qualified ids in a way that can die at any write: the next open completes the move (in 7.25.4 a qualified copy left half way failed the next move, *"Node already exists"*). Delete the snapshot (`delete-node` of its row in `__snaps__` of `treedb_system_schema`), or fix the cause of the ERROR, and open again. A literal that is not higher is not installed: the file runs and `__system__` keeps what it holds. With `impose_c_schema` on, a literal at the file's `schema_version` is not installed either (`treedb_open_db()` keeps the file at a tie): the FILE runs, another content is said at every open with the WARNING of the tie (*"Schema from C has the schema_version of the dynamic schema in use but another content: NOT applied, raise its schema_version to publish it"*, `"imposed": 1`, the `diff`), and `__system__` is projected (seeded) from the file, not from the literal that does not run; in 7.25.4 and before an imposed tie was silent, and a seed took the literal, whose difference read as the operator's draft. A topic the store runs at a `topic_version` ABOVE the file's (a literal installed whole over a topic an apply had raised; tranger2 keeps its own, and only `topic_cols.json` says what runs) is said at every open that runs the file, per topic: WARNING *"Schema file in use declares other columns than the store runs, at a topic_version behind the store's ... __system__ holds the file's columns, so save-schema has nothing to save until the topic is edited there: to keep what runs, edit the topic in __system__ to those columns, then save-schema and apply-schema; to run the file's, raise its topic_version above running_version (and the schema_version) in the schema from C"*, for example `{"topic_name": "users", "topic_version": 1, "running_version": 2, "path": ".../treedb_x/users"}` (`path`: the directory of the `topic_cols.json` that says what runs). `__system__` is projected from the file, so a save with no edit has nothing to save (see `save-schema`, `store_ahead`). To keep what runs, edit `users` in `__system__` to those columns (add `email` back), then save and apply: the save publishes it past what runs. To run the file's, raise it in the literal, for example `'schema_version': '4'` (above the file's 3) and `users` at `'topic_version': '3'` (above the running 2). **The first open of a treedb by this release** writes a record of the upgrade, `saved_schemas/<treedb>.upgrade.json` (`{"release": "7.25.5", "left_by_older_release": ["treedb_x.departments", "treedb_x.departments.name"], "leftover_nodes": {...}, "topics": {"treedb_x.departments": "departments", ...}, "system_schema_version": 18}`); until it exists, `__system__` is read as an older release left it. A node stamped with the version of the file in use may be a FIRST projection 7.23.0-7.25.4 stamped first and did not finish (every imposed treedb got its first projection so, over a file already at the literal's version; so did every treedb from before 7.13.0): when the file runs and IS the literal, what `__system__` misses of it is written from the literal and linked, ONE WARNING *"Restored from the schema from C: ..."* with `restored` (the ids) and `failed`, and nothing is reported as the operator's; when a newer literal is installed, what it misses of the old file is no deletion of the operator's (INFO, with the ids). After that first open a stamped projection is a complete one (this release stamps last), and a topic the operator deletes is a draft, never restored. And 7.25.4 and before never deleted from `__system__`: a node OF the treedb that neither the file in use nor a pending saved schema declares, and no record of an unfinished projection names, is LEFT BY AN OLDER RELEASE. While it stays as that first open found it, it is no draft (`draft_changed` does not name it); the open that removes it reports it apart, `withdrawn_at_open` `{"topics": {"departments": "left_by_older_release"}}` (never `"unsaved"`), with ONE WARNING *"Removed from __system__ what an older release left there: ..."* (`topics`, `ids`). An edit of it after the upgrade makes it the operator's, and a save then publishes it; as it was found, `save-schema` leaves it out of what it compares and writes (`data.left_by_older_release`). `delete-treedb` removes the record of the upgrade. Either way `__system__` is not read at open: it is where a schema is edited, and the MASTER projects into it what runs; a replica writes nothing and reads what the master wrote. With it on, `__system__` is re-made from the literal (whole) only when it has no projection of that treedb or a lower `schema_version`. It is configuration and the DEFAULT of every treedb: set it in the yuno's `main.c` (`'global': {'treedbs.impose_c_schema': false}`) or in its config file; `dynamic_schema_treedbs` names the treedbs that open from their file whatever it says. No command changes it: it is read when a treedb opens, and a treedb opens when its yuno starts. See [TreeDB crash course](../../../../yunos/c/yuno_agent/YUNO_TREEDB.md) §3.11. |
| `dynamic_schema_treedbs` | `list` | `SDF_RD`, default `[]`, **not persistent**. The treedbs that open from their schema FILE whatever `impose_c_schema` says, so their schema can change dynamically (`save-schema` + `apply-schema`). Configuration: `'global': {'treedbs.dynamic_schema_treedbs': ['treedb_wattyzer']}` in the yuno's `main.c`. The yuno's code still wins (`open-treedb impose_c_schema=1`). |

### Commands

| Command | Description |
|---------|-------------|
| `open-treedb` / `close-treedb` | Open or close a treedb instance. Parameters of `open-treedb`: `treedb_name`, `filename_mask` (default `%Y-%m-%d`), `treedb_schema` (a json object), `initial_load`, `exit_on_error`, `impose_c_schema`, `import_root`, `files_max_size`, `files_content_types`. An `open-treedb` from OUTSIDE (a peer's, its kw carries `__username__`) is checked BEFORE anything is made, and the code's settings are refused to it; an open by the yuno's C code goes as it always went: see [*What a peer hands C_TREEDB*](#c-treedb-peer-data). Among those checks, a peer never opens a treedb the yuno's code opened, nor a name whose two services (`<name>` and `tranger_<name>`) would take that treedb's, in any case and also while that treedb is closed (new after 7.26.9): with the code's `treedb_y` closed, `command-yuno id=<id> service=treedbs command=open-treedb treedb_name=tranger_treedb_y treedb_schema='{...}'` answers `-1` *"\<role^name>: cannot open treedb 'tranger_treedb_y': its services would take the names of treedb 'treedb_y', which the code of the yuno opens, and a peer cannot take them even while it is closed; nothing was changed"*. `open-treedb impose_c_schema=1`, passed by the yuno's code, imposes the schema from C whatever the attribute says. `open-treedb` of a treedb that is already open here is refused BEFORE anything is touched: `-1` *"\<role^name>: treedb '\<name>' is already open here: close-treedb first, nothing was changed"* (new after 7.25.4; 7.25.4 reconciled `__system__` first, with creates and updates only, and then failed with *"Internal error, tranger client NULL"*). A name whose `<name>` or `tranger_<name>` service exists already, and is not a treedb open here, answers `-1` with no log: with the broker's treedb open, `command-yuno id=<broker> service=treedbs command=open-treedb treedb_name=tranger_treedb_mqtt_broker treedb_schema='{...}'` answers *"\<role^name>: cannot open treedb 'tranger_treedb_mqtt_broker': a service named 'tranger_treedb_mqtt_broker' exists already, nothing was changed"*. Every answer of `open-treedb`, `close-treedb` and `delete-treedb` names the yuno, the refusals of their parameters too (*"\<role^name>: what treedb_name?"*, *"\<role^name>: what filename_mask?"*; 7.25.4: *"What treedb_name?"*, *"Treedb closed!"*, *"Treedb_name not found: '\<name>'"*). `open-treedb` answers `0` *"\<role^name>: treedb opened: '\<name>'"*; `-1` *"\<role^name>: treedb '\<name>' did not open, its schema was refused (see the log): close-treedb it before opening it again"* when `treedb_open_db()` refuses it (7.25.4 answered `0` *"Treedb opened!"*); `-1` *"\<role^name>: cannot open treedb '\<name>': no valid treedb_schema (see the log)"* when the schema is refused before (7.25.4: *"treedb_schema not found"*); and `-1` *"\<role^name>: cannot open treedb '\<name>': its tranger could not be created (see the log)"* (7.25.4: *"Internal error, tranger client NULL"*). After an open that did not open, its services stay until `close-treedb` (from outside: while the yuno plays, by the user who sent that `open-treedb`, with `force=1`; while it is paused, by any user with `open-close`; an open of the yuno's code is closed by its code), the next `open-treedb` answers `-1` *"\<role^name>: treedb '\<name>' did not open at its last open-treedb, its schema was refused (see the log): close-treedb it before opening it again, nothing was changed"*, and its `treedbs` row says `"opened": false`. The failed open logs its cause once (a schema file with no topics: ERROR *"No topics found"*), and the `close-treedb` that follows logs nothing: `C_NODE` neither sets the callback of a treedb that did not open nor closes it (7.25.4 logged *"TreeDB not found"* at the open and twice at the close, with stacks). `close-treedb` answers `0` *"\<role^name>: treedb closed: '\<name>'"*. A yuno that treats a `-1` of `open-treedb` as fatal stops there now: the agent logs it with `LOG_OPT_EXIT_ZERO`, exits `0`, and its `ydaemon` watcher does not relaunch it (7.25.4 answered `0` and the agent ran without its treedb). The treedb's tranger is created before the schema is decided: when another process holds its store, the treedb opens as a replica, runs its schema file, and `__system__` is not reconciled (INFO *"The store of the treedb is not written here..."*). `close-treedb` acts only on a treedb that THIS service opened, never on its own `__system__` treedb, whatever `force` says. **From outside it never closes a treedb that the yuno's code opened** (the agent's, a `db_history`'s, the broker's `treedb_mqtt_broker`), whether the yuno plays or is paused, with `force` or without (new after 7.26.9; up to 7.26.9 anyone with `open-close` closed it with `force=1`, and while the yuno did not play with no `force`). That code holds pointers to the treedb's `C_NODE` and tranger, and to what else it opened on that tranger (the msg2db `alarms` of the broker and of the `db_history` yunos), and the close freed what they point at: a crash of the yuno at its next write. `mqtt_broker` keeps its `treedbs` service and its treedb through a pause, so a close of a PAUSED broker was a crash at its next `play-yuno` or stop. The refusal is `-1` *"\<role^name>: cannot close treedb 'treedb_x': the code of the yuno opened it and holds its handles, playing or paused; the yuno's own lifecycle closes it (pause-yuno + play-yuno, or a restart of the yuno)"*, with a WARNING *"close-treedb from outside of a treedb the code of the yuno opened, refused"*. Such a treedb is closed only by the yuno's own lifecycle: a yuno whose code opens it at play closes it at pause (`pause-yuno` + `play-yuno` closes it and opens it again, and while it is paused its `treedbs` service is not there); `mqtt_broker` opens it at its start and closes it at its stop, so only a restart does (`kill-yuno` + `run-yuno`). **Any other close needs `force=1` while the yuno plays, and while it plays `force` from outside is taken only for a treedb that the SAME user opened with `open-treedb`**: another user gets `-1` *"\<role^name>: cannot force the close of treedb 'treedb_x' while the yuno plays: this user did not open it, and whoever did may hold its handles. Use pause-yuno + play-yuno"*, with a WARNING *"close-treedb force of a treedb this peer did not open, refused"*, and a close with no `force` while the yuno plays answers `-1` *"\<role^name>: cannot close a treedb while the yuno plays. Use pause-yuno + play-yuno, which reopens it through the yuno's own lifecycle (force=1 only if nothing cached its handles)"*, with no log. **Both rules ask whether the yuno PLAYS** (`gobj_is_playing()`). While it is PAUSED, any user with `open-close` closes a treedb that a PEER opened, with `force` or without, whoever opened it: in a yuno that keeps its `C_TREEDB` through a pause (as `mqtt_broker` does), bob's `command-yuno id=<id> service=treedbs command=close-treedb treedb_name=treedb_foo` of the treedb alice opened answers `0` *"\<role^name>: treedb closed: 'treedb_foo'"*. A treedb the yuno's code opened stays refused while paused too (above). The C code of the yuno keeps its `force`, and closes the treedbs it opened. So a peer closes the treedb it opened with `command-yuno id=<id> service=treedbs command=close-treedb treedb_name=<name> force=1`, and no command from outside closes a treedb the yuno's code opened. **A failed open keeps the saved schema.** An open that installs a newer literal withdraws the treedb's saved schema (WARNING *"Schema from C withdrew work on the schema at open"*, `saved_schema_version`), but the file is removed only when the open OPENS. An open that does not open keeps it, the operator's work: INFO *"Saved schema kept: the open that installs the schema from C did not open; the next one that installs a schema from C and opens withdraws it"* (`saved_schema_version`, `path`), and `withdrawn_at_open` answers `saved_schema_version` 0. `treedb_open_db()` writes the literal over the file before it checks the rest, so after a literal it refused the save is `stale` (below that file); with the good file put back and the literal rolled back, it is pending again and `apply-schema` installs it. For example, a save of `users` (saved schema 2 over the file 1) and a literal 3 with no `topics`: the open answers `-1` *"... did not open, its schema was refused ..."*, and `saved-schema` answers `"saved": false, "stale": true`, `"withdrawn_at_open": {"schema_version": 3, "saved_schema_version": 0, "topics": {"users": "saved"}}`; with the file 1 back and the literal 1, `"saved": true, "saved_schema_version": 2, "can_apply": true`. |
| `delete-treedb` | Delete a treedb's SCHEMA: its projection in `__system__`, EVERY `treedbs` / `topics` / `cols` node OF that treedb -- a column the operator moved to another of its topics and a node of it that no tree reaches included (in 7.25.4 a node that no tree reaches stayed) -- while a node of another treedb that somebody linked into it is only unlinked. It never touches the treedb's own store on disk. `force=1` is required and means "yes, delete the schema". It refuses the system schema by name, and it refuses a treedb that is OPEN, and `force` does not lift that: an open treedb keeps answering from the copy in memory while its schema no longer exists anywhere, and the next `open-treedb` dies on the C_TRANGER service still alive under its name. Close it first: a treedb a peer opened is closed by that peer, `close-treedb force=1` while the yuno plays, or by any user with `open-close` while it is paused (see `close-treedb` above); then `command-yuno id=<id> service=treedbs command=delete-treedb treedb_name=<name> force=1`. **From outside, a treedb the yuno's code opened is refused, open or closed, its name in any case** (new after 7.26.9: before, a peer's `delete-treedb` of it, while it was closed in a `C_TREEDB` that lived on, deleted what the code's next open reads, its projection in `__system__` and its files in `saved_schemas/`): `command-yuno id=<broker> service=treedbs command=delete-treedb treedb_name=TREEDB_MQTT_BROKER force=1` answers `-1` *"\<role^name>: cannot delete the schema of treedb 'treedb_mqtt_broker': the code of the yuno opened it, and its schema is the code's, open or closed"*, with a WARNING *"delete-treedb from outside of a treedb the code of the yuno opened, refused"*. The answer names an open treedb as it was opened, and a closed one as the peer wrote it. It is asked after `force` and the system schema, and before the OPEN check, so an open code treedb gets this answer too. The C code of the yuno deletes it as before. From outside, the name passes the name rule of `open-treedb` first: `x/../treedb_y` reached the files of the OPEN treedb `treedb_y` in `saved_schemas/` (up to 7.26.9 it removed its saved schema). A treedb whose last `open-treedb` did not open it is refused the same way, with `-1` *"\<role^name>: cannot delete the schema of '\<name>': its last open-treedb did not open it, and its services are still there. close-treedb it first, then delete-treedb"* (7.25.4 said *"while it is OPEN"*). It writes `__system__`, so it asks what `__system__`'s tranger IS: a master that lost its lock answers *"READ-ONLY"* (new after 7.25.4; 7.25.4 asked the `master` attribute and went on). It removes the treedb's saved schema from `saved_schemas/` too (new after 7.25.4; in 7.25.4 it stayed, and a treedb created again under the name found it), and the record of an apply not opened yet (`<treedb>.applied.json`), the record of an unfinished projection (`<treedb>.unfinished.json`) and the record of the upgrade (`<treedb>.upgrade.json`, see `impose_c_schema` above). It deletes the columns first, then their topics, then the nodes of the treedb that no tree reaches, and the `treedbs` node LAST, so a `delete-treedb` cut half way is finished by the next one: it answers `0` *"\<role^name>: schema of '\<name>' deleted, 5 nodes of __system__"* with the ids in `data.deleted`, for example `{"treedb_name": "treedb_foo", "deleted": ["treedb_foo.users.id", "treedb_foo.users", "treedb_foo"]}`. When the `treedbs` node is already gone (7.25.4 deleted it FIRST, and a delete cut after that answered `-1` *"not projected in __system__"* at every run), the nodes left in no tree are still the treedb's, read from the nodes, and they go. Nothing left answers `0` *"\<role^name>: nothing of the schema of '\<name>' was in __system__"*. A node that refuses the delete answers `-1`, keeps the `treedbs` node, and lists in `data.deleted` what went. |
| `create-topic` / `delete-topic` | Manage topics within a treedb that THIS service opened (not `__system__`): `command-yuno id=<id> service=treedbs command=delete-topic treedb_name=<name> topic_name=<topic>`. The answers name the yuno, the topic and the treedb: `0` *"\<role^name>: topic 'frames' created in treedb 'treedb_x'"*, `0` *"\<role^name>: topic 'frames' deleted from treedb 'treedb_x'"*, `-1` *"\<role^name>: cannot create topic 'frames' in treedb 'treedb_x' (see the log)"*, and `-1` *"\<role^name>: treedb 'treedb_x' not found"* (7.25.4: *"Topic created!"*, *"Topic deleted!"*, *"Treedb_name not found: 'treedb_x'"*). Parameters of `create-topic`: `treedb_name`, `topic_name`, `topic_version` (default `1`), `topic_tkey`, `pkey2s`, `cols`. A topic the treedb has is answered as it is (the create is idempotent). **From outside** (a peer's command), a NEW topic is checked first, as a topic of a peer's schema is (see [*What a peer hands C_TREEDB*](#c-treedb-peer-data)), with a WARNING, and refused with `-1` *"\<role^name>: cannot create topic '\<topic>' in treedb 'treedb_x': \<why>"*; an empty `topic_name` answers *"\<role^name>: what topic_name?"* with no log. **From the yuno's C code** the command goes as it always went: [`treedb_create_topic()`](<#treedb_create_topic>) judges the topic and logs what it refuses as an ERROR (an empty `topic_name` is *"topic_name empty"*), and the answer is *"cannot create topic '\<topic>' in treedb 'treedb_x' (see the log)"*. `delete-topic` refuses an empty `topic_name` (*"what topic_name?"*, no log), a topic the treedb does not have and a system topic: `-1` *"\<role^name>: Topic not found in treedb 'treedb_x': 'nosuch'"*, `-1` *"\<role^name>: '\_\_snaps\_\_' is a system topic of treedb 'treedb_x', it cannot be deleted"*. From outside it refuses too a topic of the treedb that ANOTHER database of the store holds as well (a C caller's `create-topic` can adopt one) -- a msg2db or another treedb open on the treedb's tranger, or one whose schema file in the store names the topic, open or not (see [*A topic of another database*](#c-treedb-foreign-topic)): `-1` *"\<role^name>: cannot delete topic 'alarms' of treedb 'treedb_x': the msg2db 'msg2db_alarms' holds it too"*, with a WARNING *"delete of a topic that a msg2db or another treedb holds too, refused"* (new after 7.26.9: `delete-topic` takes the directory and every record, with no `force`, and the msg2db's next write failed). A C caller's delete goes on, as it always went. |
| `diff-schema` | What the `__system__` projection of a treedb says that its schema from C does not. A stored `order` that says nothing about the node's place -- absent, or the default 9999 that a node written before `order` existed (before 7.14.0) is loaded with -- is no difference: `command-yuno id=<id> service=treedbs command=diff-schema treedb_name=treedb_x` over such a projection answers no `order` row, and neither `draft_changed` nor what a newer literal withdraws names its topics for it (the diff compares `order` since 7.25.5; 7.25.4 did not compare it). |
| (every write) | Asks what the tranger it writes IS, not the `master` attribute. A replica, or a master that lost its lock, answers `-1` *"\<role^name>: treedb '\<name>' is READ-ONLY, this yuno is not the master of its tranger"*. Between a stop of the service and its next start the tranger holds no lock, and the answer is `-1` *"\<role^name>: treedb '\<name>' is STOPPED: its tranger holds no lock until its service starts again"* (new after 7.25.4; in 7.25.4 `save-schema` and `delete-treedb` read the stale `master` of the stop and went on to a `__system__` whose treedb was closed, and `save-schema` answered *"nothing to save"*). Applies to `save-schema`, `delete-treedb`, `create-topic`, `delete-topic` and `apply-schema`. |
| (every command) | Asks a permission, except `help` and `authzs`: `open-close` (`open-treedb`, `close-treedb`, `delete-treedb`), `create-delete` (`create-topic`, `delete-topic`, `apply-schema`), `write` (`save-schema`), `read` (`diff-schema`, `treedbs`, `saved-schema`). The test walks C_TREEDB's command table (`tests/c/c_treedb_system_schema`), so a command added without one fails it. On a replica every write answers *"READ-ONLY"*. Every answer of every command starts with the yuno, a refused permission too: `-403` *"\<role^name>: No permission to 'read' in service 'treedbs'"* (7.25.4 had no yuno in `create-topic`, `delete-topic`, `treedbs`, `save-schema`, `saved-schema`, `apply-schema` and `diff-schema`). From outside, every command that takes a `treedb_name` asks it the name rule of `open-treedb` after the permission (see [*What a peer hands C_TREEDB*](#c-treedb-peer-data)): `-1` *"\<role^name>: treedb 'x/../treedb_y' refused: its name breaks the name rule (empty, '.', '..', '/', '\`' or '^')"*. `save-schema`, `saved-schema` and `apply-schema` of a treedb that this service has not opened (`treedb_system_schema` included) answer `-1` *"\<role^name>: treedb 'treedb_y' is not open here"*, with a WARNING *"treedb not open here, refused"* (an ERROR with a stack from C code; up to 7.26.9 an ERROR *"Treedb not open here"* for everyone). |
| `treedbs` | The treedbs this service opened (`treedb_system_schema` first), one row each: `opened` (`false` when the last `open-treedb` created its services but `treedb_open_db()` refused the schema: `close-treedb` it), `impose_c_schema` as it applies to that treedb, `decided_by` (`code`, `dynamic_schema_treedbs`, `impose_c_schema` or `system`), the `c_schema_version`, `in_use_schema_version` and `saved_schema_version`, and `master`: whether that treedb's tranger can write NOW (new after 7.25.4; a master that lost its lock reads `false`), and `stopped`: whether that tranger is stopped (new after 7.25.4), and `withdrawn_at_open`: what the last open of that treedb withdrew (`{}` when nothing; see `saved-schema`), and `unfinished_projection`: the ids of `__system__` that the projection could not remove or write, as its record says, and every id it was writing when its process died half way (`[]` when the projection is complete; see `impose_c_schema`). A STOPPED tranger -- the service was stopped and has not started again -- gave its lock back at the stop and holds none: it reads `master: false, stopped: true`, although its json still says the `master` it was until the next start revives it. Needs `read`. Example row: `{"treedb_name": "treedb_authzs", "opened": true, "impose_c_schema": true, "decided_by": "impose_c_schema", "c_schema_version": 19, "in_use_schema_version": 19, "saved_schema_version": 0, "master": true, "stopped": false, "withdrawn_at_open": {}, "unfinished_projection": []}`. |
| `save-schema` | Publish the draft of a schema edited in `__system__`: every topic that differs from the schema file IN USE gets its `topic_version` + 1, the treedb its `schema_version` + 1, and the schema is written to `saved_schemas/<treedb>.treedb_schema.json` under the `__system__` tranger, never over the file in use. `dry_run=1` answers it and writes nothing. Master only; permission `write`. `command-yuno id=<id> service=treedbs command=save-schema treedb_name=<name>`. A topic is raised past what RUNS, not only past the file: with the file at `users` 1 and the store running `users` 2 (see `impose_c_schema`), an edit of `users` saves `"topic_versions": {"users": 3}`, so the apply installs it (7.25.4 saved 2, and the apply reached nothing). With NO edit the draft is the file (`__system__` is projected from it), so there is nothing to save, and the answer names the topic: `0: <role^name>: nothing to save, the draft of 'treedb_x' is the schema in use; the store runs 'users' ahead of it with other columns, which the draft cannot say: to keep what runs, edit the topic in __system__ to those columns and save again; to run the file's, raise its topic_version and its schema_version in the schema from C`, with `"store_ahead": {"users": {"topic_version": 1, "running_version": 2, "path": ".../treedb_x/users"}}` (`{}` when the store runs no topic ahead of the file). The ORDER of the saved schema is the draft's: a node with a real `order` goes there, and one whose `order` says nothing (absent, or the default 9999: a projection from before 7.14.0, a column made by hand) goes where the file in use declares it, then where C declares it, then last; a projection from 7.13.1 whose file says `id, username, zeta, alpha` saves `id, username, zeta, alpha` (in 7.25.4 9999 was read as a position, and it saved `alpha, id, username, zeta`: a reorder nobody made). And the save writes into `__system__` the place each topic and column has in what it saved, where its node says another `order`: a column the operator added with `order` 99, third in `users`, is saved third, its node says 2, and `saved-schema` answers `draft_changed: {}` right after the save and after the apply. Each node is found by NAME, as the diff finds it, and written under its own id: a column the operator moved from `departments` to `users` keeps the id `treedb_x.departments.name` and gets its place in `users`, so `users` reads as saved after the save (the composed id `treedb_x.users.name` is no node). A topic of another treedb linked here keeps the id of its own treedb, and the `topic_version` a save raises is written under that id too. **A save publishes what its places imply**: a node placed in front of its siblings shifts them, and a sibling whose saved place is not the file's is published by the same save, with an `order` row in `changes` (`"stored"`: the place saved, `"from_c"`: the file's). For example, `order` 5 written on `treedb_x.users` saves `departments, users` and answers `"topic_versions": {"users": 2, "departments": 2}`; a second save answers the same, and after the apply there is nothing to save. `saved-schema` says it before the save: `draft_changed` is `{"users": true, "departments": true}`. **A node of more than one parent gets no place of its own.** The fkeys of the meta-schema are lists, so a column the operator links to a second topic too, or a topic of another treedb linked here that stays in its own, hangs from two parents, and one `order` cannot say its place in each. The save writes its `order` as 9999 (says nothing), the draft places it where the file in use declares it, then where the schema from C declares it, then last, and the comparison of drafts does not look at it. The answer says so: `0: <role^name>: saved 'treedb_x', schema_version 2; apply-schema puts it in use; 1 node(s) hang from more than one parent and get no place of their own (one `order` cannot say a place in each): see places_not_written`, with `"places_not_written": ["treedb_x.departments.name"]` (`[]` when there is none). Written from each parent's save, the place in one parent would read as a move in the other: the saves would flip between saved and unsaved for ever, and raise topics nobody edited. And a node that STOPS being shared goes where the file of the parent that remains put it: move `treedb_y.extra` into `treedb_x` by linking it there, save and apply `treedb_x`, then unlink it from `treedb_y`, and `treedb_x` answers `draft_changed: {}` and nothing to save. Kept, its `order` would be its place in its first parent (`treedb_y`), read as a real place in `treedb_x`: `draft_changed: {"extra": true, "departments": true}`, and a save that publishes a reorder of both. With no `treedb_name` (this and the next two) it acts on every treedb opened there and lists the answers. The file in use is read whatever shape its `topics` have, a list or a dict keyed by name (what a node opened with `impose_c_schema` off wrote before 7.25.0): the same schema gives the same diff and publishes the same versions. It writes `__system__`, so it asks whether `__system__` is the master's (it asked the treedb's tranger). A column's `default: {}` is not written, on any column: it is the meta-schema's placeholder for "no default", and kept on a `required` column it filled the field, so `required` never refused a record (as in 7.25.4). The trade-off: a `required` column whose literal really declares `'default': {}` loses it through save + apply, and a record created without that field is refused with *"Field required: '\<col>'"*. **A draft that is the file in use again withdraws the saved schema**: after "edit, save, undo the edit", a saved schema newer than the file in use is removed (logged *"Saved schema withdrawn, the draft is the schema in use"*), so `saved-schema` answers `can_apply: false` and Apply cannot install what was taken back (new after 7.25.4; in 7.25.4 the save answered "nothing to save" and left it). **What an older release left in `__system__` is left out** (see `saved-schema`): as the first open after the upgrade found it, it is neither compared nor written, and every answer names its ids in `data.left_by_older_release`. With nothing else changed: `0: <role^name>: nothing to save, the draft of 'treedb_x' is the schema in use` with `"left_by_older_release": ["treedb_x.departments", "treedb_x.departments.id", "treedb_x.departments.name", "treedb_x.users.email"]`. An edit of one of them makes it the operator's, and then it is saved: a left TOPIC that holds a column the operator added or edited is published WHOLE, with its left columns, `id` included (new after 7.26.9; up to 7.26.9 its left columns stayed out, and the save published a topic with no `id` column, which the treedb refuses at its next open). For example, `departments` left by an older release and its `name` given the header `"Operator name"`: the save publishes `departments` with `id` and `name`, and `data.left_by_older_release` no longer names them. (In 7.25.4 a save wrote the whole tree: it published what an older release left, and the next `apply-schema` put it back in the treedb.) The saved schema is written WHOLE, through `<treedb>.treedb_schema.json.new` and a rename, as the records beside it: a save that dies or finds the disk full leaves the pending save as it was (it was truncated and rewritten in place). Both "nothing to save" answers carry `data: {treedb_name, withdrawn, schema_version, path, changes, left_by_older_release, store_ahead}`, for example `0: <role^name>: the draft of 'treedb_x' is the schema in use: the saved schema_version 13 is withdrawn` with `withdrawn: true`; `dry_run=1` says "would be withdrawn" and removes nothing. **Refused while the projection is unfinished** (`unfinished_projection` not empty): `-1` *"\<role^name>: the projection of 'treedb_x' into __system__ is not complete, 2 node(s) could not be removed or written (see the log): a save would publish them"*, with `data: {"treedb_name": "treedb_x", "unfinished_projection": ["treedb_x.departments", "treedb_x.users.departments"]}`. **Refused for a draft whose elements get one id** in `__system__` (a name with a dot, see `impose_c_schema`): ONE ERROR naming both and `-1` *"\<role^name>: cannot save the schema of 'treedb_x': topic 'u.x' of treedb 'treedb_x' and column 'x' of topic 'u' of treedb 'treedb_x' get the same id 'treedb_x.u.x' in __system__, rename one of them"*, with `data: {treedb_name, changes, collision}`. **Refused for two siblings with one name**: two topics of the treedb (a topic of another treedb linked here, under a name the treedb has), or two columns of one topic. A schema keeps one per name, so the rebuild kept the last and the diff found the first. ONE ERROR *"Schema refused: two topics of the treedb in __system__ have the same name, unlink or rename one of them"* and `-1` *"\<role^name>: cannot save the schema of 'treedb_x': the topics 'treedb_x.users' and 'treedb_y.users' have the same name 'users', and a schema keeps one per name: unlink or rename one of them"*, with `data: {"treedb_name": "treedb_x", "twins": {"what": "topics", "treedb_name": "treedb_x", "topic_name": "", "name": "users", "first": "treedb_x.users", "second": "treedb_y.users"}}`. The LINK of such a topic is refused already (*"Treedb already has a topic with this name"*, see `treedb_link_nodes()`), but an autolink update, or a store written before, can hold one. (In 7.25.4 the save published the other treedb's `users`, at a `topic_version` it did not raise, and said nothing.) **Refused for a draft with no topics** (a treedb without topics does not open): a WARNING *"Draft of a treedb schema with no topics: not saved, a treedb without topics does not open"* and `-1` *"\<role^name>: cannot save the schema of 'treedb_x': its draft in __system__ has no topics, and a treedb without topics does not open"*. **Refused for a topic the treedb would refuse at its next open** (new after 7.26.9; up to 7.26.9 such a draft was saved, and refused only by `apply-schema`): each topic of the draft is checked as `apply-schema` checks it -- what the treedb refuses of a NEW topic ([`treedb_check_new_topic()`](<#treedb_check_new_topic>)), its directory in the store, the ids of its lists and of its nodes in `__system__` -- with its WARNING (an ERROR from C code), and from outside also what only a peer's schema is asked (one name per topic, the types of its values and versions, a name the store keeps, a topic of another database of the store, one `main_topic` on a topic hooked to itself: see [*What a peer hands C_TREEDB*](#c-treedb-peer-data)), and `-1` *"\<role^name>: cannot save the schema of 'treedb_x': topic 'users' of its draft: a pkey2 cannot be the pkey 'id', the treedb would refuse it at its next open"*, with `data: {treedb_name, changes}` -- for example a pkey2 `id` edited into `users` in `__system__`. **From outside, refused for a version past 2147483647** (new after 7.26.9): a save raises the `schema_version` and the `topic_version` of each changed topic by one, and one raised past [`TRANGER2_VERSION_MAX`](#timeranger2-version-max) was refused by the next `apply-schema` and by every later save, a peer locked out of its own treedb. Nothing is written: a WARNING *"save-schema would raise a version past 2147483647, refused"* (`version_of`: `schema_version` or the topic) and `-1` *"\<role^name>: cannot save the schema of 'treedb_x': the version 'schema_version' would pass 2147483647, the largest version a command can publish"* (for a topic: *"... the topic_version of topic 'users' would pass 2147483647, ..."*), with `data: {treedb_name, changes}`. The limit has a cost to know: a peer can open a treedb AT 2147483647 (the largest version it may hand), and then no edit of it can be saved again -- with the `schema_version` at the maximum, no edit at all; with a `topic_version` there, no edit of that topic. Give a peer's schema versions far below it. The C code of the yuno keeps its 64-bit versions. A `treedb_name` of another type than a string (`{"treedb_name": 5}`) is refused by the command parser: up to 7.26.9 it read as no treedb named, and saved EVERY treedb open there. |
| `saved-schema` | What `save-schema` wrote, what it changes against the file in use (`diff`: `added` / `removed` / `changed`, one row per `json2flat` leaf, the topics and columns keyed by name; their ORDER is a leaf of its own, `__topics_order__` and `topics`\<topic>`__cols_order__`, the names joined by `, `, so a moved column reads `"topics`users`__cols_order__": {"from": "id, username, email, departments", "to": "id, username, departments, email"}` -- new after 7.25.4; in 7.25.4 a save that only moved a column showed its versions raised and nothing else; an order leaf is a difference only when the names both sides declare come in another order, so an added or removed column is an `added` / `removed` leaf and not a moved one), `impose_c_schema` for that treedb (the code's force included) and `can_apply`. `saved` is `true` only for a save still PENDING (newer than the file in use); a saved file that is not newer answers `stale: true`, no `diff` and `can_apply: false` (new after 7.25.4; 7.25.4 answered `saved: true` with the diff of a schema already in use), for example `{"saved": false, "stale": true, "can_apply": false, "in_use_schema_version": 14, "saved_schema_version": 13, "diff": {}}`. A saved file that cannot be READ answers `broken: true` (not `stale`), `can_apply: false`, and a comment *"the saved schema of '\<treedb>' cannot be read (see the log): it is left out of apply-schema, save again to replace it"* (7.25.4 answered `saved: false`, with nothing to say why). A saved schema is withdrawn when an open writes the literal over the file it was saved against (no file in use, a newer literal, or an imposed one). **`withdrawn_at_open`** says what the last open of the treedb withdrew (new after 7.25.4; in 7.25.4 the log only): `{"schema_version": 22, "saved_schema_version": 21, "topics": {"users": "saved", "departments": "unsaved"}}` -- the literal that did it, the saved schema it withdrew (`0`: none), and each topic of the operator's work the literal replaced or removed: `"applied"` (an apply that never ran, as `apply-schema` recorded it), `"in_use"` (an apply that an open DID read: the dynamic schema the treedb was running; new after 7.25.4), `"saved"` or `"unsaved"` (a draft in `__system__`, a topic the operator DELETED from `__system__` included: `"saved"` when the pending saved schema does not declare it either, for example `{"topics": {"departments": "unsaved"}}` when a newer literal re-creates a topic the operator deleted and never saved; a topic the operator ADDED is `"saved"` only when the pending saved schema declares it, so `groups` added after a save of `users` is `{"users": "saved", "groups": "unsaved"}`), or `"left_by_older_release"` -- no operator's work: a topic, or a column of it, that 7.25.4 or before left in `__system__` and no schema declares, removed now (see `impose_c_schema`; for example `{"topics": {"departments": "left_by_older_release"}}`; any other kind of the same topic replaces it); `{}` when nothing. Only the topics an apply changed are `"applied"` / `"in_use"`: a topic that only the developer changed is nobody's work. What an unfinished projection left (its record's `leftovers`), as it left it, is not a draft either, and is never withdrawn, on any path: it is in `unfinished_projection`. An edit of it is a draft (see `impose_c_schema`). The same open logs ONE warning with the same content, *"Schema from C withdrew work on the schema at open"* (`treedb_name`, `schema_version`, `in_use_version`, `saved_schema_version`, `topics`). A save taken back replaces nothing. Permission `read`. `draft_changed` names the topics whose draft in `__system__` is NOT SAVED: it is diffed against the saved schema when there is one newer than the file in use, and against the file in use otherwise, without what an unfinished projection left -- what the schema editor marks as unsaved, rebuilt from it after a reload. It names too a topic whose PLACE the draft shifts, which a save publishes (see `save-schema`): `order` 5 written on `users` answers `{"users": true, "departments": true}`. Until 7.25.3 it was always the file in use, so a topic saved a moment ago read as unsaved until an Apply. Example after a save: `{"draft_changed": {}, "can_apply": true}`. A pending saved schema with no topics answers `can_apply: false` and the comment *"the saved schema of '\<treedb>' has no topics: apply-schema refuses it, a treedb without topics does not open"*. |
| `apply-schema` | Put the saved schema in place of the file in use: master only, only when C does not impose that treedb's schema, and only a saved `schema_version` higher than the one in use. It takes effect at the next open of the treedb (restart its yuno). Permission `create-delete`, like `create-topic` and `delete-topic`: it replaces the schema of a treedb whole (`save-schema` asks `write`; in 7.25.3 the two were the other way round). `command-yuno id=<id> service=treedbs command=apply-schema treedb_name=<name>`. The answer carries `data: {treedb_name, applied, saved_schema_version, in_use_schema_version}`; `applied` is `true` only when the file in use was replaced, and it is what says to restart. The file is written to a flushed temporary and renamed over the one in use (mode `rpermission`, 0660), never truncated in place. With no `treedb_name` it is **all or none up to the renames**: every treedb with something to apply is checked and written to its temporary first, and one that fails there leaves every file in use as it was (`-1`, every row `applied: false`). The renames that follow are one by one, and a rename that fails fails alone: its row `applied: false`, the others `true`, the answer `-1` with *"N of M treedb(s) applied, see each one"* -- read the rows, not the result. Each row is `{treedb_name, result, comment, data}` with that same `data`, and an apply with nothing to apply answers `0` and no row. A treedb whose saved file cannot be READ is not part of the all or none: it is left out, the others are applied, its row goes last with `applied: false, broken: true`, and the answer is `-1` with *"...; left out, their saved schema cannot be read: \<treedb>"* (new after 7.25.4; 7.25.4 refused every treedb). The file is written without the derived `fkey` marks of `parse_schema()` (new after 7.25.4; in 7.25.4 it carried them). Once in place the saved schema is removed from `saved_schemas/`: it IS the file in use (new after 7.25.4; in 7.25.4 it stayed, and read as saved). **A newer literal withdraws an apply that never ran, and says so.** `apply-schema` then `upgrade-yunos` with a literal whose `schema_version` is higher than the applied file: the literal is written over the whole file, as any newer literal is, and every applied topic it says otherwise (or removes) is reported as `"applied"` in `withdrawn_at_open` and in the warning (until 7.25.4 it was lost with no word). A saved schema whose elements get one id in `__system__` (a name with a dot) is refused like one that does not parse: ONE ERROR naming both, and `-1` *"\<role^name>: the saved schema of 'treedb_x' is refused: ... get the same id '\<id>' in __system__, rename one of them"*, the file in use unchanged. The apply records what it put in use in `saved_schemas/<treedb>.applied.json` (`{"schema_version": 13, "topics": {"users": "applied"}}`: the topics whose `topic_version` it raised), written WHOLE (a `.new` file created `O_EXCL\|O_NOFOLLOW`, flushed, renamed over the old one), BEFORE the file is renamed in place: a record that cannot be written refuses the apply with the file in use unchanged, and a rename that fails puts the previous record back. The record lives while that file is in use: the open that reads the apply marks its topics `"in_use"`, and a newer literal reports them and removes it -- both only once the open has OPENED (an open that fails leaves the record as it was). An applied topic is never guessed from the store: a topic whose store directory is gone is no apply. A saved schema with no topics is refused, like one that does not parse: a WARNING *"Saved treedb schema with no topics: not applied, a treedb without topics does not open"* and `-1` *"\<role^name>: the saved schema of 'treedb_x' has no topics: a treedb without topics does not open"*, the file in use unchanged. A saved schema whose topic the treedb would refuse at its next open -- a pkey2 `id` or the tkey, a column the treedb does not take, and from outside what only a peer's schema is asked (see `save-schema`), a topic of another database of the store among them -- is refused the same way, with its WARNING (an ERROR from C code), and `-1` *"\<role^name>: the saved schema of 'treedb_x' is refused: topic 'users': a pkey2 cannot be the pkey 'id'"*, the file in use unchanged (new after 7.26.9: it was applied, and the next open lost that topic with the CRITICAL *"Cannot create topic of the schema"*, an exit of the yuno). To keep the operator's change, take it into the literal. See [TreeDB crash course](../../../../yunos/c/yuno_agent/YUNO_TREEDB.md) §3.11. |

**Events.** `EV_OPEN_TREEDB` and `EV_CLOSE_TREEDB` are public events of
`C_TREEDB`, and they are NOT implemented: each one answers `-1` and logs an
ERROR, *"EV_OPEN_TREEDB is not implemented: nothing opened, use the command
open-treedb"* (and the same for `EV_CLOSE_TREEDB`), with `src` and
`treedb_name`. In 7.25.4 the open did nothing and answered `0`, and neither
said anything. Open and close a treedb with the commands, which
ask their permission. From outside, `close-treedb` closes only a treedb
that a peer opened. While the yuno plays, it needs `force=1`, and only the
user whose `open-treedb` opened it gets that `force`. While the yuno is
paused, any user with `open-close` closes it, with `force` or without (see
`close-treedb` above). A treedb that the
yuno's code opened is never closed from outside, playing or paused: its code
closes it through the yuno's own lifecycle (`pause-yuno` + `play-yuno` for a
yuno that opens its treedbs at play; a restart for `mqtt_broker`, which opens
its treedb at its start).

**The case of the name** (new after 7.26.9). The service registry finds a
service by its name in any case, and a closed treedb is a directory, whose
name is case-sensitive. So `C_TREEDB` reads a name in three ways:

- The name of an OPEN treedb is taken in any case. Every command then acts
  under the name the treedb was opened with, and the answer names that name:
  `TREEDB_FOO` closes `treedb_foo`, and the answer says `'treedb_foo'`. An
  `open-treedb` of `TREEDB_FOO` answers *"\<role^name>: treedb 'TREEDB_FOO'
  is already open here: close-treedb first, nothing was changed"*, with the
  name as it was written.
- The name of a treedb the yuno's CODE opened is refused to a peer in any
  case, also while that treedb is closed: `close-treedb`, `open-treedb` and
  `delete-treedb` of `TREEDB_MQTT_BROKER` are refused as for
  `treedb_mqtt_broker`. While it is closed, the answer names it as the peer
  wrote it. A name whose services are that treedb's (`tranger_<it>`, or
  `<x>` when the code opened `tranger_<x>`), in any case, does not open
  either: while the treedb is open, its services exist and the answer is
  *"a service named ... exists already"* (see `open-treedb` above); while it
  is closed, the peer gets its own refusal (see
  [*What a peer hands C_TREEDB*](#c-treedb-peer-data)).
- Any other name of a closed treedb is taken as it is written: `TREEDB_FOO`
  and `treedb_foo` are two stores, two directories of `path`. Only one of
  them can be open at a time, because they share their service names: with
  `TREEDB_FOO` open, `open-treedb treedb_name=treedb_foo` answers
  *"\<role^name>: treedb 'treedb_foo' is already open here: close-treedb
  first, nothing was changed"*.

For example, on a `mqtt_broker` `<broker>` where alice opened `treedb_foo`
and `treedb_bar` with `open-treedb`:

```
# the broker plays; bob, another user, forces the close of alice's treedb_foo: refused
command-yuno id=<broker> service=treedbs command=close-treedb treedb_name=treedb_foo force=1
# -> -1 <role^name>: cannot force the close of treedb 'treedb_foo' while the yuno plays: this user did not open it, and whoever did may hold its handles. Use pause-yuno + play-yuno

# alice forces it, with the name in another case: closed, and the answer names it as it was opened
command-yuno id=<broker> service=treedbs command=close-treedb treedb_name=TREEDB_FOO force=1
# -> 0 <role^name>: treedb closed: 'treedb_foo'

# the broker paused: the treedb its code opened is refused, with force or without
pause-yuno id=<broker>
command-yuno id=<broker> service=treedbs command=close-treedb treedb_name=treedb_mqtt_broker
# -> -1 <role^name>: cannot close treedb 'treedb_mqtt_broker': the code of the yuno opened it and holds its handles, playing or paused; the yuno's own lifecycle closes it (pause-yuno + play-yuno, or a restart of the yuno)
# the same treedb named in another case: refused the same way, and the answer names it as it was opened
command-yuno id=<broker> service=treedbs command=close-treedb treedb_name=TREEDB_MQTT_BROKER force=1
# -> -1 <role^name>: cannot close treedb 'treedb_mqtt_broker': the code of the yuno opened it and holds its handles, playing or paused; the yuno's own lifecycle closes it (pause-yuno + play-yuno, or a restart of the yuno)
# alice's treedb_bar, closed by bob while the broker is paused: no force needed
command-yuno id=<broker> service=treedbs command=close-treedb treedb_name=treedb_bar
# -> 0 <role^name>: treedb closed: 'treedb_bar'
play-yuno id=<broker>

# a db_history yuno: its code closes its treedbs at pause and opens them at play
pause-yuno id=<db_history>
play-yuno id=<db_history>
```

(c-treedb-peer-data)=
### What a peer hands C_TREEDB

A command whose kw carries a `__username__` came from OUTSIDE: `C_IEVENT_SRV`
puts it there, and the agent stamps the operator in a `command-yuno`. What
such a command hands is the peer's data, and a mistake in it is the peer's:
`C_TREEDB` refuses it with a WARNING (`msgset: MSGSET_PARAMETER`, no stack)
and `-1` that says why. A command without one comes from C code of the yuno,
and the same refusal is a broken contract: an ERROR with a stack. Every value
is read AFTER the permission check, in every command (new after 7.26.9 for
`open-treedb`, `close-treedb`, `delete-treedb`, `create-topic`,
`delete-topic` and `diff-schema`): a peer without the permission makes the
log write nothing of its values.

**An `open-treedb` from outside is checked before anything is made**, in this
order, and before the `C_TRANGER` of the treedb is created: a refused open
leaves no store directory and no service (up to 7.26.9 a bad name or a
missing schema left the directory and the `C_TRANGER` / `C_NODE` services
behind, a name longer than `NAME_MAX` was a CRITICAL of the tranger, and the
CRITICAL *"Cannot create topic of the schema"* EXITED the yuno with the
`exit_on_error` the peer passed). The answer is `-1`, in one of these forms:

| Answer | For |
|---|---|
| *"\<role^name>: cannot open treedb '\<name>': \<why>, nothing was changed"* | the name, the settings of the code, a `treedb_schema` that is not an object, the attributes and the seeds, the store |
| *"\<role^name>: cannot open treedb '\<name>': filename_mask '\<mask>' has \<why>, nothing was changed"* | the `filename_mask` |
| *"\<role^name>: cannot open treedb '\<name>': topic '\<topic>' of its schema: \<why>, nothing was changed"* | a topic of the schema, and its `main_topic` mark |
| *"\<role^name>: cannot open treedb '\<name>': its schema: \<why>, nothing was changed"* | the schema as a whole: no `topics`, its `schema_version`, two `file` columns of one `__assets__` hook, a schema that does not parse, two elements of one id in `__system__` |
| *"\<role^name>: cannot open treedb '\<name>': it is opened by the code of the yuno, and a peer cannot open it, nor its store, even while it is closed; nothing was changed"* | a treedb the yuno's code opened (below) |
| *"\<role^name>: cannot open treedb '\<name>': its services would take the names of treedb '\<code treedb>', which the code of the yuno opens, and a peer cannot take them even while it is closed; nothing was changed"* | a name whose services are those of a treedb the yuno's code opened (below) |

What is refused, in the order it is asked:

| What is refused | `why` (examples) |
|---|---|
| A `treedb_name` that breaks the topic rule, or holds a `^` (a gobj name cannot hold it: its `C_TRANGER` was not created, a CRITICAL) | `` its name breaks the name rule (empty, '.', '..', '/', '`' or '^') `` |
| A name that starts with `__`, the namespace of the system (`__system__` is the store of the schemas, and its lock was a CRITICAL), or any other name of a store ([`tranger2_reserved_name_fault()`](<#tranger2_reserved_name_fault>)): a leading `.`, a `.json` or `.bak` suffix. The name is the id of the treedb's node in the `treedbs` topic of `__system__`, which cannot start with a `.`: a treedb `.x` was opened, its projection was refused with an ERROR outside the window of the peer, it ran with no projection, and its `save-schema` answered *"projection ... is not complete"* (new after 7.26.9). The rule is a peer's: the yuno's CODE can still open a `.x` treedb (the code is trusted with its names), and it runs as it always ran -- with no projection in `__system__`, an ERROR at its open | *"a name starting with '__' is of the system"*, *"a name starting with '.' is a hidden file of the store"*, *"a name ending in '.json' is a file of the store"* |
| A name with `.treedb_schema.json` in it (the treedb cuts the name there, and registered it under another name than its `C_NODE`'s) | *"a name with '.treedb_schema.json' in it is cut there by the treedb"* |
| A name too long for the names made from it. Each one must fit `NAME_MAX`: the file `.<name>.treedb_schema.json.tmp` of `apply-schema`, and the id of the list of the `__assets__` index, `treedb-<yuno>-C_TRANGER-tranger_<name>-<name>-__assets__-`. `<yuno>` is the yuno that opens it, `<role>^<name>` (only `<role>` for a yuno with no name). The id is 38 bytes + the yuno and its `-` + 2 times the name: in `db_history_ce^1620` (19 bytes with its `-`), the name is 98 bytes at most. Before the yuno was in the id (up to 7.27.10), the limit was 108 bytes | *"its name is too long for the names of the files and lists made from it"* |
| A `filename_mask` that is not ONE file name. It names the files of the records (`strftime()` of their time), so it is made of letters, digits, `-`, `_` and the conversions that write only digits and `-`: `%Y %y %C %G %g %m %d %j %H %I %M %S %U %W %V %u %w %s %F` (the names of months and days depend on the locale). Each conversion counts as 20 bytes, and the expansion with `.json` must fit `NAME_MAX`. `../../tmp/evil` wrote the records out of the store, and `%Y/%m` made a CRITICAL of every append | *"a path separator"*, *"a '.' (a leading '.' or '..')"*, *"a conversion that may write more than digits and '-'"*, *"an expansion that may not fit a file name"* |
| `exit_on_error` other than `0`, `import_root` not empty, `impose_c_schema` true: the yuno's code sets them, never a command (a CRITICAL exits with `exit_on_error`; `import_root` is where `import-assets` reads files from) | *"exit_on_error is set by the yuno's code, not by a command"* |
| A `treedb_schema` that is not a json object | *"treedb_schema must be a json object"* |
| A schema whose `id` is not the `treedb_name`: it is the schema of another treedb, and it would be saved as `<treedb_name>.treedb_schema.json` with that other name in it. A schema with no `id` is taken. Logged as *"open-treedb with the schema of another treedb, refused"* (new after 7.26.9) | *"the id of its schema is not its name"* |
| A schema with no `topics` (a list, or a dict keyed by name); a `schema_version` that is not an integer from 0 to [2147483647](#timeranger2-version-max) (or its decimal string, as the literals write it: `'26'`) | *"a schema without topics (a list or a dict of them)"*, *"a schema_version that is not an integer from 0 to 2147483647"* |
| A topic that is not a json object, or with no name. **One name per topic**: in a list a topic is named by its `id`, a string; in a dict by its key. A `topic_name` (or, in a dict, an `id`) that says another name is refused: the readers of the schema took one or the other, so one name was checked and another one opened | *"a topic whose topic_name is not its id: the readers of the schema take one or the other"* |
| A topic whose `pkey` is not `id`, whose `system_flag` is not `sf_string_key`, whose `tkey` is not a string, whose `topic_version` is not an integer from 0 to 2147483647 (or its decimal string), or whose `system_topic` or `main_topic` is not a boolean | *"a topic whose pkey is not 'id'"*, *"a topic whose topic_version is not an integer from 0 to 2147483647"*, *"a topic whose main_topic is not a boolean"* |
| A topic the treedb refuses as a NEW topic, asked by the treedb itself inside the window of a peer's data ([`treedb_check_new_topic()`](<#treedb_check_new_topic>)): its columns, its `id` column, its `file` columns, a column both `hook` and `fkey`, a `hook` column not of type `dict`, `object`, `list` or `array` (an `fkey` one not of these or `string`), its pkey2s | the treedb's cause, e.g. *"a pkey2 cannot be the pkey 'id'"* |
| A topic named as what a store keeps ([`tranger2_reserved_name_fault()`](<#tranger2_reserved_name_fault>)): a leading `.` or `__`, a `.json` or `.bak` suffix | e.g. *"a name ending in '.json' is a file of the store"* |
| A topic whose directory in the store is a file, or a directory that is not a topic (no `topic_desc.json`: `.blobs`, where the files of the treedb live) | *"a file of the database has that name, it cannot be a topic"*, *"a directory of the database that is not a topic (no topic_desc.json)"* |
| A topic that ANOTHER database of the store says is its own: a msg2db or another treedb open on the store holds it, or a schema file of another database in the store names it, open or not (see [*A topic of another database*](#c-treedb-foreign-topic)). Logged as *"topic of the store that is not this treedb's, refused"* | *"a topic of a msg2db open on the store of the treedb: it is the msg2db's"*, *"a topic the schema file of a msg2db in the store of the treedb names: it is the msg2db's"* (and the same of *"another treedb"*) |
| A topic whose names do not fit: the list id of its `id` index and of each pkey2 (`NAME_MAX`), its id in `__system__`, `<treedb>.<topic>`, and each `<treedb>.<topic>.<col>` (255 bytes) | *"the names of the treedb, the topic and its tkey are too long for the list of its index"*, *"a column id too long for the id of the column in __system__"* |
| More than one topic marked `main_topic`, or the mark on a topic that is not hooked to itself (no `hook` column whose map names its own topic). The open kept the first mark and ignored the others, and each fault was an ERROR at every open, outside the window of a peer's data. Logged as *"schema with a wrong main_topic, refused"* | *"more than one topic marked main_topic"*, *"a topic marked main_topic that is not hooked to itself"* |
| Two `file` columns that derive one `__assets__` hook, `as_<topic>_<col>` ([`treedb_check_file_hooks()`](<#treedb_check_file_hooks>)) | *"two 'file' columns derive the same __assets__ hook (as_<topic>_<col>): rename one of them"* |
| A schema that [`parse_schema()`](<#parse_schema>) refuses (its faults are WARNINGs: it runs inside the window of a peer's data) | *"it does not parse (see the log)"* |
| Two elements that get one id in `__system__` (a name with a dot, see `impose_c_schema`), said in ONE WARNING that names both | *"two of its elements get the same id in __system__ (a name with a dot)"* |
| `files_max_size` that is not an integer (a real `1.5` included), `files_content_types` that is not a list of strings, `import_root` that is not a string | *"files_max_size must be an integer"* |
| `initial_load` that is not `{"<topic>": [<record>, ...]}`, a record that is not a json object, or a seed without its `id`, a string | *"a seed of initial_load without its id, a string"* |
| A treedb that the yuno's code opened in this `C_TREEDB`, while it is closed (a `C_TREEDB` that lives on after its code closed the treedb), its name in any case. Opened by a peer, its schema replaced the code's file in use, and the code's next open answered *"already open"* and went on without its treedb; in another case it is another directory but the same services, and the code's next open answered *"already open"* for the peer's `C_NODE`. Logged as *"open-treedb from outside of a treedb the code of the yuno opens, refused"* | (its own answer, in the table above) |
| A name whose services are those of a treedb the yuno's code opened, while it is closed, in any case. An open registers two services, `<name>` (its `C_NODE`) and `tranger_<name>` (its `C_TRANGER`): a peer's `tranger_<code treedb>` takes the name of the code's tranger, and for a code treedb named `tranger_<x>` a peer's `<x>` takes it with its own tranger. The code's next open answered *"a service named ... exists already"* (new after 7.26.9). Logged as *"open-treedb from outside with the service names of a treedb the code of the yuno opens, refused"*, with `code_treedb` | (its own answer, in the table above) |
| Then, as for any caller, a treedb already open here | (the answer of `open-treedb` above) |
| Then the store `<path>/<treedb_name>`, when it exists, must be THAT treedb's and free: a file; a directory without `<treedb_name>.treedb_schema.json` (the directory of `C_TREEDB` holds other stores: `C_AUTHZ`'s `authzs`, stores of a `C_TRANGER`); a store that a `C_TRANGER` of this process has open (compared by real path: a replica holds no lock); when this service opens masters, a store whose lock is held (an exclusive `flock()` of its `__timeranger2__.json`, taken and given back at once). Up to 7.26.9 such a name opened that store as a treedb with the peer's schema, or met its lock: a CRITICAL | *"a directory there is not the store of that treedb (no schema file of it)"*, *"its store is held by a master already, in this process or another"* |

```bash
command-yuno id=<id> service=treedbs command=open-treedb treedb_name=__mine treedb_schema='{"id":"__mine","schema_version":1,"topics":[]}'
# -> -1 <role^name>: cannot open treedb '__mine': a name starting with '__' is of the system, nothing was changed
command-yuno id=<id> service=treedbs command=open-treedb treedb_name=treedb_x filename_mask=%Y/%m treedb_schema='{...}'
# -> -1 <role^name>: cannot open treedb 'treedb_x': filename_mask '%Y/%m' has a path separator, nothing was changed
command-yuno id=<id> service=treedbs command=open-treedb treedb_name=treedb_x exit_on_error=2 treedb_schema='{...}'
# -> -1 <role^name>: cannot open treedb 'treedb_x': exit_on_error is set by the yuno's code, not by a command, nothing was changed
command-yuno id=<id> service=treedbs command=open-treedb treedb_name=treedb_x treedb_schema='{"id":"treedb_x","schema_version":1,"topics":[{"id":"users","topic_name":"people","pkey":"id","system_flag":"sf_string_key","topic_version":1,"cols":{"id":{"header":"Id","type":"string","flag":["persistent"]}}}]}'
# -> -1 <role^name>: cannot open treedb 'treedb_x': topic 'users' of its schema: a topic whose topic_name is not its id: the readers of the schema take one or the other, nothing was changed
command-yuno id=<id> service=treedbs command=open-treedb treedb_name=treedb_x treedb_schema='{...}' initial_load='{"users":[{"name":"x"}]}'
# -> -1 <role^name>: cannot open treedb 'treedb_x': a seed of initial_load without its id, a string, nothing was changed
command-yuno id=<id> service=treedbs command=open-treedb treedb_name=treedb_x treedb_schema='{"id":"treedb_x","schema_version":1,"topics":[{"id":"a","pkey":"id","system_flag":"sf_string_key","topic_version":1,"main_topic":true,"cols":{"id":{"header":"Id","type":"string","flag":["persistent"]}}}]}'
# -> -1 <role^name>: cannot open treedb 'treedb_x': topic 'a' of its schema: a topic marked main_topic that is not hooked to itself, nothing was changed
command-yuno id=<id> service=treedbs command=open-treedb treedb_name=treedb_x treedb_schema='{"id":"treedb_x","schema_version":1}'
# -> -1 <role^name>: cannot open treedb 'treedb_x': its schema: a schema without topics (a list or a dict of them), nothing was changed
# treedb_y: the yuno's code opened it in this C_TREEDB, and closed it
command-yuno id=<id> service=treedbs command=open-treedb treedb_name=treedb_y treedb_schema='{...}'
# -> -1 <role^name>: cannot open treedb 'treedb_y': it is opened by the code of the yuno, and a peer cannot open it, nor its store, even while it is closed; nothing was changed
# the same, named in another case: the answer names it as it was written
command-yuno id=<id> service=treedbs command=open-treedb treedb_name=TREEDB_Y treedb_schema='{...}'
# -> -1 <role^name>: cannot open treedb 'TREEDB_Y': it is opened by the code of the yuno, and a peer cannot open it, nor its store, even while it is closed; nothing was changed
# a name whose C_NODE would take the service name of treedb_y's tranger
command-yuno id=<id> service=treedbs command=open-treedb treedb_name=tranger_treedb_y treedb_schema='{...}'
# -> -1 <role^name>: cannot open treedb 'tranger_treedb_y': its services would take the names of treedb 'treedb_y', which the code of the yuno opens, and a peer cannot take them even while it is closed; nothing was changed
# the other way: the code opened a treedb named tranger_logs, and closed it; a peer's Logs would take its name with its own tranger
command-yuno id=<id> service=treedbs command=open-treedb treedb_name=Logs treedb_schema='{...}'
# -> -1 <role^name>: cannot open treedb 'Logs': its services would take the names of treedb 'tranger_logs', which the code of the yuno opens, and a peer cannot take them even while it is closed; nothing was changed
```

The topic of a peer's schema, written so that it opens:

```json
{
    "id": "treedb_x",
    "schema_version": 1,
    "topics": [
        {
            "id": "users",
            "pkey": "id",
            "system_flag": "sf_string_key",
            "topic_version": 1,
            "pkey2s": "email",
            "cols": {
                "id":    {"header": "Id",    "type": "string", "flag": ["persistent", "required"]},
                "email": {"header": "Email", "type": "string", "flag": ["persistent"]}
            }
        }
    ]
}
```

A `main_topic` mark, written so that it opens: ONE topic of the schema, and
hooked to itself -- a `hook` column whose map names its own topic (here
`children`, filled through the `parent_id` fkey of the child):

```json
{
    "id": "departments",
    "pkey": "id",
    "system_flag": "sf_string_key",
    "topic_version": 1,
    "main_topic": true,
    "cols": {
        "id":        {"header": "Id",       "type": "string", "flag": ["persistent"]},
        "parent_id": {"header": "Parent",   "type": "string", "flag": ["fkey"]},
        "children":  {"header": "Children", "type": "array",  "flag": ["hook"], "hook": {"departments": "parent_id"}}
    }
}
```

**An `open-treedb` from the yuno's C code goes as it always went** (the
agent, the controlcenter, a `db_history`, the MQTT broker): none of the checks
above is asked of it, and the name rule neither. What the treedb refuses of
its schema is logged and left out: the treedb logs its cause (an ERROR, for
example *"Topic refused: bad columns"*), then the CRITICAL *"Cannot create
topic of the schema"*, logged with the tranger's `on_critical_error` -- the
`exit_on_error` of the open. With `LOG_OPT_EXIT_ZERO` (`"exit_on_error": 2`)
that CRITICAL EXITS the yuno; with `0`, the default of the command, the
treedb opens without that topic. An `open-treedb` that answers `-1`
is the caller's to judge: the agent EXITS, and the yunos of the projects go
on with no treedb. A topic that is already ON DISK keeps the pkey2s and the
list ids it ran with, each fault an ERROR (see
[`treedb_create_topic()`](<#treedb_create_topic>)). A C caller can open a
treedb named `__x`, and close it.

```C
/*  the open of the yuno's code: no __username__ in the kw  */
json_t *jn_resp = gobj_command(gobj_treedbs, "open-treedb",
    json_pack("{s:s, s:s, s:i, s:o}",
        "treedb_name", "treedb_x",
        "filename_mask", "%Y-%m-%d",
        "exit_on_error", 0,             // a refused topic is left out, the yuno goes on
        "treedb_schema", jn_schema      // owned
    ),
    gobj
);
```

**The seeds of a peer are its data at every start.** For an external open,
`C_TREEDB` creates the `C_NODE` with `peer_initial_load` set, and what the
seeds get wrong (a topic the treedb has not, a parent ref that names no
parent, a hook that does not take the seed's topic) is a WARNING at the
open, and again at every later start of that `C_NODE` (the seeds run in its
`mt_start`: a stop and a start of the service applies them again). A
`pause-yuno` + `play-yuno` does not start it again: the yunos of today
destroy their `C_TREEDB` at pause, the treedbs a peer opened with it, or
(`mqtt_broker`) keep it running. The seeds of a C caller keep their ERRORs.
The store and the schema are opened OUTSIDE that window: a store that does
not open is an ERROR whoever opens it.

For example, a peer's seeds with a topic the treedb has not (`nope`) and a
ref through a hook that `groups` does not have (`nohook`), in a schema where
`items` has the fkey `group_id`:

```bash
command-yuno id=<id> service=treedbs command=open-treedb treedb_name=treedb_seeded treedb_schema='{...}' initial_load='{"nope":[{"id":"x"}],"groups":[{"id":"g1"}],"items":[{"id":"i2","group_id":"groups^g1^nohook"}]}'
# -> 0 <role^name>: treedb opened: 'treedb_seeded'
#    WARNING "initial_load: topic not found in treedb"   (topic_name: nope)
#    WARNING "initial_load: bad parent ref"              (why: its hook is not a hook of that topic that links the seed's topic)
# a stop and a start of the service treedb_seeded: the same two WARNINGs again
# the same initial_load in an open-treedb of the yuno's C code: the same two lines as ERRORs, with a stack
```

**`create-topic` from outside** checks a NEW topic as a topic of a peer's
schema is (the treedb's own checks, a name the store keeps, the store, a
topic of another database of the store, the names that must fit, a
`topic_version` past 2147483647), and its `file` columns against the hooks
that the `__assets__` of the open treedb already has. A topic the treedb
has is answered as it is, before any of these. **From the yuno's C code
none of them runs**: the command goes as it went before 7.27.0, straight to
[`treedb_create_topic()`](<#treedb_create_topic>), which judges the topic
and logs what it refuses as an ERROR. A name longer than `NAME_MAX` is one of
them: the ERROR *"Topic refused: bad name"*, and the tranger is never reached.
Only a NEW topic is refused there for its pkey2s and
the length of its list ids (see `treedb_create_topic()`).

```bash
command-yuno id=<id> service=treedbs command=create-topic treedb_name=treedb_x topic_name=.blobs cols='{"id":{"header":"Id","type":"string","flag":["persistent"]}}'
# -> -1 <role^name>: cannot create topic '.blobs' in treedb 'treedb_x': a name starting with '.' is a hidden file of the store
command-yuno id=<id> service=treedbs command=create-topic treedb_name=treedb_x topic_name=notes pkey2s=id cols='{"id":{"header":"Id","type":"string","flag":["persistent"]}}'
# -> -1 <role^name>: cannot create topic 'notes' in treedb 'treedb_x': a pkey2 cannot be the pkey 'id'
command-yuno id=<id> service=treedbs command=create-topic treedb_name=treedb_x topic_name=notes topic_version=2147483648 cols='{"id":{"header":"Id","type":"string","flag":["persistent"]}}'
# -> -1 <role^name>: cannot create topic 'notes' in treedb 'treedb_x': a topic_version that is not an integer from 0 to 2147483647
```

```C
/*  the yuno's code: no __username__, treedb_create_topic() judges it  */
json_t *jn_resp = gobj_command(gobj_treedbs, "create-topic",
    json_pack("{s:s, s:s, s:{s:{s:s, s:s, s:[s]}}}",
        "treedb_name", "treedb_x",
        "topic_name", "notes",
        "cols", "id", "header", "Id", "type", "string", "flag", "persistent"
    ),
    gobj
);
/*  the same with a topic_name of 300 bytes: the ERROR "Topic refused: bad
 *  name" (why: "the topic name breaks the name rule (empty, '.', '..', '/',
 *  '`', or longer than NAME_MAX)"), nothing on disk, and jn_resp is
 *  -1 "<role^name>: cannot create topic '<name>' in treedb 'treedb_x' (see the log)"  */
```

(c-treedb-foreign-topic)=
**A topic of another database.** The store of a treedb is the tranger of
other databases too: the `db_history` yunos and the MQTT broker open a msg2db
`msg2db_alarms` on the tranger of their treedb, so the store
`treedb_wattyzer/` holds the topic `alarms/` and the file
`msg2db_alarms.msg2db_schema.json` beside the topics of the treedb. Adopted
by the treedb, such a topic was the treedb's: its records were loaded as
nodes and written by `C_NODE`, a higher `topic_version` rewrote its
`topic_cols.json` and `topic_var.json` under the msg2db (which never wrote
them back: its own version is lower), and a `delete-topic` took the
directory and every record. So a PEER's new topic is refused when ANOTHER
database of the store says, positively, that the topic is its own (new after
7.26.9):

| Evidence | `why` |
|---|---|
| a msg2db open on the treedb's tranger holds the topic | *"a topic of a msg2db open on the store of the treedb: it is the msg2db's"* |
| another treedb open on the same tranger holds it | *"a topic of another treedb open on the store of the treedb: it is that treedb's"* |
| a `<msg2db>.msg2db_schema.json` in the store names it (written by `msg2db_open_db()`; the msg2db closed too, and whether its directory is there or not) | *"a topic the schema file of a msg2db in the store of the treedb names: it is the msg2db's"* |
| another treedb's `<treedb>.treedb_schema.json`, or a one-topic `<treedb>-<topic>.treedb_schema.json`, names it | *"a topic the schema file of another treedb in the store of the treedb names: it is that treedb's"* |

It is asked of a peer's `create-topic`, of each topic of a peer's
`open-treedb` schema, and of each topic of a draft that a peer's
`save-schema` or `apply-schema` publishes; a peer's `delete-topic` asks the
same evidence (see `delete-topic` above). A topic the treedb holds now is
its own. A directory that no other database names is taken as it always was:
a topic the treedb had and DROPPED comes back with its records (nothing
keeps the name of a dropped topic), a topic a `create-topic` made and nobody
published, a plain topic of a `C_TRANGER`. A schema file left behind by a
msg2db that is gone still names its topics: only a C caller, or removing
the file, gets past it. A C caller is asked none of this, as before.

```bash
# db_history_wz: the msg2db `msg2db_alarms` is open on the tranger of treedb_wattyzer
command-yuno id=<db_history_wz> service=treedbs command=create-topic treedb_name=treedb_wattyzer topic_name=alarms topic_version=2 cols='{"id":{"header":"Id","type":"string","flag":["persistent"]}}'
# -> -1 <role^name>: cannot create topic 'alarms' in treedb 'treedb_wattyzer': a topic of a msg2db open on the store of the treedb: it is the msg2db's

# a peer's own treedb: opened with the topics items and x, closed (force=1),
# opened with items only, closed; opened again with items and x:
command-yuno id=<id> service=treedbs command=open-treedb treedb_name=treedb_p treedb_schema='{"id":"treedb_p","schema_version":3,"topics":[<items>,<x>]}'
# -> 0 <role^name>: treedb opened: 'treedb_p'      (x is back, with the records it had)
```

**The MQTT broker opens its treedb as C code.** It held the yuno's
`__username__` in the kw of its own `open-treedb` and `close-treedb`, which
made them a peer's (its `impose_c_schema` was refused). It holds it as an
attribute now, as the agent and the controlcenter do, and its permission is
asked with it (see [`mqtt_broker`](#mqtt-broker-treedb-user)).

---

(gclass-c-node)=
## C_NODE

Node resource interface for TreeDB — full CRUD and graph operations on
tree nodes with linking, snapshots, and import/export.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `tranger` | `pointer` | The tranger the treedb lives on. Set by the host. |
| `treedb_name` | `string` | Treedb name. |
| `treedb_schema` | `json` | The schema from C (the literal), handed to `treedb_open_db()`. It is installed over the schema FILE on disk only when it is newer (with `impose_c_schema`, whenever its version differs); otherwise the treedb runs its file. It is never opened from `__system__`: `__system__` is where `C_TREEDB` projects the schema to be EDITED, and an edit reaches the file only through `save-schema` + `apply-schema`. `export-db` names its file with this schema's `schema_version`. |
| `initial_load` | `json` | Seed records, created if missing and marked immutable: `{"<topic>": [<record>, ...]}`, each record with its `id`, a string, and the parent refs (`topic^id^hook`) of the links it declares. What a seed gets wrong -- a topic the treedb has not, seeds of a topic that are not a list, a record without its string `id`, a parent ref that is not three parts, names a topic that is not of the treedb or a hook that does not take the seed's topic, a parent that is not there -- is logged by `C_NODE` and the seed (or the link) is left out: an ERROR with a stack, and a WARNING inside the window of a peer's data ([`treedb_set_peer_data()`](<#treedb_set_peer_data>)), which is open around the seeds of an external `open-treedb` (see `peer_initial_load`). Up to 7.26.9 every such fault was an ERROR, and a ref was handed to `decode_parent_ref()`, `treedb_get_node()` and `treedb_link_nodes()` before it was asked, each one an ERROR of its own. Example: `{"departments": [{"id": "direction", "name": "Direction"}], "users": [{"id": "admin", "departments": ["departments^direction^users"]}]}`. |
| `peer_initial_load` | `bool` | `SDF_RD`, default `0`: `initial_load` is a peer's data. Set by `C_TREEDB` for an `open-treedb` from outside (its kw carries `__username__`); a yuno does not set it. `C_NODE` applies the seeds at every start of the service (its `mt_start`: the start of the open, and any later `gobj_stop()` + `gobj_start()` of it; a `pause-yuno` + `play-yuno` is one only when the yuno's lifecycle stops and starts the service), and with it on it applies them inside the window of a peer's data, so what they get wrong is a WARNING at every start, not only at the open (new after 7.26.9). The treedb itself is opened outside that window: a store that does not open is an ERROR. |
| `with_link_events` | `bool` | Default `1` since 7.26.0 (`0` up to 7.25.22). A link/unlink publishes `EV_TREEDB_NODE_LINKED` / `UNLINKED` instead of the parent's `EV_TREEDB_NODE_UPDATED`, which collapses the whole parent (every child id) on every link. A gobj subscribed to EVERY event of the service, or the parent of a pure-child `C_NODE`, receives the two and must declare them. Set by `C_TREEDB` (or `C_AUTHZ`) at open; changed at run time with `set-link-events`. Written while the treedb is not open (before its open, or after an open that failed), it changes nothing then and logs nothing: the open reads it. |
| `impose_c_schema` | `bool` | Open with `treedb_schema` over a newer schema on disk too (`treedb_open_db()` option `"impose"`). Set by `C_TREEDB`. |
| `files_max_size` | `integer` | Largest file a `file` column accepts. Default 128 MB. A **memory** limit as much as a policy one — see *File columns* below. |
| `files_content_types` | `json` | Mime types a `file` column may hold, checked on the **bytes**. The default carries images, PDF, video and audio; `image/svg+xml` is **not** in it on purpose (an SVG served from the app's own origin runs script). A column narrows the list, never widens it. |
| `import_root` | `string` | Root that `import-assets` is confined to. Empty: `import-assets` is refused. |

### Commands

| Command | Description |
|---------|-------------|
| `create-node` / `update-node` / `delete-node` | CRUD operations on nodes. A record with `file` columns carries its bytes **beside** the record, in `__files__` — see below. `delete-node` takes two options that are NOT the same thing: `force` unlinks the children, and `ignore_snaps` deletes a node a snapshot still holds (and so breaks that snap's rollback). Since after 7.24.1 `force` no longer implies `ignore_snaps`. `ignore_snaps` erases records a snapshot froze, so it asks `create` (what `shoot-snap` asks) besides `delete` (after 7.25.3). Example: `ycommand -c 'command-yuno id=<id> service=<treedb> command=delete-node topic_name=items record={"id":"item1"} options={"force":1}'`. The comments name the yuno (new after 7.25.4): `<role^name>: Node update! 'item1' of topic 'items'`, `<role^name>: Node deleted, 'item1' of topic 'items'`. |
| `import-assets` | Turn a directory already on this node into N assets of `__assets__`: one command, no bytes on the wire. Confined to `import_root`. It creates index nodes, links nothing, and **answers the map `path -> id`** so the loader can link what it imported. `dry_run=1` says what it would take. The confinement is resolved, not only spelled: a `source_dir` with `..`, or one that resolves out of `import_root` through a symlink, is refused. |
| `gc-assets` | Delete the assets that **no live node and no snapshot** links — row and bytes. Never automatic: `delete-node force=1` unlinks children rather than deleting them, so an unlinked asset is a normal intermediate state of a bulk operation. `dry_run=1` lists what it would take. It sweeps too the blobs of the store that NO row names (what an interrupted write leaves). **Its `data` is the report of `treedb_gc_files2()`** (new after 7.25.4; BREAKING: 7.25.4 answered the list of ids taken): `{"dry_run": false, "assets": ["<id>", ...], "blobs": ["<id>", ...]}`, answered `0` with *"\<role^name>: deleted 1 orphan asset(s) and 0 blob(s) no row names"*. **A refused gc still says what it swept**: while a snap is active in any treedb of the tranger (the nodes in memory are the snap's photo, not the live links), or when what the snapshots hold cannot be read whole, or when a topic that links assets (or `__assets__`) did not load whole, the asset rows are left untouched and the blobs no row names are swept all the same (no link nor snapshot can lead to them). The answer is `-1`, `data.refused` carries the reason, `data.assets` is `[]`, `data.blobs` lists what was taken: *"\<role^name>: gc refused: a snap is active, the nodes in memory are its photo and not the live links (deactivate it first); the asset rows are untouched, deleted 1 blob(s) no row names"*. When the blobs cannot be swept either, `data.blobs_refused` says why. `command-yuno id=<id> service=<treedb> command=gc-assets dry_run=1` |
| `node` / `nodes` | Retrieve one node / list a topic's nodes (with filters). |
| `instances` | List node instances. |
| `link-nodes` / `unlink-nodes` | Manage parent-child relationships: `command-yuno id=<id> service=<treedb> command=link-nodes parent_ref=items^item00^children child_ref=items^item01` answers `0: <role^name>: Nodes linked, 'items^item01' to 'items^item00^children'`, and a refused one `-1: <role^name>: cannot link 'items^item01' to 'items^item00^children' (see the log)`. **The child ref is split at its FIRST `^`**: the topic is before it (a topic name has no `^`), the id is ALL the rest, up to `RECORD_KEY_VALUE_MAX - 1` (255) bytes. A child id is never written into a ref (only the child's fkey is, naming its parent), so a topic without hooks can hold an id with `^` or of 255 bytes -- an MQTT client id -- and the treedb lists it as it is: `child_ref=users^client^7` links the user `client^7` (new after 7.25.4: the child ref had to have exactly one `^` and an id shorter than 255 bytes, so such a child could be listed and never linked or unlinked by name). The parent ref stays `parent_topic_name^parent_id^hook_name`, exactly three parts: a parent's id is written into its children's fkeys and cannot hold `^`. A child ref with no `^`, or an empty topic or id, answers `-1: <role^name>: Wrong child ref` and logs *"Wrong child reference: must be \"child_topic_name^child_id\"..."*. On a REPLICA both answer `-1` READ-ONLY, **also for a pair that is already linked** (new after 7.25.4; in 7.25.4 the treedb found the pair linked, wrote nothing and answered `0`): on a replica every write is refused, one that would change nothing included, because whether it would is read from the replica's memory, which lags the master's disk. A C caller of `gobj_link_nodes()` / `gobj_unlink_nodes()` on a replica gets `-1` and the log line *"Cannot link nodes on a READ-ONLY replica"*. |
| `parents` / `children` | Navigate the graph. |
| `hooks` / `links` | Inspect hook and fkey relationships. |
| `jtree` | Get a node's full subtree as JSON, by a hook (a self hook makes a tree): each child whole, with its `__path__`, in the `hook` -- or in `rename_hook` (`data` for Webix), the hook then left out. Each child appears once (up to 7.25.22, without `rename_hook`, after the refs the hook already held: twice). Example: `ycommand -c 'command-yuno id=<id> service=<treedb> command=jtree topic_name=departments node_id=top hook=departments'`. |
| `shoot-snap` / `activate-snap` / `deactivate-snap` | Snapshot management. `deactivate-snap` answers `-1` when the save of the active snap fails: the snap is still ACTIVE on disk and the next start loads it (after 7.25.3; it answered *"Snap deactivated"*); its success is `<role^name>: Snap deactivated, treedb '<treedb>'`. `activate-snap` answers a snap that does not exist itself, `-1 <role^name>: snap not found: 's9'`, and any other failure as `cannot activate snap 's9' (see the log)` (new after 7.25.4; 7.25.4 quoted the process-wide last error message, whoever wrote it). When the new snap cannot be saved active, the old one is saved active again (*"Cannot activate snap, the one active before is active again"*). |
| `snaps` / `snap-content` | Inspect snapshots. `snap-content name=<tag>` (or `id=<n>`) counts the records the snap tags in each topic; `snap-content name=<tag> topic_name=users` lists them. Only the topics of ITS treedb: a topic of the tranger that is not a topic of the treedb (another treedb's, or a plain topic) is refused, `-1` *"\<role^name>: Topic not found in treedb 'treedb_x': 'other_topic'"*, and the overview does not count it (a snap id is the treedb's: another treedb tags its records with the same numbers). In 7.25.4 and before it read any topic of the tranger. See [YUNO_LIFECYCLE.md](../../../../yunos/c/yuno_agent/YUNO_LIFECYCLE.md) §6.7. |
| `import-db` / `export-db` | Bulk import/export. `import-db content64=<base64 of {"topic": [record, ...], ...}> if-resource-exists=abort\|skip\|overwrite` creates each record, then links them all (`update-node` with `autolink`). Its `data` counts what happened: `{"added": 1, "ignored": 2, "overwrite": 0, "failure": 0, "abort": 0, "link failure": 0, "errores": {"node exists": 2}}`. **`errores` counts the refused creates by their cause** (new after 7.25.4): `"node exists"`, or `"cannot create the node (see the log)"`. A create refused for another cause than an existing node is a `failure` in every mode, and stops the import in `abort` mode. 7.25.4 keyed `errores` on the process-wide last error message, which names the id (one key per record) or an older, unrelated error. **A link it cannot make is a `link failure`**: the record names a parent that does not exist, the record is saved without that link, and the treedb logs the cause (*"fkey reference: parent node not found"*); in 7.25.4 the update answered the node and it was counted a success. **An import that aborts, or has a `failure` or a `link failure`, answers `-1`**: `-1 <role^name>: import-db incomplete: 1 added, 0 overwritten, 0 ignored, 0 failed, 1 link failure(s) (see the log)`, or `import-db ABORTED: ...`; a clean one answers `0 <role^name>: import-db: 1 added, 0 overwritten, 2 ignored` (`ignored` records, in `skip` mode, are no failure). In 7.25.4 every import answered `0` with no comment. **A `content64` that is not json** is the peer's doing: `-1` *"\<role^name>: Bad json in content64"*, and ONE WARNING that names the peer the command came from, with the parser's error and at most 256 bytes of it, for example `{"msgset": "Protocol", "msg": "frame is not json", "peername": "10.0.0.7:52144", "error": "unexpected token near end of file", "position": 18, "len": 18}` (in 7.25.4 and before an ERROR with a stack trace and a dump of the whole buffer, naming no peer). `export-db` with no `filename` writes `<treedb>-<schema_version>-<YYYY-MM-DD>.trdb.json` under the realm's `temp/`, for example `treedb_x-3-2026-09-24.trdb.json` (the version is read as a number; read as a string an integer `schema_version` logged an ERROR and was left out of the name), and answers `-1` when the file cannot be written (it answered `0`). |
| `treedbs` / `topics` | List the treedbs of the tranger / the topics of a treedb. |
| `desc` / `descs` | Describe one topic's schema / every topic's. |
| `set-link-events` | Show (no `set`) or change (`set=1` / `set=0`; `set` is `DTP_JSON` since 7.27.0, a parameter the handler reads in more than one shape: a number, a boolean or their text -- `set=1`, `set=true`, `{"set": "0"}` from a SPA -- and `""` or no `set` shows) which events a link and an unlink publish, on the open treedb and at once: `1` publishes `EV_TREEDB_NODE_LINKED` / `UNLINKED` with the relationship (`hook_name`, `parent_topic_name`, `parent_id`, `child_topic_name`, `child_id`), `0` the parent's `EV_TREEDB_NODE_UPDATED` (what the v1 SPAs read). Either/or for every subscriber of the treedb. Needs the permission `update`. Not persistent: the next start takes the configured `with_link_events` again. Example: `ycommand -c 'command-yuno id=<id> service=<treedb> command=set-link-events set=1'`. |
| `print-tranger` | Dump the tranger the treedb lives on as bounded JSON (`kw_collapse()`-truncated: unexpanded containers answer as `[[size]]`, and `lists_limit` and `dicts_limit` bound the expansion). Pass `path=` (backtick-delimited, `kw_find_path` style, arrays by numeric index) to lazily drill into one subtree — this is what feeds the gui_treedb "Raw JSON" viewer. The path is walked as `C_TRANGER`'s `print-tranger` walks it ([`kw_find_path()`](<#kw_find_path>), a loop): a value that is not a dict or a list at its end is answered as it is (`path=directory` answers the directory, a string; up to 7.26.9 an ERROR of `kw_collapse()`), and a path that names nothing -- whatever its length, or a segment under a value that is not a dict or a list (`` path=directory`x ``) -- answers `-1` *"\<role^name>: path not found: '\<path>'"* and logs nothing (up to 7.26.9 ERRORs of `kw_find_path()`: *"buffer too small"*, *"kw must be list or dict"*). |

**Every comment starts with the yuno that answers** (`<role^name>: `), and a
failure says its own cause and points at the log: `create-node` answers
`<role^name>: Node created, 'item01' of topic 'items'` or `<role^name>: cannot
create the node of topic 'items' (see the log)`; `shoot-snap`, `snap-content`,
`nodes`, `instances`, `parents`, `children`, `jtree`, `desc`, `links`, `hooks`,
`topics` and `import-assets` the same (new after 7.25.4). In 7.25.4 they
answered `gobj_log_last_message()`, the process-global buffer of the last ERROR
of anybody -- a stale or unrelated cause -- and most answers carried no yuno.
The exception is the refusal of a permission, *"No permission to '\<perm>' in
service '\<treedb>'"*, which names its service. `instances` answers `-1` when it
cannot list (it answered `0`). The test walks C_NODE's command table
(`tests/c/c_node_authz`), so a command added with a bare comment fails it.
The log says its own cause too: an update of a node that does not exist, with
`create` and a create the library refuses, logs the library's error and then
*"Cannot update node: it does not exist and it cannot be created (see the
previous log)"* (new after 7.25.4; 7.25.4 logged the last message again as its
own).

### Permissions

Every command that reads or writes the treedb asks for a permission, inside
the command, with or without the global `enable_command_authz` gate. It
matters only in a yuno with an authz checker (`C_AUTHZ`).

| Permission | Commands |
|---|---|
| `read` | `nodes`, `node`, `instances`, `pkey2s`, `parents`, `children`, `jtree`, `hooks`, `links`, `snaps`, `snap-content`, `print-tranger`, `export-db`, `treedbs`, `treedb-info`, `topics`, `desc`, `descs`, `system-schema`, `schema-file`, `set-link-events` (shown, no `set`) |
| `create` | `create-node`, `import-assets`, `shoot-snap` |
| `update` | `update-node`, `link-nodes`, `unlink-nodes`, `set-link-events` (changed), `trace`, `activate-snap`, `deactivate-snap` |
| `delete` | `delete-node`, `gc-assets` |
| `create` and `update` | `import-db` |
| `delete` and `create` | `delete-node` with `options.ignore_snaps=1` |

Only `help` and `authzs` ask nothing. `system-schema`, `trace` and the
`set-link-events` display answered anyone until after 7.24.1; the test walks
C_NODE's command table (`tests/c/c_node_authz`), so a command added without a
permission fails it.

`read` also guards the treedb's feed: the five `EV_TREEDB_NODE_*` events are
`EVF_AUTHZ_SUBSCRIBE`, and `read` carries the `__subscribe_event__` alias, so
in a yuno that sets `enable_subscription_authz` a peer needs `read` on the
treedb service to subscribe to them (new after 7.25.4; in 7.25.4 any
authenticated user got the feed). The gate is off by default, and it goes in
the yuno's config:

```json
{
    "yuno": {
        "enable_subscription_authz": true
    }
}
```

A peer without `read` is refused and the channel stays open: ERROR *"No
permission to subscribe event"*, with `service`, `event`, `permission: read`
and `username`. See [`YUNO_AUTH.md`](../../../../yunos/c/yuno_agent/YUNO_AUTH.md) §4.6
and the test `tests/c/c_subscription_authz`, which subscribes to a real
`C_NODE`.

`update-node` with `options.create=1` also asks for `create` when the node
does not exist yet. A refusal answers `-403`, and nothing is written.

`options.create_only=1` makes a NEW node and refuses one that exists
(*"Node already exists"*); `options.create=1` alone is an upsert, and a taken
id there is an update of the existing record. A table's +New sends
`create_only` (gobj-ui 7.23.194). Both ask for `create` when the node does
not exist: `create_only` alone used to create under `update`.

```
command-yuno id=<id> service=<treedb> command=update-node topic_name=users record='{"id":"bob","username":"Bob"}' options='{"create_only":1}'
```

An `update-node` with `autolink` whose record names a link that cannot be
made saves the record and answers **-1**: *"\<role^name>: node 'x' saved, but
some of its links were NOT changed: a link it names cannot be made, and that
column was left as it was (see the log)"*. The fkey column keeps the links it had (see
`treedb_replace_links()`); it used to answer *"Node update!"*. The answer is
the update's own, even when a subscriber of its events updates the same service
before it answers (until 7.25.3 the nested update reset it to a success).

```
ycommand -c 'command-yuno id=<id> service=<treedb> command=delete-node topic_name=items record={"id":"item1"} options={"ignore_snaps":1}'
# -403 No permission to 'create' in service '<treedb>'   (a role with 'delete' only)
```

**A replica writes nothing, whoever asks.** The commands answer *"READ-ONLY"*
before anything is read. From C, every write method refuses before it moves
anything: `gobj_update_node()` returns `NULL` for every update but a `volatil`
one (memory only) -- the door C_AUTHZ uses for a user created from an event,
which no command guards; `gobj_link_nodes()`, `gobj_unlink_nodes()` and
`gobj_delete_node()` return `-1` and log *"Cannot link nodes / unlink nodes /
delete a node on a READ-ONLY replica"* (new after 7.25.4; 7.25.4 moved the
links in memory first and met the refused save last); `gobj_create_node()` is
refused by `treedb_create_node()`; and shooting or activating a snap by the
treedb (*"Only master can ..."*).

```C
/*  On a replica: -1, nothing moved  */
int ret = gobj_delete_node(gobj_node, "items",
    json_pack("{s:s}", "id", "item00"), json_pack("{s:b}", "force", 1), src);
```

**An autolink write that fails at its save changes nothing it wrote.**
`update-node` (or `gobj_update_node()`) with `autolink` writes the fields, the
links and the save as ONE write
([`treedb_update_node_and_links()`](#treedb_update_node_and_links)). When the
append fails (a full disk, a store gone), it answers `NULL` / `-1`. The fields
and the links go back in memory to what the disk has, and none of the events
of the write is told (`EV_TREEDB_NODE_LINKED`, `EV_TREEDB_NODE_UPDATED`):

- **create + autolink**: the create is on disk, with its ordinary fkeys EMPTY
  (a create stores none: only its `file` columns, which it links itself, and,
  for a secondary instance, the links it inherits from the primary). Its links
  are taken back, so memory and disk both have the node without them. The
  create was told (`EV_TREEDB_NODE_CREATED`), and an ERROR says what is left:
  *"Node created, but its links cannot be saved (autolink): the node stays
  without them"*.
- **update + autolink**: the node, its fields and its links are as the disk
  has them, in memory and on disk. One exception: the bytes of a `file`
  column of the record are stored BEFORE the write opens, as a write of their
  own. The blob and its `__assets__` node stay on disk, and their
  `EV_TREEDB_NODE_CREATED` (or `EV_TREEDB_NODE_UPDATED`, for a new name of
  bytes already stored) is told. The node does not link them.

In 7.25.4 this write was three calls, and a failed save took nothing back:
memory kept the fields and the links until the next reload, and
`EV_TREEDB_NODE_LINKED` had been told. Write it again once the cause is fixed:
the same `update-node` with `autolink` converges.

```C
/*  the store of `users` cannot be written (a full disk)  */
json_t *node = gobj_update_node(gobj_node, "users",
    json_pack("{s:s, s:s, s:[s]}",
        "id", "alice",
        "username", "ALICE-NEW",
        "departments", "departments^direction^users"
    ),
    json_pack("{s:b}", "autolink", 1),
    src
);
/*  node == NULL; alice keeps her old username and links, in memory as on
 *  disk; direction does not hook her; no event was published            */
```

`EV_TREEDB_UPDATE_NODE` is an input event for gobjs of the SAME yuno
(`gobj_send_event()`), with no permission asked. It is not a public event since
after 7.25.3: `C_IEVENT_CLI` delivered a public event from its peer to the
local service the event names, so the other end of an outbound session could
write the treedb.

Example: a user whose only role on `treedb_devices` has `"permission": "read"`
gets `nodes` and `node`, and is refused `link-nodes`:

```bash
ycommand -c 'command-yuno id=<yuno> service=treedb_devices command=link-nodes parent_ref=places^p1^devices child_ref=devices^d1'
# -403 No permission to 'update' in service 'treedb_devices'
```

(c-node-peer-data)=
### What a command names is the peer's data

A topic, a hook, a link, a parent ref or a record that a command names comes
from the peer, and a mistake in it is the peer's, not a broken contract of the
yuno. `C_NODE` refuses it **after** the permission check (a peer without the
permission learns nothing about which topics exist) with a WARNING
(`msgset: MSGSET_PARAMETER`; a name that is not yet known to be valid is
capped at 64 bytes, never in the middle of a character) and answers `-1`.
The `gobj_*_node()` methods and the treedb keep their ERROR for the same
mistakes when the caller is C code -- including a subscriber that writes from
its `EV_TREEDB_NODE_*` handler during the command, or from the callback of an
rt list of the topic (`C_TRANGER`'s `open-rt`): the treedb publishes and
appends outside the peer's window -- because for a C caller it is a broken
contract.

| The command names | WARNING | Answer |
|---|---|---|
| a topic the treedb does not have, or one that breaks the name rule ([`tranger2_name_is_identifier()`](<#tranger2_name_is_identifier>)) | *"topic not found, refused"* | `Topic not found: '<topic>'` (`link-nodes` / `unlink-nodes`: `Topic not found`, without the name) |
| a hook / link that is not a column of the topic with that flag | *"not a column of the topic with that flag, refused"* | `link-nodes` / `unlink-nodes`: `Hook not found: '<hook>'`; `parents`, `children`, `jtree`: `cannot list the ... (see the log)` |
| a hook whose map does not take the child's topic (`link-nodes`, `unlink-nodes`) | *"the hook does not link that topic, refused"* | `Hook '<hook>' does not link topic '<topic>'` |
| a column name that breaks the rule ([`treedb_col_name_is_identifier()`](<#treedb_col_name_is_identifier>)) | `` column name breaks the name rule (empty or '`'), refused `` | as the row above |
| a parent ref that is not `topic^id^hook` | *"parent ref must be topic^id^hook, with names of the name rule; refused"* (with a `why`) | `Wrong parent ref (see the log)` |
| a record that is not a json object, whose `id` is not a string, or with a NUL in a string at any depth (`create-node`, `update-node`, `delete-node`, `import-db`; also a `content64` that decodes to something else). A `null` id is absent: the treedb makes one (uuid, rowid, qualified) or says it is required. A frame is decoded with `JSON_ALLOW_NUL`, and C reads a string up to its NUL: up to 7.26.9 the id `"dx\u0000evil"` took the key `dx` (a create of `dx` then answered *"Node already exists"*), and an update of `g1` with the id `"g1\u0000x"` saved that id under the key `g1`, kept after a reload, with no log | *"record of a wrong shape, refused"* (with a `why`: *"the record is not a json object"*, *"the record id is not a string"*, *"a string with a NUL in it"*, ...) | `Wrong record: <why>` (up to 7.26.9: `Wrong record: a json object, with a string id`) |
| an `update-node` of a node that does not exist, without `create` | *"node not found, update refused (no 'create' option)"* | `Node not found: '<id>' of topic '<topic>'` |
| an `update-node` with `create_only` of a node that exists | *"node already exists, create_only refused"* | `Node already exists: '<id>' of topic '<topic>'` |
| `instances` by a `pkey2` that is not a secondary index of the topic (`id` names the primary one) | *"not a pkey2 of the topic, refused"* | `Not a pkey2 of topic '<topic>': '<pkey2>'` |
| `import-db` of a topic the treedb does not have, records that are not a list, or a record of a wrong shape | the same WARNINGs, one per topic or record | `-1` in every mode: in abort mode the import stops there (`import-db ABORTED: ...`); in `skip` and `overwrite` it goes on with the rest and answers `import-db incomplete: ...`. Each one is counted in `data.failure` and `data.errores` (`topic not found`, `wrong record: ...`) |

What only the treedb can judge -- a field required or of the wrong type, an
fkey that cannot be linked, a link that would close a cycle or links a node
to itself, an unlink of a child that does not hang from that parent, a create
of an id that exists or holds a path metacharacter, a value not in its enum,
a delete of a node a snapshot holds, a seed link the command would cut, a
time in the topic's tkey that the md2 row of the record cannot keep -- is
logged by the
treedb (or by the method) with its own message, as a WARNING without a stack:
the command marks the record as a peer's around the call
([`treedb_set_peer_data()`](<#treedb_set_peer_data>), since SDK 7.27.0), and
answers `-1` with *"cannot ... (see the log)"*.

```
# `users` links users, not departments (SDK 7.27.0+; before it an ERROR of the treedb)
command-yuno id=<id> service=<treedb> command=link-nodes parent_ref=departments^top^users child_ref=departments^dev
# -> -1 <role^name>: Hook 'users' does not link topic 'departments'

# an update of a node that is not there: add options='{"create":1}' to make it
command-yuno id=<id> service=<treedb> command=update-node topic_name=departments record='{"id":"nosuch","name":"Nobody"}'
# -> -1 <role^name>: Node not found: 'nosuch' of topic 'departments'

# a link that closes a cycle: dev hangs from top (a WARNING of the treedb, no stack)
command-yuno id=<id> service=<treedb> command=link-nodes parent_ref=departments^dev^departments child_ref=departments^top
# -> -1 <role^name>: cannot link 'departments^top' to 'departments^dev^departments' (see the log)
```

**The time of the tkey must fit the record.** The tkey of a topic is the
`__tm__` of each record's md2 row, which keeps 44 bits
([`TRANGER2_MAX_TIME`](#timeranger2-max-time)). A topic made by a peer
(`create-topic ... topic_tkey=when`) can have a string tkey, read as a date,
and a date has no year limit: `"@99999999999999 +0000"` was a node whose
`__tm__` was not its date, cut without a word. `create-node` and
`update-node` refuse it before anything is written (new after 7.26.9), with
*"Node refused: the time of its tkey does not fit a record (negative, or past
44 bits)"* (a WARNING from a peer; the same `gobj_create_node()` /
`gobj_update_node()` of C code is an ERROR with a stack, and is refused too).
An update that does not write the tkey keeps the time the node had, and a
link, an unlink or a snapshot of a node that already holds such a time is
not refused (see [`treedb_create_node()`](<#treedb_create_node>)). For
example, with the string tkey `when`:

```
# events: topic_tkey=when, a string column
command-yuno id=<id> service=<treedb> command=create-node topic_name=events record='{"id":"e1","when":"@99999999999999 +0000"}'
# -> -1 <role^name>: cannot create the node of topic 'events' (see the log)
command-yuno id=<id> service=<treedb> command=create-node topic_name=events record='{"id":"e2","when":"@1760000000 +0000"}'
# -> 0 <role^name>: Node created, 'e2' of topic 'events'
command-yuno id=<id> service=<treedb> command=update-node topic_name=events record='{"id":"e2","when":"@99999999999999 +0000"}'
# -> -1 <role^name>: cannot update the node of topic 'events' (see the log)
```

**A tkey the clock writes always fits.** The clock overwrites a column on
every create and every update only when it is of type `integer` and flagged
`now`: it stamps the time in seconds, and the value the caller sends is
ignored. So such a tkey is never refused. Not every topic has one: a topic
can have no tkey (`'tkey': ''`, the default of `create-topic`), and a `now`
on a column of another type stamps nothing (a `string` tkey flagged `now`
keeps the caller's text, and is checked as above). An `integer` column
flagged `time` without `now` is not one either: it gets the clock only at a
create that sends no value for it and whose column has no `default`, an
update that does not send it leaves it as it is, and a value that is sent is
kept and checked as above. A topic whose tkey the clock writes, in the form
of a C schema literal (as `users_accesses` of `treedb_authzs`):

```C
{
    'id': 'accesses',
    'pkey': 'id',
    'tkey': 'tm',
    'system_flag': 'sf_string_key',
    'topic_version': '1',
    'cols': {
        'id': {'header': 'User', 'type': 'string',  'flag': ['persistent', 'required']},
        'tm': {'header': 'Time', 'type': 'integer', 'flag': ['time', 'now', 'persistent']}
    }
}
```

```
# accesses: tm is stamped by the clock; the value sent is ignored, so it is never refused
command-yuno id=<id> service=<treedb> command=create-node topic_name=accesses record='{"id":"a1","tm":17592186044416}'
# -> 0 <role^name>: Node created, 'a1' of topic 'accesses'
```

A tkey flagged `time` without `now`, as `'born': {'header': 'Born', 'type':
'integer', 'flag': ['time', 'persistent']}` of a topic `births` with `'tkey':
'born'`, gets the clock only when the create sends nothing:

```
command-yuno id=<id> service=<treedb> command=create-node topic_name=births record='{"id":"b1"}'
# -> 0 <role^name>: Node created, 'b1' of topic 'births'      (born: the time of the create)
command-yuno id=<id> service=<treedb> command=create-node topic_name=births record='{"id":"b2","born":17592186044416}'
# -> -1 <role^name>: cannot create the node of topic 'births' (see the log)
```

With a `default` on that column, `'born': {'header': 'Born', 'type':
'integer', 'flag': ['time', 'persistent'], 'default': 0}`, the create of
`b1` stores `0`, not the clock: the default is the value the create did not
send.

(treedb-file-columns)=
### File columns: `__assets__`

A treedb node often owns something that is not JSON — a photo, a plan, a
signed pdf. Those bytes cannot go *in* the treedb: it is held in memory and
timeranger2 rewrites the whole record on every update, so a 40 KB photo would
be rewritten every time its node changed state and would ride along in every
page of `nodes`. Measured on one census: 12 134 blobs, 346 MB on disk, would
be ~460 MB of RAM for the life of the yuno.

So **you mark a column `file`, and treedb gives you a pseudo-filesystem**: you
hand it a file, you get it back; the *index* lives in memory and the *content*
on disk. Design note:
[`DESIGN-treedb-files.md`](https://github.com/artgins/yunetas/blob/7.29.2/kernel/c/timeranger2/DESIGN-treedb-files.md).

```
'foto': {'header': 'Photo', 'type': 'string', 'flag': ['fkey', 'file']}
```

- **The column is an fkey into `__assets__`**, a system topic every treedb
  creates at open next to `__snaps__` and `__graphs__` (shown in system mode
  only). Its rows are the index entries: `id` (the **sha256 of the content**),
  `content_type`, `size`, `t`, `original_name` (the only writable one),
  `uploaded_by`. Its **hooks are derived**: for every column `C` of topic `T`
  flagged `file`, `__assets__` gains `as_<T>_<C> -> {T: C}` in memory, never
  written to `topic_cols.json`. The host declares nothing but the column, and
  nothing about `__assets__` is ever versioned. The derivation **follows the
  schema at run time**: `create-topic` and `delete-topic` are live commands,
  so a topic added while the yuno runs gets its hook, and one deleted takes
  its hook — and the children it held — away with it.
- **The bytes live under the treedb**, at `<treedb dir>/.blobs/ab/cd/<sha256>.<ext>`,
  so a `cp -a` of the treedb directory carries the nodes **and** their bytes.
- **The bytes ride beside the record**, never inside the column, in a
  `__files__` manifest keyed by column, consumed at the door. A browser sends
  `content64`; a C caller (command or `EV_TREEDB_UPDATE_NODE` alike) puts a
  real `gbuffer` in the kw and the manifest says
  which slice is whose (`offset`, `size`) — a kw holds ONE binary field, so one
  buffer carries every file of the record:

```json
{"topic_name": "devices",
 "record": {"id": "E22000041",
            "foto": "",
            "__files__": {"foto": {"content64": "...", "original_name": "E22000041.jpg",
                                   "content_type": "image/jpeg"}}},
 "options": {"create": 1, "autolink": 1}}
```

- **Treedb re-hashes what arrives.** A client may put the sha256 in the column
  up front, ask `node` whether that asset exists and skip the bytes if it does
  (a census reload then sends ~0 instead of 346 MB) — but the id is a claim,
  never an authority: a wrong id with good bytes is refused, and a bare id of
  an asset nobody stored is refused. A bare id of an existing asset links it.
  A column may also arrive holding the **full** reference, and then it must be
  exactly `__assets__^<id>^as_<T>_<C>` of that very column: the link is made
  by the hook the value names, so one naming another `file` column's hook
  would store the file into that other column.
- **Size and type are checked at the door, on the bytes.** The size is checked
  on the base64 before decoding; the type is *sniffed* from the first bytes
  and the declared one must agree — a png called `image/jpeg` is refused, and
  an svg called `image/png` is refused, which is the case the allowlist exists
  for. Two levels: the treedb's ceiling (`files_max_size`,
  `files_content_types`) and the column's policy, in its `properties`
  (`{'max_size': 4096, 'content_types': ['application/pdf']}`), which narrows
  the ceiling and never raises it. The ceiling itself sits **behind** the
  transport's: the message was accepted whole and parsed before treedb saw it,
  so keep `files_max_size` under the transport's `max_pkt_size`.
- **Three writes, in order**: the blob, the `__assets__` node, the host record
  with its link. Interrupted early it leaves an orphan blob or an orphan index
  node, which `gc-assets` takes; never a link to nothing. A `create-node`
  whose `file` column cannot be linked is **undone**, so the answer never
  says yes over a record with an empty column.
- **The write path links the `file` column itself, `autolink` or not.** An
  ordinary fkey moves only through `link-nodes` or an `autolink`; a `file`
  column is edited by handing over a file, and the link is part of it:
  `create-node` and `update-node` link what the column names, `""` unlinks,
  and a column the record does not carry is left alone. The `autolink` in
  the example above is for the OTHER fkeys of the record, not for `foto`.
- **A second arrival of the same bytes is an update of the asset node**, so
  the history of `__assets__` says every name a file arrived under (a manifest
  that carries no `original_name`, or the name already stored, says nothing
  new about the file and appends nothing). The blob
  is written once, and the **first** arrival names it for ever: the extension
  is part of the served path and the URL is cached for ever, so a later
  arrival that declares another member of the same container (`audio/mp4`
  for what was stored as `video/mp4`) keeps the stored `content_type` and
  says so in the log.
- **`gc-assets` reads the snapshots, exactly.** `shoot-snap` skips every `__`
  topic, so an asset node never carries a tag and the tag guard never protects
  it: the collector walks the instances on disk of every topic with a `file`
  column and keeps what an ACTIVATION would load — per key, the newest
  instance under each existing snap's tag, and only that one (a save is
  untagged since 7.24.0, so that instance is the record the snap SHOT, and a
  node that moves on after the shot does not drag the snap's hold along). A snapshot holds bytes alive; **deleting the snap frees them**, and
  the delete is `delete-node` on its `__snaps__` row (there is no `delete-snap`
  command). A treedb with no snapshot does not walk. It also takes the
  **bytes with no row** — what an interrupted write leaves, and the `.tmp` of
  one that never reached its rename — because every other reader of the store
  goes through the rows. `.blobs` is the tranger's, so what counts as named is
  the union of every treedb's `__assets__`.
- **Deleting an asset node deletes its bytes.** Refused while a node links it,
  like any parent with children — and refused, `force` included, while a
  **snapshot** links it: `delete-node` on an `__assets__` row runs the same
  walk `gc-assets` does, because the ordinary tag guard never fires for an
  asset. `force` means "unlink the children", never "ignore what a snapshot
  needs", and no key of `options` skips the walk: the gc skips it through a
  private entry, not through anything the wire can spell. On a tranger that
  hosts more than one treedb, `__assets__` and its bytes are shared: both
  `gc-assets` and `delete-node` read every treedb's links, and an asset
  another treedb of the tranger links is refused, `force` or not.
- **A `file` column must be `['fkey','file']` on a `string`**, and that is
  checked in three places: at open (fatal), by `create-topic` with the yuno
  running (the topic is refused, the answer says why), and by the write path
  (the write is refused). A column that is `file` and not `fkey` would store
  its bytes into a column nothing links, and `gc-assets` would take them.

- ⚠️ **A non-master replica gets the index and not the bytes.** The watcher
  replicates a topic's files, so a replica has every row of `__assets__` and
  none of the `.blobs` behind them: `get-asset` answers *"asset has no bytes
  on disk"* there unless a web server in front is serving a copy of the blob
  directory. Getting the bytes to a replica is not solved — plan for it, or
  serve the assets from the master. The same holds for `export-db` /
  `import-db`: they carry the rows of `__assets__` and not the `.blobs`.

Serving the bytes to a browser is [`C_ASSETS`](#gclass-c-assets)' job.

---

(gclass-c-resource2)=
## C_RESOURCE2

Simple resource persistence — stores each resource as a flat JSON file.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `strict` | `bool` | Enable schema validation. |
| `json_desc` | `json` | Resource schema descriptor. |
| `persistent` | `bool` | Persist to disk. |
| `service` | `string` | Service name. |
| `database` | `string` | Database name. |

---

(gclass-c-assets)=
## C_ASSETS

The way **out** of the bytes a treedb keeps for its `file` columns: a signed
URL a web server checks by itself, or the bytes inline when there is no web
server in front. Storing is treedb's ([File columns](#treedb-file-columns)):
`file` columns, `__assets__`, and the `import-assets` / `gc-assets` commands
of `C_NODE`. This gclass only publishes what is stored, and is one command.

The asset id is the **sha256 of its content**, so a served URL means the
same bytes for ever and can be cached for ever. `get-asset` takes the asset
id and nothing else — not a node plus a column: keyed by node, the same URL
would mean different bytes the moment the node was relinked.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `treedb` | `pointer` | The `C_NODE` that owns the treedb. Set by the host; takes precedence over `treedb_service`. |
| `treedb_service` | `string` | Its service name, when the host did not build it itself. |
| `public_url` | `string` | URL prefix a web server serves the blobs from. Empty: `get-asset` answers inline. |
| `sign_secret` | `string` | Shared secret of the web server's `secure_link_md5`. Empty: `get-asset` answers inline. |
| `url_ttl` | `integer` | Seconds a signed URL stays valid. Default `900`. |

### Commands

| Command | Description |
|---------|-------------|
| `get-asset` | Return a signed URL, or the bytes inline. See below. Takes `asset_id`, **not** `id`. |

Gated by the `read` authz of the service.

**`get-asset` takes `asset_id`, not `id`.** `command-yuno` hands its whole kw
to `gobj_list_nodes()` as the filter that picks the yuno, so a parameter named
like a field of the yuno record becomes a filter on that field: an `id` of a
sha256 matches no yuno and the answer is *"Yuno not found"* — which names the
yuno and never the parameter. The bare `id` still works for a caller that
never crosses the agent.

### Two ways out to a browser, and the service picks

`get-asset` answers in one of two shapes:

```json
{"mode": "url",    "url": "/media/ab/cd/<id>.jpg?e=<expires>&s=<token>"}
{"mode": "inline", "content_type": "image/jpeg", "content64": "..."}
```

It signs a URL when `public_url` **and** `sign_secret` are both configured,
and answers inline when they are not. So the caller has **one** code path,
and a node with no web server in front of it still shows its images instead
of showing nothing.

The signed form reproduces, byte for byte, what this nginx block hashes.
**Do not use `/assets/`**: a Vite SPA on the same vhost already owns that
prefix for its content-hashed bundles, and the two locations would fight
over it. The `alias` is the treedb's own blob directory.

```nginx
location /media/ {
    secure_link      $arg_s,$arg_e;
    secure_link_md5  "$secure_link_expires$uri <sign_secret>";
    if ($secure_link = "")  { return 403; }   # bad signature
    if ($secure_link = "0") { return 410; }   # expired
    alias <store>/<realm>/treedb_<name>/.blobs/;
    expires max;    # the name IS the hash: it can never go stale
    access_log off;
}
```

The client address is deliberately **not** in the signature: it would tie the
URL to one IP and break every phone that changes network mid-session. The
short lifetime is what limits a leaked URL.

`secure_link` needs nginx built `--with-http_secure_link_module`. Yuneta's
nginx and openresty are, but a node still running an older build must serve
inline until its web server is replaced.

`tests/c/c_assets` covers the whole round trip: a record with its bytes
beside it through `update-node`, both doors (`content64` and a `gbuffer` of
two slices), `get-asset` inline and signed, `import-assets` with hostile
paths (`..`, absolute, a symlink out of the root), and `gc-assets`.
