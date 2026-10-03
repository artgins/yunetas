# c_mqtt test

MQTT GClass test. Spins up an embedded MQTT broker and a client inside the same process and exercises a **QoS 0 publish/subscribe round-trip** to verify the client-side protocol and broker integration.

`test_mqtt_acl` (`main_acl.c` + `c_acl.c`) drives the broker's publish/subscribe ACL through `EV_MQTT_ACL_CHECK`. It also checks `list-queues queue=<name>` of a queue that exists and cannot be opened (its `keys/` of mode 0, SKIPPED as root): `-1`, *"cannot open the queue"*. Up to 7.25.4 it answered `0` with an empty list. And `list-queues` / `clean-queues` of a store whose directory cannot be listed (mode 0, SKIPPED as root): `-1`, *"cannot list the topics of the store[, nothing cleaned], see the log"* -- a cause of their own, not the last ERROR of the process read back (which said *"...: Success"*, the `errno` of the log call, not of the listing).

`test_mqtt_malformed` sends malformed MQTT packets to the broker.

`test_mqtt_will_acl` (`main_will_acl.c` + `c_will_acl.c`) uses RAW MQTT 3.1.1
clients against the broker with `enable_acl` on. The group of `will_bad` and
`will_ok` may publish only to `allowed/#`; `will_sub` (a group with no
patterns) subscribes `forbidden/will` and `allowed/will`. `will_bad` connects
with a will to `forbidden/will` and goes away with no DISCONNECT: the broker
must refuse the will (the WARNING *"Will message refused by the ACL: not
published"*) and `will_sub` must get nothing. `will_ok` does the same with a
will to `allowed/will`, which `will_sub` must get. Up to 7.25.22 the will
bypassed the ACL (red: `will_sub` got the will to `forbidden/will`). Then
`will_ret` publishes a RETAINED message with a payload (QoS 1), and `will_late`
subscribes it afterwards: it must get it flagged retained, with its payload.
Each step waits for the packet it depends on (CONNACK, SUBACK, PUBACK, the
PUBLISH the subscriber gets); the timer is only a guard that names the step
that did not come. The memory check at the end caught a second defect: each
CONNECT with a will, and each retained PUBLISH with a payload, leaked its
payload (`C_NODE` handed the treedb the event's shared kw, whose `gbuffer` the
treedb takes; red: 3 x 357 bytes not freed).

The leak checks of these tests (*"system memory not free"* at the end) work
only in a build with `CONFIG_DEBUG_TRACK_MEMORY`; where it is off (the nodes'
builds, wattyzer's release run) they pass whatever leaks.

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

`test_mqtt_wrong_ack` (`main_wrong_ack.c` + `c_wrong_ack.c`) uses a RAW MQTT 5
client against the broker, and a log handler of `main` that reads EVERY line,
traces included. A. `auth_data` written on the broker's `C_PROT_MQTT2` must be
shown as `********` by `view-attrs attribute=auth_data` and `view-gobj`: up to
7.25.20 it lacked `SDF_SECRET` and showed in clear. B. With the `show-decode`
trace armed, a CONNECT with username `wack_user` and a 300-byte password: the
password must appear in no line of the log, and the username must be printed
as `'wack_user'`. Up to 7.25.20 the trace printed the password, and read the
username past its end (the packet is not NUL-terminated): *"username
'wack_user,PASSWORD...'"*. C. A QoS 1 PUBLISH 20, then PUBREL 20: the broker
has no QoS 2 message 20, so it must answer PUBCOMP 20 and say *"Message not
found"* as a WARNING. D. PUBREC of the QoS 1 message the broker delivered: a
*"QoS mismatch"* WARNING and DISCONNECT 0x82. Up to 7.25.20 both C and D were
ERRORs (C with a stack); the test fails on any ERROR in the log. E. A second
raw client sends the same CONNECT with one byte too many: the broker refuses
it and dumps the frame, and the dump must not carry the password (the scanner
also looks for a 16-byte row of it, as a dump splits it). Up to 7.25.20 the
whole CONNECT was dumped.

`test_mqtt_legacy_prot` (`main_legacy_prot.c` + `c_legacy_prot.c`) drives
`C_PROT_MQTT`, the deprecated protocol gclass, as a server with no broker and
no socket. The protocol gobj is a child of the driver, its bottom is a
`C_FAKE_TRANSPORT` (`c_fake_transport.c`: what the protocol writes goes to the
driver, and a drop comes back as `EV_DISCONNECTED`). The driver also keeps
the protocol's clients in memory, as its resource gobj. It writes the client's
MQTT 5 packets by hand. It subscribes to `a/b` and to a 300-byte topic, then
UNSUBSCRIBEs both in ONE packet: the UNSUBACK must carry two 0x00, the
"unsubscribing" `EV_ON_MESSAGE` must list the two topics as sent, and no
subscription may be left. Up to 7.25.20 the topic was read in the packet as a
C string: `a/b` ran on into the length of the next topic, so it answered 0x11
and `a/b` stayed subscribed. Then, on a new connection, a CONNECT with a
300-byte password and one byte too many: the dump of the refused frame must
not carry the password (up to 7.25.20 it carried the whole CONNECT).

`test_mqtt_client_pubrec` (`main_client_pubrec.c` + `c_client_pubrec.c`) drives
`C_PROT_MQTT2` as a CLIENT on a `C_FAKE_TRANSPORT`, with its queues in a
`C_TRANGER` of the driver; the driver plays the broker. After CONNACK: a PUBREC
of a packet id the client never used must be said once, as the WARNING *"Mqtt:
Received PUBREC for an unknown packet ID"*, and answered with PUBREL (up to
7.25.20 the client also logged an ERROR *"Message not found"* with a stack).
Then a QoS 1 PUBLISH of the client, and PUBREC of it: *"QoS mismatch"*, a
WARNING that must name the client (`client_id`, checked in the log), and
DISCONNECT 0x82.

The tests that keep a store (`acl`, `queued_in`, `client_queues`,
`out_flight`, `wrong_ack`, `legacy_prot`, `client_pubrec`) work in a dir of
their own run, `<$TMPDIR or /tmp>/test_mqtt_<name>.<pid>.<n>`
(`test_work_dir.c`), removed at the end; each binary has a port of its own.
So they run at once (`ctest -j`). Up to 7.25.20 each wiped a fixed
`/tmp/test_mqtt_<name>` at its start.

## Run

```bash
ctest -R '^c_mqtt/' --output-on-failure --test-dir build
```

Requires `CONFIG_MODULE_MQTT=y`.
