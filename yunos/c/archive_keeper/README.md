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
  `end`, and is fsync'ed (the files, the directory, then after the rename
  every directory above it up to `backup_directory`: the `begin` may have
  created the levels of the node, the database, the topic and the key). Only
  then the keeper answers `stored`, and only that removes the batch at the
  sender.
- **A batch is in flight on one channel at a time**, so a `.part-` is written
  by one session only. A `begin` of a batch that another channel is receiving
  is refused (*"the batch is in flight on another channel"*): two senders that
  claim the same node, or a sender that reconnected, try again later. Until
  7.29.1 the second `begin` removed the first channel's `.part-` and made its
  own, and the first channel could be answered `stored` for files it had not
  written (its writes went to the removed files, and the `end` checked the
  sha256 it had computed in memory). The exception is a channel that has sent
  no frame for `stale_session_timeout`: a sender gone without a close (a
  half-open connection, which nothing else ends) would hold its batch for
  ever. Its batch is taken over by the new `begin`, its `.part-` removed and
  the channel dropped (WARNING *"A batch in flight on a channel without frames
  for stale_session_timeout is taken over by another channel"*).
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
  `.part-` is there (left by a crash: no channel is receiving it) removes it
  first.
- **The files of a `begin` must fit** in the free space of the filesystem of
  `backup_directory`, together with what the other batches in flight still
  have to write: those bytes are reserved. A batch that does not fit is
  refused at its `begin`, not when the disk is full.
- **Every `begin` drops every channel silent for `stale_session_timeout`**,
  whatever its batch, and the space its batch reserved is free again (WARNING
  *"A channel without frames for stale_session_timeout is dropped"*, with the
  node, the batch and the bytes it still had to write). Up to 7.29.2 only a
  `begin` of the same batch dropped it: a half-open channel whose batch was
  never begun again (removed at the sender, or of a retired node) reserved its
  bytes for ever, and a peer that declared the whole free space and sent
  nothing made every `begin` of every node *"does not fit"*.
- A channel that keeps sending, however slowly, keeps its batch and its
  reservation. There is no deadline for a whole batch: a slow link may need
  hours for a big one. Such a channel is an allowed sender (with a
  certificate, of its own node only), which could fill the backup with real
  batches as well.

## What it refuses

Everything in a frame comes from the peer. These are refused with a WARNING,
the answer `refused` and the removal of the batch aside:

- a `node` or a name of the batch that is not one plain name (empty, starts
  with `.`, has a `/`, is longer than `NAME_MAX`);
- a batch that is not four such names, `<database>/<topic>/<key>/<batch>`;
- a node that is not in `allowed_nodes`;
- with `require_client_certificate` (the default): a channel without a
  verified client certificate, or a node that is not the `cn` nor a DNS name
  of that certificate (WARNING *"A sender names a node that is not the name
  of its client certificate"*, with the `cn` and the `subject`);
- more files than `max_files` (the answer says how many and the limit), a file
  name repeated, a size that is not an integer of 0 or more;
- a batch in flight on another channel (above), or one that does not fit in
  the free space;
- data of a batch not begun, data out of its place (not at the end of what is
  written), data of a file before the files before it are whole, data beyond
  the declared size;
- an `end` with a file not whole, without a `sha256` for each file, or with
  one (64 lowercase hex characters) that is not the sha256 of what arrived.

A batch is refused once: the frames of it that the sender had already sent
(its window) are answered `refused` too, but not logged nor counted again.

A frame whose header is not a json object drops the channel.

## Configuration

| Attribute | Default | Meaning |
|---|---|---|
| `backup_directory` | — | Absolute. Created if it is not there |
| `allowed_nodes` | `[]` | The node names whose batches are taken; `["*"]` takes any node. Required: empty, the yuno does not start (ERROR) |
| `input_service` | `__input_side__` | The `C_IOGATE` whose channels are the senders |
| `max_files` | 100000 | The most files a batch may declare. The first archive of an hourly topic holds two files per hour (`.json` and `.md2`): 100000 is more than five years. Each file in flight costs the keeper some hundreds of bytes of memory |
| `stale_session_timeout` | 60000 | Milliseconds without a frame after which a channel is dropped at the next `begin` of any channel: its batch in flight is taken over (if that `begin` is of it) and the space it reserved is free again. `0`: never |
| `require_client_certificate` | `true` | The node of a batch must be the `cn` or a DNS name of the verified client certificate of its channel (mutual TLS). `false`: the node is the claim of the sender, said on purpose |

```json
{
    "global": {
        "archive_keeper.backup_directory": "/backups/yuneta",
        "archive_keeper.allowed_nodes": ["central", "edge"],
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

**Security.** The node of a batch is bound to the client certificate of its
channel (mutual TLS):

- The `C_TCP_S` asks every sender for a certificate of `ssl_trusted_certificate`
  (the CA of the nodes), and `"ssl_verify_mode": "required"` refuses a sender
  without one in the handshake.
- `C_TCP` gives the certificate in its `EV_CONNECTED` (`peer_cert`), and it
  reaches the keeper with the `EV_ON_OPEN` of the channel. The `node` of each
  `begin` must be its `cn`, or one of its DNS names (`subjectAltName`),
  exactly. So a node cannot write, or make fail, the batches of another.
- `require_client_certificate` checks it in the keeper too: a channel
  without a verified certificate (a `C_TCP_S` left in `"optional"`, or without
  a CA) is refused every batch.
- `allowed_nodes` stays: it says which nodes this backup takes, among those
  the CA certifies. It must be set (fail closed: `["*"]` says "any node" on
  purpose).

The CA of the nodes is a private one: a public CA does not certify a node
name, and Let's Encrypt no longer issues certificates for client
authentication. Each node gets a certificate with its `node_name` as `cn`,
and `extendedKeyUsage=clientAuth`. For example, with the openssl command:

```bash
# Once, on the backup machine: the CA of the nodes
openssl req -x509 -newkey rsa:3072 -nodes -sha256 -days 3650 \
    -keyout nodes_ca.key -out nodes_ca.pem -subj '/CN=Yuneta nodes CA' \
    -addext 'basicConstraints=critical,CA:TRUE' \
    -addext 'keyUsage=critical,keyCertSign,cRLSign'

# For each node (here "central"): its key, and its certificate
openssl req -newkey rsa:3072 -nodes -sha256 -keyout central.key -out central.csr -subj '/CN=central'
printf 'basicConstraints=CA:FALSE\nextendedKeyUsage=clientAuth\n' > client.ext
openssl x509 -req -in central.csr -CA nodes_ca.pem -CAkey nodes_ca.key -CAcreateserial \
    -days 825 -sha256 -extfile client.ext -out central.crt
```

Put the keeper's port where only the nodes can reach it all the same.

## Commands and stats

| Command | Does |
|---|---|
| `list-sessions` | The batches being received, one per channel, with what each file has |

Stats: `batches_stored`, `bytes_stored`, `batches_already` (offered and already
here), `batches_refused` (batches, not frames: once per batch refused).

Traces: `batches` and `frames`, as in archive_sender.

## Tests

`tests/c/archive_shipping/`: `test1` runs the two yunos end to end (see the
README of archive_sender); `test2` runs the keeper against a fake sender that
writes the frames by hand -- a forged sha256, too few of them, not lowercase
hex, data out of the order of the files (and a frame of it already on its way:
not refused twice), an empty batch, a batch of a kept name with other bytes:
each refused, and nothing of it left in the backup. Over two links: a batch in
flight on one is refused to the other and stored with the bytes of the first,
and a batch whose link went silent is taken over by the other link.
