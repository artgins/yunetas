# archive_sender

`archive_sender` ships the batches of a timeranger2 archive to a backup, and
then removes them. An archive is a directory where retention keeps the files
that it purges.

To use it:

1. Give the retention of the store an archive directory on the filesystem of
   the store (see Configuration below).
2. Run one `archive_sender` for each archive directory. Set `archive_directory`
   and `transport` in its configuration.
3. To see what waits, run `list-pending`, or read the stats `bytes_pending`
   and `oldest_pending_t`.

Retention can keep what it purges. `tranger2_archive_key_files()` does the same
purge as `tranger2_delete_key_files()`, and it leaves the purged files in a batch
of an archive directory (design: `kernel/c/timeranger2/DESIGN-retention.md`
§12). What happens to the batches is not the job of the store. It is the job of
this yuno.

```
store ──retention──> archive_directory ──archive_sender──> backup
        (tranger2_archive_key_files)        native ──> archive_keeper (another machine)
                                            rsync  ──> rsync destination
```

The node opens the connection to the backup. Nothing connects to the node, so
a sealed node can also ship.

## The archive it reads

```
<archive_directory>/
    .ready/<mark>                                   one empty file per batch
    <database>/<topic>/<key>/<upto>@<t>/            one batch per purge of a key
        <file_id>.json  <file_id>.md2  ...          the files the purge took
        topic_desc.json  topic_cols.json            the schema of the topic
        manifest.json                               what the batch holds
```

- A name that starts with `.` is not a batch. It is half built (`.new-*`), or
  it is one of the files of this yuno (`.ready/`, `.rsync.out`).
- The ready mark: when a batch is whole and its purge happened, the SDK
  writes an empty file in `.ready/`. Its name is the path of the batch, with
  `%` written `%25` and `/` written `%2F`, for example
  `events%2Fraw_events%2FDEV42%2F2026-10-01@1793164943`. The yuno watches that
  ONE directory (`C_FS`). It does not watch the tree, for two reasons: a
  `rename()` gives no fs event, and one watch for each key does not scale.
- The tree is the truth, and the mark is only a hint. When the yuno plays, it
  scans the whole tree. The scan finds a batch with no mark (a crash before the
  mark, or a mark that the SDK failed to write). The scan also runs when the
  watch lost events, and with the command `rescan`. The yuno removes a mark of
  a batch that is not there.
- Order: the oldest batch first (the `<t>` of its name). A batch that
  failed goes to the back of the queue.

## The two transports

`transport` selects one of them. They are equal in one point: the yuno removes
a batch here only when the backup has all of it.

### native: to an archive_keeper

The batches go to an `archive_keeper` (`yunos/c/archive_keeper/`) over the
`C_IOGATE` that `output_service` names:
`C_CHANNEL` → `C_PROT_TCP4H` → `C_TCP`, with TLS when the url is `tcps://`.
There is one batch in flight. Its data goes in frames of `chunk_size` bytes,
with at most `window` frames not yet acknowledged. Thus the yuno never holds a
whole file in memory.

The link is mutual TLS. The node shows its client certificate, and the keeper
takes a batch only when its `node` is the name of that certificate.

A frame is a compact json. A `data` frame adds a NUL byte and then the bytes of
the file.

| Frame | From | Fields | Answer |
|---|---|---|---|
| `begin` | sender | `node`, `batch` (`<database>/<topic>/<key>/<batch>`), `files` (`[{name, size}]`, `manifest.json` last) | `begin-ack`, or `refused` |
| `data` | sender | `batch`, `file` (index in `files`), `offset`, then NUL and the bytes. The files go in their order, each one whole before the next | `data-ack` (`offset` = the end of the bytes), or `refused` |
| `end` | sender | `batch`, `sha256` (one per file of `files`, in its order: 64 lowercase hex characters) | `stored`, when every file is whole, has its sha256, and is fsync'ed, or when the keeper already has this batch with this content. Else `refused` |
| `refused` | keeper | `batch`, `msg` | the batch fails here. The sender tries it again after `retry_period` |

```json
{"cmd": "begin", "node": "node-b", "batch": "events/raw_events/DEV42/2026-10-01@1793164943",
 "files": [{"name": "2026-10-01.json", "size": 3512018}, {"name": "2026-10-01.md2", "size": 288000},
           {"name": "topic_cols.json", "size": 612}, {"name": "topic_desc.json", "size": 455},
           {"name": "manifest.json", "size": 301}]}
{"cmd": "end", "batch": "events/raw_events/DEV42/2026-10-01@1793164943",
 "sha256": ["9f2c...", "41d0...", "e7a5...", "0b3e...", "c8f1..."]}
```

(The sha256 are 64 hex characters each, cut here.) The sender hashes each file
while it reads the file for its `data` frames (`sha256_update()` of gobj-c).
Thus the sha256 cost no additional read, but they are known only at the end.
That is why they go in the `end` and not in the `begin`.

A link that closes while a batch is in flight is not a failure of the batch.
When the link opens, the sender ships the batch again, whole. The keeper
removes what it got of it.

### rsync

`rsync_path` runs two times for each batch:

```
rsync -a --mkpath --protect-args --timeout=<rsync_timeout> <rsync_options...> --exclude=/manifest.json <batch>/ <dest>/<node>/<database>/<topic>/<key>/<batch>/
rsync -a --mkpath --protect-args --timeout=<rsync_timeout> <rsync_options...> <batch>/manifest.json   <dest>/<node>/<database>/<topic>/<key>/<batch>/
```

