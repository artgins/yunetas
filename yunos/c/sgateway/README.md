# sgateway

Simple Gateway: a raw TCP relay. It listens on `input_url`, connects to
`output_url`, and sends the bytes that arrive on one side, as they are, to the
other side. There is no framing and no parsing, so it carries any protocol
that runs over TCP.

```
client --> input_url  (__input_side__:  C_TCP_S, channel tcp-1)
                                   C_SGATEWAY
            output_url (__output_side__: client C_TCP, channel output) --> server
```

- The input side takes **one** connection at a time. While a client is
  connected, a second client is refused.
- **One output connection per client.** The output connects when a client
  connects and is closed when the client leaves; when the output closes, the
  client is dropped. No client inherits the backend session of another one.
  A failed attempt to connect is retried after `timeout_between_connections`
  ms (default 100), doubling up to 30 s, while the client waits. What the
  client sends before its output is open is kept and goes first.
- **Back-pressure.** A copy of what one side sends goes to the other side,
  and the first side is not read again until the second has written it
  (`rx_paused` of `C_TCP`, resumed at `EV_TX_READY`). A slow peer, or one
  that does not read, slows the other one down through TCP.
- Bytes for a side that has closed are dropped. They are counted in
  `dropMsgs`, with one WARNING each time a side closes.
- A TLS output (`tcps://`) takes its configuration from `crypto`.
- `Side open` / `Side closed` (INFO) are said at the trace level
  `connections` of `C_SGATEWAY` only; no trace is on by default.

## Configuration

| Attribute | Default | Description |
|-----------|---------|-------------|
| `input_url` | (required) | Where the input side listens, for example `tcp://0.0.0.0:2011` |
| `output_url` | (required) | Where the output side connects, one connection per client, for example `tcp://192.0.2.10:2012` |
| `crypto` | `{"ssl_use_system_ca": true, "ssl_verify_mode": "required"}` | TLS of the output side (`tcps://`); replaces the default whole, for example `{"ssl_trusted_certificate": "/path/ca.pem"}` |
| `timeout_between_connections` | `100` | ms before the output connects again after a failed attempt, doubled at each failure up to 30 s |
| `timeout` | `1000` | Period of the rate stats, in ms |

The urls are read at each play. If one of them is empty, the play is refused
with an ERROR, and the yuno keeps running without playing.

Standalone (without an agent), with a config file:

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

Under the agent, put the same `global` keys in the yuno's configuration. Or
write the urls at run time (they are persisted), then pause and play the yuno:

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=write-attr attribute=input_url value=tcp://0.0.0.0:2011'
ycommand -c 'command-yuno id=<id> service=__yuno__ command=write-attr attribute=output_url value=tcp://192.0.2.10:2012'
ycommand -c 'pause-yuno id=<id>'
ycommand -c 'play-yuno id=<id>'
```

## Stats

`rxMsgs` (received from either side), `txMsgs` (sent to the other side),
`dropMsgs` (dropped: the other side had closed, or the client left before its
output opened), and the rates `rxMsgsec` /
`txMsgsec` with their maximums `maxrxMsgsec` / `maxtxMsgsec`.

## Test

`tests/c/sgateway/` runs this binary between a listener and a client: bytes
both ways, one output connection per client, the client dropped when its
output goes, 8 MB each way against a slow peer, a peer that never reads, and
20 clients through a TLS output with a log that does not grow; a clean log at
each orderly stop.
