# archive_keeper

Keeps the batches of timeranger2 archives that
`archive_sender`s (`yunos/c/archive_sender/`) ship with the `native`
transport. It runs on the backup machine. Each channel is one sender, and each
sender has at most one batch in flight. The frames are described in the
README of archive_sender.

## Where the batches go

```
<backup_directory>/<node>/<database>/<topic>/<key>/<batch>/
<backup_directory>/<node>/<database>/<topic>/<key>/.part-<batch>/     one in flight
```

- A batch is written aside, in `.part-<batch>`, and hashed as it arrives (the
  files in their order, each one whole before the next). It is renamed to its
  name only when every file has the size of the `begin` and the sha256 of the
  `end`, and is fsync'ed (the files, the directory, then the parent after the
  rename). Only then the keeper answers `stored`, and only that removes the
  batch at the sender.
- **A name without the `.` is a whole batch.** A reader of the backup (a tape
  job, an offsite copy) takes only those.
- A batch that is already there is answered `stored` when its `end` comes,
  **if it has the files, the sizes and the content of what arrived**: each of
  its files is hashed again and compared with the sha256 of the `end`. A
  sender that crashed between the answer and its removal ships the batch
  again, whole (the sender knows the sha256 only at the end), and the copy
  aside is removed: nothing is written twice in the backup. A batch of the
  same name with other content is kept as it is, and the one that arrived is
  refused (WARNING *"A batch of this name is here with other content"*): a
  peer that wrote a batch of that name first cannot make the real sender
  remove its own, even with the same names and sizes. The sender keeps the
  batch and tries again later, so an operator sees it in its `list-pending`.
- A channel that closes takes its `.part-` with it. A `begin` of a batch whose
  `.part-` is there removes it first.

## What it refuses

Everything in a frame comes from the peer. These are refused with a WARNING,
the answer `refused` and the removal of the batch aside:

- a `node` or a name of the batch that is not one plain name (empty, starts
  with `.`, has a `/`, is longer than `NAME_MAX`);
- a batch that is not four such names, `<database>/<topic>/<key>/<batch>`;
- a node that is not in `allowed_nodes`;
- more files than `max_files`, a file name repeated, a size that is not an
  integer of 0 or more;
- data of a batch not begun, data out of its place (not at the end of what is
  written), data of a file before the files before it are whole, data beyond
  the declared size;
- an `end` with a file not whole, without a `sha256` for each file, or with
  one (64 lowercase hex characters) that is not the sha256 of what arrived.

A frame whose header is not a json object drops the channel.

## Configuration

| Attribute | Default | Meaning |
|---|---|---|
| `backup_directory` | — | Absolute. Created if it is not there |
| `allowed_nodes` | `[]` | The node names whose batches are taken; `["*"]` takes any node. Required: empty, the yuno does not start (ERROR) |
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
`allowed_nodes` stops an operator error, not an attacker, and it must be set
(fail closed: `["*"]` says "any node" on purpose). Put the keeper's port where
only the nodes can reach it. Not done yet:
binding the node to the client certificate of a mutual TLS, so that one node
cannot write the batches of another. Without it, a peer that can reach the port
and names a node of `allowed_nodes` can store batches under that node, and
can make one batch of the real node fail (it stays at the real sender): it
cannot make the real sender remove one.

## Commands and stats

| Command | Does |
|---|---|
| `list-sessions` | The batches being received, one per channel, with what each file has |

Stats: `batches_stored`, `bytes_stored`, `batches_already` (offered and already
here), `batches_refused`.

Traces: `batches` and `frames`, as in archive_sender.

## Tests

`tests/c/archive_shipping/` (see the README of archive_sender).
