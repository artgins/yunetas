# archive_keeper

`archive_keeper` keeps the batches of timeranger2 archives that
`archive_sender`s (`yunos/c/archive_sender/`) ship with the `native`
transport. It runs on the backup machine. Each channel is one sender, and each
sender has at most one batch in flight. The README of archive_sender describes
the frames.

To run it, give it a `backup_directory`, the `allowed_nodes`, and a TLS
listener that asks the senders for a certificate of the CA of the nodes. The
section "Configuration" below has a full example.

## Where the batches go

```
<backup_directory>/<node>/<database>/<topic>/<key>/<batch>/
<backup_directory>/<node>/<database>/<topic>/<key>/.part-<batch>/     one in flight
```

- The keeper writes a batch aside, in `.part-<batch>`, and hashes it as it
  arrives (the files in their order, each one whole before the next). It gives
  the batch its name only when every file has the size of the `begin` and the
  sha256 of the `end`. It also does an fsync of the files, of the directory,
  and, after the rename, of every directory above it up to `backup_directory`.
  The reason is that the `begin` can have created the levels of the node, the
  database, the topic and the key. Only then does the keeper answer `stored`,
  and only that answer removes the batch at the sender.
- A batch is in flight on one channel at a time, so only one session writes a
  `.part-`. The keeper refuses a `begin` of a batch that another channel
  receives (*"the batch is in flight on another channel"*). Two senders that
  claim the same node, or a sender that reconnected, try again later. Until
  7.29.1, the second `begin` removed the `.part-` of the first channel and made
  its own. Then it was possible that the keeper answered `stored` to the first channel for files
  that it did not write. Its writes went to the removed files, and the `end`
  compared the sha256 that it computed in memory. The exception is a channel
  that sent no frame for `stale_session_timeout`. Without it, a sender gone
  without a close (a half-open connection, which nothing else ends) will hold
  its batch for ever. The new `begin` takes over its batch, the keeper removes
  its `.part-` and drops the channel (WARNING *"A batch in flight on a channel
  without frames for stale_session_timeout is taken over by another
  channel"*).
- A name without the `.` is a whole batch. A reader of the backup (a tape
  job, an offsite copy) takes only those.
- If a batch is already there, the keeper answers `stored` when its `end`
  comes, if the batch has the files, the sizes and the content of what
  arrived. The keeper hashes each of its files again and compares it with the
  sha256 of the `end`. A sender that crashed between the answer and its
  removal ships the batch again, whole (the sender knows the sha256 only at
  the end). The keeper then removes the copy aside, so nothing is written
  twice in the backup. If a batch of the same name has other content, the
  keeper keeps it as it is and refuses the one that arrived (WARNING *"A batch
  of this name is here with other content"*). So a peer that wrote a batch of
  that name first cannot make the real sender remove its own, even with the
  same names and sizes. The sender keeps the batch and tries again later, so
  an operator sees it in its `list-pending`.
- A channel that closes takes its `.part-` with it. If the `.part-` of a batch
  is there when its `begin` comes, a crash left it and no channel receives it.
  The `begin` removes it first.
- The files of a `begin` must fit in the free space of the filesystem of
  `backup_directory`, together with what the other batches in flight still
  must write. The keeper reserves those bytes. It refuses a batch that does
  not fit at its `begin`, not when the disk is full.
- Every `begin` drops every channel that sent nothing for
  `stale_session_timeout`, whatever its batch. The space that its batch
  reserved is free again (WARNING *"A channel without frames for
  stale_session_timeout is dropped"*, with the node, the batch and the bytes
  that it still had to write). Up to 7.29.2, only a `begin` of the same batch
  dropped it. So a half-open channel whose batch nobody began again (removed
  at the sender, or of a retired node) reserved its bytes for ever. Also, a
  peer that declared the whole free space and sent nothing made every `begin`
  of every node *"does not fit"*.
- A channel that keeps sending, however slowly, keeps its batch and its
  reservation. A whole batch has no deadline, because a slow link can need
  hours for a big one. Such a channel is an allowed sender (with a
  certificate, of its own node only). That sender can fill the backup with
  real batches too.

## What it refuses

The peer writes everything in a frame. The keeper refuses the items below
with a WARNING, the answer `refused`, and the removal of the batch aside:

- A `node` or a name of the batch that is not one plain name (empty, starts
  with `.`, has a `/`, is longer than `NAME_MAX`).
- A batch that is not four such names, `<database>/<topic>/<key>/<batch>`.
- A node that is not in `allowed_nodes`.
- With `require_client_certificate` (the default): a channel without a
  verified client certificate, or a node that is not the `cn` nor a DNS name
  of that certificate (WARNING *"A sender names a node that is not the name
  of its client certificate"*, with the `cn` and the `subject`).
- More files than `max_files` (the answer gives the count and the limit), a
  file name repeated, a size that is not an integer of 0 or more.
- A batch in flight on another channel (above), or a batch that does not fit
  in the free space.
- Data of a batch not begun, data out of its place (not at the end of what is
  written), data of a file before the files before it are whole, data beyond
  the declared size.
- An `end` with a file not whole, without a `sha256` for each file, or with
  one (64 lowercase hex characters) that is not the sha256 of what arrived.

The keeper refuses a batch once. It also answers `refused` to the frames of
the batch that the sender already sent (its window), but it does not log or
count them again.

A frame whose header is not a json object drops the channel.

## Configuration

| Attribute | Default | Meaning |
|---|---|---|
| `backup_directory` | — | Absolute. Created if it is not there |
| `allowed_nodes` | `[]` | The node names whose batches the keeper takes. `["*"]` takes any node. Required: if it is empty, the yuno does not start (ERROR) |
| `input_service` | `__input_side__` | The `C_IOGATE` whose channels are the senders |
| `max_files` | 100000 | The most files that a batch can declare. The first archive of an hourly topic holds two files per hour (`.json` and `.md2`): 100000 is more than five years. Each file in flight costs the keeper some hundreds of bytes of memory |
| `stale_session_timeout` | 60000 | Milliseconds without a frame. After them, the next `begin` of any channel drops the channel. That `begin` takes over its batch in flight (if the `begin` is of that batch), and the space that it reserved is free again. `0`: never |
| `require_client_certificate` | `true` | The node of a batch must be the `cn` or a DNS name of the verified client certificate of its channel (mutual TLS). `false`: the node is the claim of the sender, said on purpose |

```json
{
    "global": {
        "archive_keeper.backup_directory": "/backups/yuneta",
        "archive_keeper.allowed_nodes": ["node-b", "node-a"],
        "__input_side__.__json_config_variables__": {
            "__input_url__": "tcps://0.0.0.0:1999",
            "__ssl_certificate__": "/yuneta/store/certs/backup.example.com.crt",
            "__ssl_certificate_key__": "/yuneta/store/certs/private/backup.example.com.key",
            "__nodes_ca__": "/yuneta/store/certs/nodes_ca.pem"
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
                            "ssl_certificate_key": "(^^__ssl_certificate_key__^^)",
                            "ssl_trusted_certificate": "(^^__nodes_ca__^^)",
                            "ssl_verify_mode": "required"
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

Security: the node of a batch is bound to the client certificate of its
channel (mutual TLS):

- The `C_TCP_S` asks every sender for a certificate of `ssl_trusted_certificate`
  (the CA of the nodes), and `"ssl_verify_mode": "required"` refuses a sender
  without one in the handshake.
- `C_TCP` gives the certificate in its `EV_CONNECTED` (`peer_cert`), and it
  reaches the keeper with the `EV_ON_OPEN` of the channel. The `node` of each
  `begin` must be its `cn`, or one of its DNS names (`subjectAltName`),
  exactly. So a node cannot write, or make fail, the batches of another.
- `require_client_certificate` makes the keeper test it too. The keeper
  refuses every batch of a channel without a verified certificate (a
  `C_TCP_S` left in `"optional"`, or without a CA).
- `allowed_nodes` stays. It tells which nodes this backup takes, among those
  that the CA certifies. You must set it (fail closed: `["*"]` says "any node"
  on purpose).

The CA of the nodes is a private one. A public CA does not certify a node
name, and Let's Encrypt no longer issues certificates for client
authentication. Each node gets a certificate with its `node_name` as `cn`,
and `extendedKeyUsage=clientAuth`. For example, with the openssl command:

```bash
# Once, on the backup machine: the CA of the nodes
openssl req -x509 -newkey rsa:3072 -nodes -sha256 -days 3650 \
    -keyout nodes_ca.key -out nodes_ca.pem -subj '/CN=Yuneta nodes CA' \
    -addext 'basicConstraints=critical,CA:TRUE' \
    -addext 'keyUsage=critical,keyCertSign,cRLSign'

# For each node (here "node-b"): its key, and its certificate
openssl req -newkey rsa:3072 -nodes -sha256 -keyout node-b.key -out node-b.csr -subj '/CN=node-b'
printf 'basicConstraints=CA:FALSE\nextendedKeyUsage=clientAuth\n' > client.ext
openssl x509 -req -in node-b.csr -CA nodes_ca.pem -CAkey nodes_ca.key -CAcreateserial \
    -days 825 -sha256 -extfile client.ext -out node-b.crt
```

Even so, put the port of the keeper where only the nodes can reach it.

## Commands and stats

| Command | Does |
|---|---|
| `list-sessions` | The batches that the keeper receives, one per channel, with what each file has |

Stats: `batches_stored`, `bytes_stored`, `batches_already` (offered and already
here), `batches_refused` (batches, not frames: once per batch refused).

Traces: `batches` and `frames`, as in archive_sender.

## Tests

`tests/c/archive_shipping/` has two tests:

- `test1` runs the two yunos end to end (see the README of archive_sender).
- `test2` runs the keeper against a fake sender that writes the frames by
  hand. It sends a forged sha256, too few of them, sha256s that are not
  lowercase hex, data out of the order of the files (with a frame of it
  already on its way, which the keeper does not refuse twice), an empty batch,
  and a batch of a kept name with other bytes. The keeper refuses each one,
  and nothing of it stays in the backup. Over two links, the keeper refuses to
  the second link a batch in flight on the first, and stores it with the
  bytes of the first. If a link goes silent, the other link takes over its
  batch.
