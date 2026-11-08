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
  a second client is refused while the first one is connected, because the
  output side is one connection and its replies could not be told apart.
- **Output side** (`__output_side__`): channel `output`
  (`C_CHANNEL > C_PROT_RAW > C_TCP`), a TCP **client** of `output_url`. It
  connects when the yuno plays and stays connected; when the connection
  fails or ends it connects again after `timeout_between_connections` ms. It
  does not depend on the input side. The gateway starts this `C_TCP` itself
  (a `C_TCP` is manual start, and `C_PROT_RAW` does not start its bottom).
- **[`C_PROT_RAW`](#gclass-c-prot-raw)** carries the payload, so bytes pass
  through untouched: no framing, no parsing, no protocol awareness.
- `C_SGATEWAY` relays `EV_ON_MESSAGE`: bytes from the input side are sent to
  the output side, and the reverse. Bytes for a side that is not open are
  dropped and counted in `dropMsgs`, with one WARNING each time a side
  closes (not one per message).
- Each side logs `Side open` / `Side closed` (INFO, `side`: `input` or
  `output`).

The url scheme selects TLS (`tcps://`), but the gateway gives no `crypto` to
its transports: a TLS input needs its certificate in the config, as a global
key of the server (`C_TCP_S.crypto`). TLS is not covered by the test.

## Configuration

Both URLs are required. They are read at each play: if one is empty, the play
is refused with an ERROR (*"What yuno input url?"*), and the yuno keeps
running without playing.

| Attribute | Default | Purpose |
|-----------|---------|---------|
| `input_url` | *(required)* | URL the gateway **listens** on (for example `tcp://0.0.0.0:2011`) |
| `output_url` | *(required)* | URL the gateway **connects** to (for example `tcp://192.0.2.10:2012`) |
| `timeout_between_connections` | `100` | ms between connection attempts of the output side |
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
was not open), and the per-second gauges `rxMsgsec` / `txMsgsec` with their
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
| [`C_TCP`](#gclass-c-tcp) | `connections`, `traffic` | Per-connection events / raw bytes |
| [`C_TCP_S`](#gclass-c-tcp-s) | `listen`, `accepted`, `not-accepted` | Listener accept activity |

`sgateway` enables `C_TCP` `connections` and `C_TCP_S` `listen/accepted/
not-accepted` by default. Enable more with
`ycommand command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=<G> set=1 level=<L>`.

## Test

`tests/c/sgateway/` runs the real binary between a listener (its output
target) and a client (of its input port): bytes both ways, a reconnect of
each side, and a clean log at the orderly stop. See the [test suite](../test_suite.md).
