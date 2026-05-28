(yuno-sgateway)=
# `sgateway`

Simple Gateway — a **raw TCP relay**. It listens on an input URL, dials out to
an output URL, and forwards bytes verbatim in both directions. Useful for
tunnelling or exposing a backend endpoint through a controlled front.

## Architecture

Two symmetric sides, each a `C_IOGATE > C_CHANNEL > C_PROT_RAW > C_TCP` stack:

```
              input_url (listen)                    output_url (connect)
client  ───▶  __input_side__  ──▶  C_SGATEWAY  ──▶  __output_side__  ───▶  upstream
        ◀───                  ◀──             ◀──                    ◀───
```

- **Input side** (`__input_side__`): a TCP **server** bound to `input_url`,
  accepting client connections.
- **Output side** (`__output_side__`): a TCP **client** that connects to
  `output_url` (reconnecting every 100 ms).
- **`C_PROT_RAW`** carries the payload, so bytes pass through untouched — there
  is no framing, parsing, or protocol awareness.
- `C_SGATEWAY` relays `EV_ON_MESSAGE`: bytes from the input side are sent to
  the output side and vice-versa.

TLS is selected by the URL scheme (`tcps://` for either side); `tcp://` is
plain. Because the relay is byte-transparent it bridges any TCP-based protocol.

## Configuration

Both URLs are required; at play time, if either is empty the yuno logs an error
and pauses itself.

| Attribute | Default | Purpose |
|-----------|---------|---------|
| `input_url` | *(required)* | URL the gateway **listens** on (e.g. `tcp://0.0.0.0:2011`) |
| `output_url` | *(required)* | URL the gateway **connects** to (e.g. `tcp://host:port`) |
| `timeout` | `1000` | Stats tick (ms) |

`input_url` / `output_url` are writable and persisted. Set them through the
agent, then (re)play the yuno:

```bash
ycommand command-yuno id=<id> service=__root__ command=write-str attribute=input_url  value=tcp://0.0.0.0:2011
ycommand command-yuno id=<id> service=__root__ command=write-str attribute=output_url value=tcp://10.0.0.5:2012
```

### Throughput stats

Exposed as reset-on-read stats (`SDF_RSTATS`): `rxMsgs`, `txMsgs`, and the
per-second gauges `rxMsgsec` / `txMsgsec` with their high-water marks
`maxrxMsgsec` / `maxtxMsgsec` (recomputed on each `timeout` tick).

## Commands

| Command | Description |
|---------|-------------|
| `help` | Command help |

(The gateway is configured through attributes, not commands.)

## Debugging

| GClass | Level | Shows |
|--------|-------|-------|
| `C_SGATEWAY` | `messages` | Relayed messages |
| `C_TCP` | `connections`, `traffic` | Per-connection events / raw bytes |
| `C_TCP_S` | `listen`, `accepted`, `not-accepted` | Listener accept activity |

`sgateway` enables `C_TCP` `connections` and `C_TCP_S` `listen/accepted/
not-accepted` by default. Enable more with
`ycommand command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=<G> set=1 level=<L>`.