- `--protect-args`: the names of a batch are data (for example, a key is a
  device id). An old rsync (before 3.2.4) gives the remote path to the remote
  shell. With this flag, no shell sees the names.

- On the backup, a batch is whole when its `manifest.json` is there. rsync
  writes each file under a temporary name and renames it, and the manifest goes
  last. A reader of the backup must ignore a batch with no manifest.
- If the second run exits with 0, the yuno removes the batch here. Any other
  exit is a failure: a WARNING with the last 1 KB of the output of rsync
  (rsync writes it to `<archive_directory>/.rsync.out`).
- `rsync_destination` is any destination that rsync accepts: `user@host:dir` (ssh),
  `rsync://host/module/dir` (an rsync daemon), or a local directory.
- `--mkpath` needs rsync 3.2.3 or later at both ends (Debian 12 has 3.2.7,
  Rocky 9 has 3.2.3).
- The event loop polls a pidfd to watch the process. Nothing blocks, and the
  yuno continues its other work while rsync runs. A pause or a stop kills the
  rsync that is running. The yuno ships the batch again later.
- rsync over ssh needs a key, and the backup must accept it. For a node that
  is sealed, or that will be sealed, prefer `native` or an rsync daemon.

## When the backup is not there

A batch takes the disk space that its files took in the store, because they
are hard links and the store link is gone. Thus an archive that is never
shipped is a store that is never purged.

`max_archive_bytes` (0: no limit) is the limit. Above it, the yuno removes the
oldest batches WITHOUT shipping them, but never the one in flight. Each one
gets a WARNING (*"Batch removed WITHOUT being shipped: the archive is over
max_archive_bytes"*). The node stays alive, and the backup loses its oldest
part, not the newest. The stats `bytes_pending` and `oldest_pending_t` show
it before it happens.

## Configuration

| Attribute | Default | Meaning |
|---|---|---|
| `archive_directory` | — | Absolute. The `archive_directory` given to `tranger2_archive_key_files()` |
| `transport` | `native` | `native` or `rsync` |
| `node_name` | the host name | The first directory of the batches of this node in the backup. native: the `cn` (or a DNS name) of the client certificate of the node |
| `output_service` | `__output_side__` | native: the `C_IOGATE` to the keeper |
| `chunk_size` | 262144 | native: bytes of a file in one frame |
| `window` | 4 | native: data frames not yet acknowledged |
| `rsync_path` | `/usr/bin/rsync` | rsync: the program |
| `rsync_destination` | — | rsync: where the batches go |
| `rsync_options` | `[]` | rsync: more arguments, before the paths |
| `rsync_timeout` | 300 | rsync: `--timeout`, seconds with no I/O |
| `max_archive_bytes` | 0 | Pending bytes above which the yuno drops the oldest batches. 0: no limit. Writable |
| `retry_period` | 60 | Seconds with no new try after a batch failed. Writable |

Native, in the batch configuration of the node:

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
from the CA of the nodes that the keeper trusts. Its `cn` must be the
`node_name`, because the keeper refuses the batches of another name. To make
one, see the README of archive_keeper.

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
for example `raw_events_archive_directory` in the configuration of an application.

## Crash safety

| Crash | What happens |
|---|---|
| Before the batch is whole (`.new-*`) | Not a batch: the next archive of the key removes it. Its purge did not happen |
| After the batch, before its mark | The scan finds it when the yuno plays |
| While shipping (native) | The keeper removes what it got when the channel closes. The sender ships the batch again |
| While shipping (rsync) | The next rsync completes the copy. A batch with no manifest at the destination is not whole |
| After the keeper said `stored`, before the removal here | The sender ships the batch again, whole. The keeper compares it with the one that it has, and answers `stored` at its `end` |
| Between the batch and the base of the store (SDK) | The purge did not happen, and retention archives the files again later. Thus the backup gets them two times. Until that purge, the first batch holds hard links to live files. A late append, a user flag, or a deleted instance changes them after the manifest of the batch. Thus the two copies can differ. Take a file id (`manifest.json`, `files`) from the batch with the latest `@<t>` |

## Commands

| Command | Does |
|---|---|
| `list-pending limit=100` | The batches waiting, the oldest first, with `t`, `bytes` and `failures` |
| `rescan` | Scan the archive tree again, and ship what it finds |

## Stats

`batches_pending`, `bytes_pending`, `oldest_pending_t`, `batches_shipped`,
`bytes_shipped`, `batches_failed`, `batches_dropped`, `bytes_dropped`.

## Traces

The trace levels are `batches` (found, shipped, failed, dropped, each rsync run)
and `frames` (each frame to and from the keeper):

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=C_ARCHIVE_SENDER set=1 level=batches'
```

## Tests

`tests/c/archive_shipping/` runs both transports in one process: a native
sender and a keeper over `tcp://127.0.0.1:7755`, and an rsync sender to a local
directory. It tests these cases:

- The batches that the scan finds, and one that its mark tells.
- A batch that the keeper already has.
- A batch of the same name, planted in the backup first, with the same files
  and sizes but other bytes. The keeper refuses it (the sha256 differ) and
  keeps the planted one. The sender keeps its own.
- A batch dropped over `max_archive_bytes`.
- What arrives is whole, and nothing is left in the archives.
