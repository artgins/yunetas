# archive_sender

Ships the batches of a timeranger2 **archive** to a backup, and removes them.

Retention can keep what it purges: `tranger2_archive_key_files()` does the same
purge as `tranger2_delete_key_files()` and leaves the purged files in a batch
of an archive directory (design: `kernel/c/timeranger2/DESIGN-retention.md`
§12). What happens to the batches is not the job of the store. It is the job of
this yuno.

```
store ──retention──> archive_directory ──archive_sender──> backup
        (tranger2_archive_key_files)        native ──> archive_keeper (another machine)
                                            rsync  ──> rsync destination
```

One `archive_sender` per archive directory. The node opens the connection to
the backup. Nothing connects to the node, so a sealed node can ship too.

## The archive it reads

```
<archive_directory>/
    .ready/<mark>                                   one empty file per batch
    <database>/<topic>/<key>/<upto>@<t>/            one batch per purge of a key
        <file_id>.json  <file_id>.md2  ...          the files the purge took
        topic_desc.json  topic_cols.json            the schema of the topic
        manifest.json                               what the batch holds
```

- A name that starts with `.` is not a batch: it is half built (`.new-*`), or
  it is one of this yuno's files (`.ready/`, `.rsync.out`).
- **The ready mark.** When a batch is whole and its purge happened, the SDK
  writes an empty file in `.ready/`. Its name is the path of the batch, with
  `%` written `%25` and `/` written `%2F`, for example
  `events%2Fraw_events%2FDEV42%2F2026-10-01@1793164943`. The yuno watches that
  ONE directory (`C_FS`). It does not watch the tree, for two reasons: a
  `rename()` gives no fs event, and one watch per key does not scale.
- **The tree is the truth, the mark is a hint.** When the yuno plays, it scans
  the whole tree. A batch with no mark (a crash before the mark, a mark that
  could not be written) is found there. The scan also runs when the watch lost
  events, and with the command `rescan`. A mark of a batch that is not there is
  removed.
- **Order:** the oldest batch first (the `<t>` of its name). A batch that
  failed goes to the back of the queue.

## The two transports

`transport` selects one of them. They are equal in one point: a batch is
removed here only when the backup has all of it.

### native: to an archive_keeper

The batches go to an `archive_keeper` (`yunos/c/archive_keeper/`) over the
`C_IOGATE` named by `output_service`:
`C_CHANNEL` → `C_PROT_TCP4H` → `C_TCP`, with TLS when the url is `tcps://`.
There is one batch in flight. Its data goes `chunk_size` bytes per frame, with
at most `window` frames not yet acknowledged, so the yuno never holds a whole
file in memory.

The link is mutual TLS: the node presents its client certificate, and the
keeper takes a batch only when its `node` is the name of that certificate.

A frame is a compact json. A `data` frame adds a NUL byte and then the bytes of
the file.

| Frame | From | Fields | Answer |
|---|---|---|---|
| `begin` | sender | `node`, `batch` (`<database>/<topic>/<key>/<batch>`), `files` (`[{name, size}]`, `manifest.json` last) | `begin-ack`; `refused` |
| `data` | sender | `batch`, `file` (index in `files`), `offset`, then NUL and the bytes. The files go in their order, each one whole before the next | `data-ack` (`offset` = the end of the bytes); `refused` |
| `end` | sender | `batch`, `sha256` (one per file of `files`, in its order: 64 lowercase hex characters) | `stored` when every file is whole, has its sha256 and is fsync'ed, or when the keeper has this batch already with this content; `refused` |
| `refused` | keeper | `batch`, `msg` | the batch fails here: it is tried again after `retry_period` |

```json
{"cmd": "begin", "node": "node-b", "batch": "events/raw_events/DEV42/2026-10-01@1793164943",
 "files": [{"name": "2026-10-01.json", "size": 3512018}, {"name": "2026-10-01.md2", "size": 288000},
           {"name": "topic_cols.json", "size": 612}, {"name": "topic_desc.json", "size": 455},
           {"name": "manifest.json", "size": 301}]}
{"cmd": "end", "batch": "events/raw_events/DEV42/2026-10-01@1793164943",
 "sha256": ["9f2c...", "41d0...", "e7a5...", "0b3e...", "c8f1..."]}
```

