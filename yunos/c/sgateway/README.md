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
  connected, a second client is refused: the output side is one connection,
  and the replies could not be told apart.
- The output side connects when the yuno plays and stays connected. If the
  connection fails or ends, it connects again after
  `timeout_between_connections` ms (default 100). It does not depend on the
  input side.
- Bytes that arrive while the other side is not open are dropped. They are
  counted in `dropMsgs`, with one WARNING each time a side closes.
- Each side logs `Side open` / `Side closed` (INFO).

## Configuration

| Attribute | Default | Description |
|-----------|---------|-------------|
| `input_url` | (required) | Where the input side listens, for example `tcp://0.0.0.0:2011` |
| `output_url` | (required) | Where the output side connects, for example `tcp://192.0.2.10:2012` |
| `timeout_between_connections` | `100` | ms between connection attempts of the output side |
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
`dropMsgs` (dropped, the other side not open), and the rates `rxMsgsec` /
`txMsgsec` with their maximums `maxrxMsgsec` / `maxtxMsgsec`.

## Test

`tests/c/sgateway/` runs this binary between a listener and a client: bytes
both ways, a reconnect of each side, and a clean log at the orderly stop.
