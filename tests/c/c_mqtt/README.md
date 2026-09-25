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

`test_mqtt_client_queues` (`main_client_queues.c` + `c_client_queues.c`) is the
other side: a C_PROT_MQTT2 CLIENT with persistent queues (`tranger_queues`)
against a RAW broker (a `C_PROT_RAW` channel of the driver that writes the MQTT
3.1.1 packets by hand). A. `PUBLISH 5 'a'` (QoS 2), again with DUP=1 (its
PUBREC 'lost'), `PUBREL 5`, then `PUBLISH 5 'b'` and `PUBREL 5`: the user must
get `a` once and then `b`, with no error. Up to 7.25.4 the duplicate was
searched by the rowid of its queue record: the old copy stayed, and the second
PUBREL delivered `a` again (red: *"a,a,"*); and every QoS 2 PUBREL logged
*"QoS mismatch"* (the flag bits compared with the qos level). B. 22 QoS 1
messages of the user with 20 in flight, `m21` with `expiry_interval` 1: after
2.5 s, `PUBACK 1` frees a slot, `m21` goes in flight expired and is discarded,
and `m22` must be sent. Up to 7.25.4 nothing moved after the expiry (red: `m22`
never sent).

`test_mqtt_out_flight` (`main_out_flight.c` + `c_out_flight.c`) uses a RAW MQTT 5
client against the broker, whose `max_inflight_messages` is 0 ("no maximum").
The client connects with Receive Maximum 2 (no clean start), subscribes to
`t/o` with QoS 1 and publishes four QoS 1 messages to it. A. The broker must
send TWO of them, and the other two after the client's PUBACKs: up to 7.25.4
it checked only its own maximum and sent all four. B. With three acked,
`list-queues queue=oflight_cl-OUT qos=1` must list ONE message: up to 7.25.4
`qos=` replaced the pending condition and listed the delivered ones too. C. The
fourth is acked with a PUBCOMP (the ack of QoS 2): the broker must say *"QoS
mismatch"* as a WARNING, answer DISCONNECT 0x82 and keep the message pending.
Up to 7.25.4 it logged an ERROR and removed the message as delivered.

## Run

```bash
ctest -R '^c_mqtt/' --output-on-failure --test-dir build
```

Requires `CONFIG_MODULE_MQTT=y`.