(The sha256 are 64 hex characters each, cut here.) Each file is hashed while
it is read for its `data` frames (`sha256_update()` of gobj-c), so the
sha256 cost no read more, and they are known only at the end: that is
why they go in the `end` and not in the `begin`.

A link that closes while a batch is in flight is not a failure of the batch.
The batch is shipped again, whole, when the link opens. The keeper removes what
it got of it.

### rsync

`rsync_path` runs two times per batch:

```
rsync -a --mkpath --protect-args --timeout=<rsync_timeout> <rsync_options...> --exclude=/manifest.json <batch>/ <dest>/<node>/<database>/<topic>/<key>/<batch>/
rsync -a --mkpath --protect-args --timeout=<rsync_timeout> <rsync_options...> <batch>/manifest.json   <dest>/<node>/<database>/<topic>/<key>/<batch>/
```

- **`--protect-args`**: the names of a batch come from data (a key is a
  device id, say), and an old rsync (before 3.2.4) gives the remote path to
  the remote shell. With it, no shell sees them.

- **On the backup, a batch is whole when its `manifest.json` is there.** rsync
  writes each file under a temporary name and renames it, and the manifest goes
  last. A reader of the backup must ignore a batch with no manifest.
- The exit 0 of the second run removes the batch here. Any other exit is a
  failure: a WARNING with the last 1 KB of what rsync said (it writes to
  `<archive_directory>/.rsync.out`).
- `rsync_destination` is anything rsync takes: `user@host:dir` (ssh),
  `rsync://host/module/dir` (an rsync daemon), or a local directory.
- `--mkpath` needs rsync 3.2.3 or later at both ends (Debian 12 has 3.2.7,
  Rocky 9 has 3.2.3).
- The process is watched with a pidfd that the event loop polls. Nothing
  blocks, and the yuno goes on with its other work while rsync runs. A pause or
  a stop kills the rsync that is running. The batch is shipped again later.
- rsync over ssh needs a key, and the backup must accept it. For a node that
  is sealed, or will be, prefer `native` or an rsync daemon.

## When the backup is not there

A batch takes on disk what its files took in the store: they are hard links,
and the store link is gone. So an archive that is never shipped is a store
that is never purged.

