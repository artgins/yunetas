# perf_mqtt

Benchmark of the MQTT publications delivered by the embedded broker
(`C_AUTHZ` + `C_MQTT_BROKER` on `mqtt://127.0.0.1:18119`), in one yuno, from
the test `c_mqtt/test1`.

The client (`C_PERF_MQTT`, over `C_IOGATE` -> `C_CHANNEL` -> `C_PROT_MQTT2`
-> `C_TCP`) connects, subscribes `perf/topic`, and publishes on it at QoS 0
a payload of 99 bytes of JSON; it publishes the next one when the broker
delivers the previous one back to it: 100 000 times, timed. Then it
disconnects. An error, or a connection closed before the last message,
fails it. The broker's store goes in `/tmp/store/perf_mqtt`.

## Run

```bash
$YUNETAS_OUTPUTS/bin/perf_mqtt
```

It prints one line:

```
#TIME (count: 100000): elapsed 2984490860 ns, ops/sec      33506 : mqtt qos0 round trips
```

What the performance round of 7.28.0 took off a publication is in the
`CHANGELOG.md` section of that release.
