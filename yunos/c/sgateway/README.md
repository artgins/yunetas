# sgateway

Simple Gateway: a raw TCP relay. It listens on `input_url` and connects to
`output_url`. It sends the bytes that arrive on one side, with no change, to
the other side. It does no framing and no parsing, so it carries any protocol
that runs over TCP.

## Usage

To run it standalone (without an agent), write a configuration file:

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

Then start the yuno with that file:

```bash
sgateway --config-file=sgateway.json
```

Under the agent, put the same `global` keys in the configuration of the yuno.
Or write the urls at run time (the yuno persists them), then pause and play
the yuno:

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=write-attr attribute=input_url value=tcp://0.0.0.0:2011'
ycommand -c 'command-yuno id=<id> service=__yuno__ command=write-attr attribute=output_url value=tcp://192.0.2.10:2012'
ycommand -c 'pause-yuno id=<id>'
ycommand -c 'play-yuno id=<id>'
```

## How it works

```
client --> input_url  (__input_side__:  C_TCP_S, channel tcp-1)
                                   C_SGATEWAY
            output_url (__output_side__: client C_TCP, channel output) --> server
```

- The input side accepts one connection at a time. While a client is
  connected, the yuno refuses a second client.
- One output connection for each client. The output connects when a client
  connects, and it closes when the client leaves. When the output closes, the
  yuno drops the client. No client gets the backend session of another one.
  If an attempt to connect fails, the yuno tries again after
  `timeout_between_connections` ms (default 100). It doubles this time up to
  30 s, while the client waits. The yuno keeps what the client sends before
  its output is open, and sends it first. The yuno does not read the client
  in that time. Thus it also does not see a client that leaves in that time.
  It delivers what that client sent when the output opens, then it closes
  the output. In that time, the yuno refuses a second client.
- Back-pressure (a slow reader slows the sender). A copy of what one side
  sends goes to the other side. The yuno does not read the first side again
  until the second side has written that copy (`rx_paused` of `C_TCP`,
  resumed at `EV_TX_READY`). A slow peer, or a peer that does not read, slows
  the other peer down through TCP.
- The yuno drops the bytes for a side that has closed. It counts them in
  `dropMsgs`, with one WARNING each time a side closes.
- A TLS output (`tcps://`) takes its configuration from `crypto`. A TLS
  input (`tcps://`) takes it from `input_crypto` (its certificate and key).
- A pause, or the stop of the yuno, ends the client and its output. It does
  not start an output again.
- The yuno logs `Side open` / `Side closed` (INFO) only at the trace level
  `connections` of `C_SGATEWAY`. No trace is on by default.

## Configuration

| Attribute | Default | Description |
|-----------|---------|-------------|
| `input_url` | (required) | Where the input side listens, for example `tcp://0.0.0.0:2011` |
| `output_url` | (required) | Where the output side connects, one connection for each client, for example `tcp://192.0.2.10:2012` |
| `crypto` | `{"ssl_use_system_ca": true, "ssl_verify_mode": "required"}` | TLS of the output side (`tcps://`). A value replaces all of the default, for example `{"ssl_trusted_certificate": "/path/ca.pem"}` |
| `input_crypto` | `{}` | TLS of the input side (`tcps://`), the `crypto` of its `C_TCP_S`, for example `{"ssl_certificate": "/path/cert.pem", "ssl_certificate_key": "/path/key.pem"}`. A `tcps://` input without a certificate does not listen, and the yuno exits |
| `timeout_between_connections` | `100` | ms before the output connects again after a failed attempt. The yuno doubles it at each failure, up to 30 s |
| `timeout` | `1000` | Period of the rate stats, in ms |

The yuno reads the urls at each play. If one of them is empty, the yuno
refuses the play with an ERROR, and it continues to run without playing.

## Stats

- `rxMsgs`: received from either side.
- `txMsgs`: sent to the other side.
- `dropMsgs`: dropped, because the other side had closed, or because a pause
  or a stop came before the output of the client opened.
- The rates `rxMsgsec` / `txMsgsec`, with their maximums `maxrxMsgsec` /
  `maxtxMsgsec`.

## Test

`tests/c/sgateway/` runs this binary between a listener and a client. It
tests these cases:

- Bytes in both directions.
- One output connection for each client.
- The client dropped when its output goes.
- 8 MB in each direction against a slow peer.
- A peer that never reads.
- A client that leaves while its output cannot connect.
- A stop with a client that does not read (no output connection opens while
  the yuno stops).
- 20 clients through a TLS input and a TLS output, with a log that does not
  grow.

The log is clean at each orderly stop.
