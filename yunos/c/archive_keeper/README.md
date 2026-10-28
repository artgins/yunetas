# archive_keeper

Keeps the batches of timeranger2 archives that
[`archive_sender`](../archive_sender/README.md)s ship with the `native`
transport. It runs on the backup machine. Each channel is one sender, and each
sender has at most one batch in flight. The frames are described in the
README of archive_sender.

## Where the batches go

```
<backup_directory>/<node>/<database>/<topic>/<key>/<batch>/
<backup_directory>/<node>/<database>/<topic>/<key>/.part-<batch>/     one in flight
```

- A batch is written aside, in `.part-<batch>`. It is renamed to its name only
  when every file has the size the sender declared and is fsync'ed (the files,
  the directory, then the parent after the rename). Only then the keeper
  answers `stored`, and only that removes the batch at the sender.
- **A name without the `.` is a whole batch.** A reader of the backup (a tape
  job, an offsite copy) takes only those.
- A batch that is already there is answered `stored` at once. A sender that
  crashed between the answer and its removal ships the batch again, and nothing
  is written twice.
- A channel that closes takes its `.part-` with it. A `begin` of a batch whose
  `.part-` is there removes it first.

## What it refuses

Everything in a frame comes from the peer. These are refused with a WARNING,
the answer `refused` and the removal of the batch aside:

- a `node` or a name of the batch that is not one plain name (empty, starts
  with `.`, has a `/`, is longer than `NAME_MAX`);
- a batch that is not four such names, `<database>/<topic>/<key>/<batch>`;
- a node that is not in `allowed_nodes`, when the list is not empty;
- more files than `max_files`, a file name repeated, a size that is not an
  integer of 0 or more;
- data of a batch not begun, data out of its place (not at the end of what is
  written), data beyond the declared size, an `end` with a file not whole.

A frame whose header is not a json object drops the channel.

## Configuration

| Attribute | Default | Meaning |
|---|---|---|
| `backup_directory` | — | Absolute. Created if it is not there |
| `allowed_nodes` | `[]` | The node names whose batches are taken. Empty: any |
| `input_service` | `__input_side__` | The `C_IOGATE` whose channels are the senders |
| `max_files` | 10000 | The most files a batch may declare |

```json
{
    "global": {
        "archive_keeper.backup_directory": "/backups/yuneta",
        "archive_keeper.allowed_nodes": ["central", "controlador"],
        "__input_side__.__json_config_variables__": {
            "__input_url__": "tcps://0.0.0.0:1999",
            "__ssl_certificate__": "/yuneta/store/certs/backup.example.com.crt",
            "__ssl_certificate_key__": "/yuneta/store/certs/private/backup.example.com.key"
        }
    },
    "services": [
        {
            "name": "__input_side__",
            "gclass": "C_IOGATE",
            "autostart": false,
            "autoplay": false,
            "children": [
                {
                    "name": "input_server",
                    "gclass": "C_TCP_S",
                    "kw": {
                        "url": "(^^__input_url__^^)",
                        "crypto": {
                            "library": "(^^__tls_library__^^)",
                            "ssl_certificate": "(^^__ssl_certificate__^^)",
                            "ssl_certificate_key": "(^^__ssl_certificate_key__^^)"
                        },
                        "child_tree_filter": {
                            "kw": {
                                "__gclass_name__": "C_CHANNEL",
                                "__disabled__": false,
                                "connected": false
                            }
                        }
                    }
                }
            ],
            "[^^children^^]": {
                "__range__": 20,
                "__vars__": {},
                "__content__": {
                    "name": "input-(^^__range__^^)",
                    "gclass": "C_CHANNEL",
                    "children": [
                        {
                            "name": "input-(^^__range__^^)",
                            "gclass": "C_PROT_TCP4H",
                            "children": [
                                {
                                    "name": "input-(^^__range__^^)",
                                    "gclass": "C_TCP"
                                }
                            ]
                        }
                    ]
                }
            }
        }
    ]
}
```

`__range__` is the number of senders that can be connected at one time.

**Security.** The posture is host to host, the same as the data links of the
nodes: TLS encrypts, and the `node` a sender says is a claim, not a proof.
`allowed_nodes` stops an operator error, not an attacker. Put the keeper's
port where only the nodes can reach it.

## Commands and stats

| Command | Does |
|---|---|
| `list-sessions` | The batches being received, one per channel, with what each file has |

Stats: `batches_stored`, `bytes_stored`, `batches_already` (offered and already
here), `batches_refused`.

Traces: `batches` and `frames`, as in archive_sender.

## Tests

`tests/c/archive_shipping/` (see the README of archive_sender).
