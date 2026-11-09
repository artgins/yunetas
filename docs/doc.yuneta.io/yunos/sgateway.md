(yuno-sgateway)=
# `sgateway`

Simple Gateway — a **raw TCP relay**. It listens on an input URL, dials out to
an output URL, and forwards bytes verbatim in both directions. Useful for
tunnelling or exposing a backend endpoint through a controlled front.

## Architecture

Two sides, each one a [`C_IOGATE`](#gclass-c-iogate) with one channel:

```
              input_url (listen)                    output_url (connect)
client  ───▶  __input_side__  ──▶  C_SGATEWAY  ──▶  __output_side__  ───▶  upstream
        ◀───                  ◀──             ◀──                    ◀───
```

- **Input side** (`__input_side__`): a [`C_TCP_S`](#gclass-c-tcp-s) on
  `input_url`, and ONE channel `tcp-1` (`C_CHANNEL > C_PROT_RAW`). The server
  finds the channel with its `child_tree_filter` and creates its `C_TCP` at
  each accept (legacy method). So the gateway takes **one client at a time**:
  a second client is refused while the first one is connected.
- **Output side** (`__output_side__`): channel `output`
  (`C_CHANNEL > C_PROT_RAW > C_TCP`), a TCP **client** of `output_url`.
  **One output connection per client**: the gateway starts this `C_TCP` when
  a client connects and stops it when the client leaves, so no client
  inherits the session of another one with the backend. Up to 7.30.18 the
  output connected when the yuno played and stayed connected, shared by the
  clients that came one after the other: a client got what the backend still
  had for the one before, and its session (a login, a state).
- **[`C_PROT_RAW`](#gclass-c-prot-raw)** carries the payload, so bytes pass
  through untouched: no framing, no parsing, no protocol awareness.
- `C_SGATEWAY` relays `EV_ON_MESSAGE`: a **copy** of what arrives from one
  side is sent to the other one.

### One client, one output connection

```
client connects     -> the output connects (client not read until it is open)
output open         -> what the client had sent goes first, then both ways
client leaves       -> the output is closed
output closes       -> the client is dropped
output cannot connect -> it tries again after timeout_between_connections ms,
                         doubling up to 30 s, while the client waits
```

What the client sends before its output is open is kept, and goes first when
it opens; if the client leaves before, it is dropped and counted in
`dropMsgs`. Bytes for a side that has closed (the other one is being closed)
are dropped and counted in `dropMsgs`, with one WARNING each time a side
closes (not one per message).

When one side ends, the other one is closed at once, and a `C_TCP` closes by
resetting the connection (see *The close resets the connection* in
[`C_TCP`](#gclass-c-tcp)): what that side's socket had not yet delivered to a
slow peer is lost. A backend that answers and closes at once can lose the end
of its answer to a slow client.

### Back-pressure

The gateway never reads one side faster than the other side writes. After
relaying what one side sent, it pauses the reading of that side
(`rx_paused` of its `C_TCP`), and resumes it at the `EV_TX_READY` of the side
written. A slow peer, or one that does not read at all, slows the other peer
down through TCP itself, and the gateway holds one read at most. Up to
7.30.18 the gateway handed over the read buffer itself, and the reading went
on: as soon as one side was slower, the buffer still queued was read into
again (the data arrived corrupted after the first megabytes), every next
message was refused with an ERROR, and against a side that never reads the
log grew by megabytes per second.

### TLS

The url scheme selects TLS (`tcps://`). The output takes its TLS
configuration from the `crypto` attribute; by default it verifies the
backend's certificate against the system CA. For a private CA:

```json
{
    "global": {
        "sgateway.output_url": "tcps://192.0.2.10:2012",
        "sgateway.crypto": {
            "ssl_trusted_certificate": "/yuneta/store/certs/backend-ca.pem",
            "ssl_server_name": "backend.example.com"
        }
    }
}
```

`crypto` replaces the default whole: repeat the verifying keys you want.
Up to 7.30.18 the gateway had no `crypto`, and a TLS output was refused at
each attempt (*"TLS client refused"*, an ERROR per second). A TLS input
needs its certificate in the config, as a global key of the server
(`C_TCP_S.crypto`); it is not covered by the test.

## Configuration

Both URLs are required. They are read at each play: if one is empty, the play
is refused with an ERROR (*"What yuno input url?"*), and the yuno keeps
running without playing.

| Attribute | Default | Purpose |
|-----------|---------|---------|
| `input_url` | *(required)* | URL the gateway **listens** on (for example `tcp://0.0.0.0:2011`) |
| `output_url` | *(required)* | URL the gateway **connects** to, one connection per client (for example `tcp://192.0.2.10:2012`) |
| `crypto` | `{"ssl_use_system_ca": true, "ssl_verify_mode": "required"}` | TLS of the output side (`tcps://`), see *TLS* |
| `timeout_between_connections` | `100` | ms before the output connects again after a failed attempt; doubled at each failure, up to 30 s |
| `timeout` | `1000` | Period of the rate stats (ms) |

Standalone (no agent), the yuno plays itself with `yuno.autoplay`:

```json
{
    "yuno": {
        "autoplay": true
    },
    "global": {
        "sgateway.input_url": "tcp://0.0.0.0:2011",
        "sgateway.output_url": "tcp://192.0.2.10:2012"
    }
}
```

```bash
sgateway --config-file=sgateway.json
```

Under the agent, the same `global` keys go in the yuno's configuration. Or
write the urls at run time (`input_url` / `output_url` are persisted), then
pause and play the yuno:

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=write-attr attribute=input_url  value=tcp://0.0.0.0:2011'
ycommand -c 'command-yuno id=<id> service=__yuno__ command=write-attr attribute=output_url value=tcp://192.0.2.10:2012'
ycommand -c 'pause-yuno id=<id>'
ycommand -c 'play-yuno id=<id>'
```

### Throughput stats

`SDF_RSTATS` (reset with `stats=__reset__`): `rxMsgs` (received from either
side), `txMsgs` (sent to the other side), `dropMsgs` (dropped: the other side
had closed, or the client left before its output opened), and the per-second gauges `rxMsgsec` / `txMsgsec` with their
high-water marks `maxrxMsgsec` / `maxtxMsgsec` (recomputed on each `timeout`
tick).

## Commands

| Command | Description |
|---------|-------------|
| `help` | Command help |
| `authzs` | Authorization help |

The gateway is configured through attributes, not commands. Its sides are
services, so the [`C_IOGATE`](#gclass-c-iogate) commands reach them:
`command-yuno id=<id> service=__input_side__ command=view-channels`.

## Debugging

| GClass | Level | Shows |
|--------|-------|-------|
| `C_SGATEWAY` | `messages` | Relayed messages (a dump of each one) |
| `C_SGATEWAY` | `connections` | `Side open` / `Side closed` of each side (INFO, `side`: `input` or `output`) |
| [`C_TCP`](#gclass-c-tcp) | `connections`, `traffic` | Per-connection events / raw bytes |
| [`C_TCP_S`](#gclass-c-tcp-s) | `listen`, `accepted`, `not-accepted` | Listener accept activity |

No trace is on by default: a peer that connects again and again does not
write the log (up to 7.30.18 `C_TCP` `connections`, `C_TCP_S` `listen/
accepted/not-accepted` and the INFO `Side open` / `Side closed` were on, some
1.4 MB of log per 1000 connections). Enable them with
`ycommand command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=<G> set=1 level=<L>`,
or in the yuno's configuration:

```json
{
    "yuno": {
        "trace_levels": {
            "C_SGATEWAY": ["connections"]
        }
    }
}
```

## Test

`tests/c/sgateway/` runs the real binary between a listener (its output
target) and a client (of its input port): bytes both ways; one output
connection per client (it closes when its client leaves, and a second
client gets nothing of the first); the client dropped when its output goes;
8 MB each way against a slow peer, whole and in order; a peer that never
reads (the gateway does not grow); then 20 clients through a TLS output with
the default traces, and a log that does not grow with them. Every log is
clean at the orderly stop. See the [test suite](../test_suite.md).
