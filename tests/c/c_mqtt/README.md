# c_mqtt test

MQTT GClass test. Spins up an embedded MQTT broker and a client inside the same process and exercises a **QoS 0 publish/subscribe round-trip** to verify the client-side protocol and broker integration.

`test_mqtt_acl` (`main_acl.c` + `c_acl.c`) drives the broker's publish/subscribe ACL through `EV_MQTT_ACL_CHECK`. It also checks `list-queues queue=<name>` of a queue that exists and cannot be opened (its `keys/` of mode 0, SKIPPED as root): `-1`, *"cannot open the queue"*. Up to 7.25.4 it answered `0` with an empty list. And `list-queues` / `clean-queues` of a store whose directory cannot be listed (mode 0, SKIPPED as root): `-1`, *"cannot list the topics of the store[, nothing cleaned], see the log"* -- a cause of their own, not the last ERROR of the process read back (which said *"...: Success"*, the `errno` of the log call, not of the listing).

`test_mqtt_malformed` sends malformed MQTT packets to the broker.

`test_mqtt_queued_in` (`main_queued_in.c` + `c_queued_in.c`) uses a RAW client
(a `C_TCP` that writes MQTT 3.1.1 packets by hand). Session 1 (clean session 0)
sends four QoS 2 PUBLISH, ids 1..4, gets four PUBREC, and goes away without
PUBREL. The broker's `max_inflight_messages` goes down to 2, and session 2
reloads the four: 1 and 2 in flight, 3 and 4 QUEUED. PUBREL 1..4 must be
answered `PUBREC 3, PUBCOMP 1, PUBREC 4, PUBCOMP 2, PUBCOMP 3, PUBCOMP 4`, with
no error: a queued message goes in flight when a slot frees, with its own
packet id, as in mosquitto. Up to 7.25.4 the quota test was inverted and the
moved message got a NEW packet id: the broker answered `PUBREC 1`, `PUBREC 2`,
and the PUBREL of 3 and 4 logged *"Message not found"* (never released).

## Run

```bash
ctest -R '^c_mqtt/' --output-on-failure --test-dir build
```

Requires `CONFIG_MODULE_MQTT=y`.
