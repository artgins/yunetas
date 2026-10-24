# perf_ievent

Benchmark of the inter-events between yunos: `C_IEVENT_CLI` ->
`C_WEBSOCKET` -> `C_IEVENT_SRV` -> a service -> its publication back to the
client's remote subscription. Both sides live in one yuno, on
`ws://127.0.0.1:7890`.

| Service | GClass | Role |
|---|---|---|
| `tester` | `C_PERF_IEVENT` | Subscribes `EV_PERF_PONG` of the remote `echo` through `cli`; when the session opens it sends `EV_PERF_PING`, and the next one when the `EV_PERF_PONG` of the previous one comes back: 100 000 round trips, timed. |
| `echo` | `C_PERF_IEV_ECHO` | Subscribes the events of the gate; answers every `EV_PERF_PING` publishing an `EV_PERF_PONG` with its `n` and its `payload` (~140 bytes of JSON). |
| `cli` | `C_IEVENT_CLI` | The client: `C_IOGATE` -> `C_CHANNEL` -> `C_WEBSOCKET` -> `C_TCP`. |
| `__input_side__` | `C_IOGATE` | The gate: `C_TCP_S` -> `C_CHANNEL` -> `C_IEVENT_SRV` -> `C_WEBSOCKET` (server) -> `C_TCP`. |

A warning, an error, or a session closed before the last round trip fails it.

## Run

```bash
$YUNETAS_OUTPUTS/bin/perf_ievent
```

It prints one line:

```
#TIME (count: 100000): elapsed 6345949576 ns, ops/sec      15758 : ievent round trips
```

Most of what a round trip costs is jansson's: the parse of every frame
received and the dump of every frame sent (see
`performance/audit/round-4-inter-yuno-http-mqtt.md`).