`max_archive_bytes` (0: no limit) is the limit. Above it, the **oldest**
batches are removed WITHOUT being shipped, never the one in flight. Each one
gets a WARNING (*"Batch removed WITHOUT being shipped: the archive is over
max_archive_bytes"*). The node stays alive, and it is the oldest part of the
backup that is lost, not the newest. The stats `bytes_pending` and
`oldest_pending_t` show it before it happens.

## Configuration

| Attribute | Default | Meaning |
|---|---|---|
| `archive_directory` | — | Absolute. The `archive_directory` given to `tranger2_archive_key_files()` |
| `transport` | `native` | `native` or `rsync` |
| `node_name` | the host name | The first directory of this node's batches in the backup. native: the `cn` (or a DNS name) of the node's client certificate |
| `output_service` | `__output_side__` | native: the `C_IOGATE` to the keeper |
| `chunk_size` | 262144 | native: bytes of a file in one frame |
| `window` | 4 | native: data frames not yet acknowledged |
| `rsync_path` | `/usr/bin/rsync` | rsync: the program |
| `rsync_destination` | — | rsync: where the batches go |
| `rsync_options` | `[]` | rsync: more arguments, before the paths |
| `rsync_timeout` | 300 | rsync: `--timeout`, seconds with no I/O |
| `max_archive_bytes` | 0 | Bytes pending above which the oldest batches are dropped. 0: no limit. Writable |
| `retry_period` | 60 | Seconds with no try after a batch failed. Writable |

Native, in the batch config of the node:

```json
{
    "global": {
        "archive_sender.archive_directory": "/yuneta/archive",
        "archive_sender.node_name": "node-b",
        "archive_sender.max_archive_bytes": 200000000000,
        "__output_side__.__json_config_variables__": {
            "__output_url__": "tcps://backup.example.com:1999"
        }
    },
    "services": [
        {
            "name": "__output_side__",
            "gclass": "C_IOGATE",
            "autostart": false,
            "autoplay": false,
            "children": [
                {
                    "name": "output",
                    "gclass": "C_CHANNEL",
                    "children": [
                        {
                            "name": "output",
                            "gclass": "C_PROT_TCP4H",
                            "children": [
                                {
                                    "name": "output",
                                    "gclass": "C_TCP",
                                    "kw": {
                                        "url": "(^^__output_url__^^)",
                                        "crypto": {
                                            "library": "(^^__tls_library__^^)",
                                            "ssl_verify_mode": "required",
                                            "ssl_use_system_ca": true,
                                            "ssl_certificate": "/yuneta/store/certs/node-b.crt",
                                            "ssl_certificate_key": "/yuneta/store/certs/private/node-b.key"
                                        }
                                    }
                                }
                            ]
                        }
                    ]
                }
            ]
        }
    ]
}
```

The `ssl_certificate` of the `C_TCP` is the client certificate of the node,
from the CA of the nodes that the keeper trusts: its `cn` must be the
`node_name` (the keeper refuses the batches of another name). How to make
one: the README of archive_keeper.

rsync over ssh:

```json
{
    "global": {
        "archive_sender.archive_directory": "/yuneta/archive",
        "archive_sender.transport": "rsync",
        "archive_sender.rsync_destination": "backup@backup.example.com:/backups",
        "archive_sender.rsync_options": ["-e", "ssh -i /yuneta/store/certs/private/backup_key"]
    }
}
```

The archive directory must be on the filesystem of the store (the batches are
hard links). The retention of the owner of the store names the same directory,
for example `raw_events_archive_directory` in an application's config.

## Crash safety

| Crash | What happens |
|---|---|
| Before the batch is whole (`.new-*`) | Not a batch: the next archive of the key removes it. Its purge did not happen |
| After the batch, before its mark | The scan finds it when the yuno plays |
| While shipping (native) | The keeper removes what it got when the channel closes. The batch is shipped again |
| While shipping (rsync) | The next rsync completes the copy. A batch with no manifest at the destination is not whole |
| After the keeper said `stored`, before the removal here | The batch is shipped again, whole; the keeper compares it with the one it has and answers `stored` at its `end` |
| Between the batch and the base of the store (SDK) | The purge did not happen, and the files are archived again later: the backup gets them twice. Until that purge the first batch holds hard links to live files, which a late append, a user flag or a deleted instance changes after its manifest, so the two copies can differ: take a file id (`manifest.json`, `files`) from the batch with the latest `@<t>` |

## Commands

| Command | Does |
|---|---|
| `list-pending limit=100` | The batches waiting, the oldest first, with `t`, `bytes` and `failures` |
| `rescan` | Scan the archive tree again, and ship what it finds |

## Stats

`batches_pending`, `bytes_pending`, `oldest_pending_t`, `batches_shipped`,
`bytes_shipped`, `batches_failed`, `batches_dropped`, `bytes_dropped`.

## Traces

`batches` (found, shipped, failed, dropped, each rsync run) and `frames` (each
frame to and from the keeper):

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=C_ARCHIVE_SENDER set=1 level=batches'
```

## Tests

`tests/c/archive_shipping/` runs both transports in one process: a native
sender and a keeper over `tcp://127.0.0.1:7755`, and an rsync sender to a local
directory. It covers:

- the batches found by the scan, and one told by its mark;
- a batch the keeper already has;
- a batch of the same name planted in the backup first, with the same files
  and sizes and other bytes: the keeper refuses it (the sha256 differ) and
  keeps the planted one, and the sender keeps its own;
- a batch dropped over `max_archive_bytes`;
- what arrives is whole, and nothing is left in the archives.
