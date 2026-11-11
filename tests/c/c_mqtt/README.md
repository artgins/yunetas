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
treedb takes; red: 3 x 357 bytes not freed). Last, `will_deny`, an MQTT 3.1
client (`MQIsdp`, level 3), subscribes `denied/topic`, in the broker's
`deny_subscribes`: the SUBACK must say `0x80`. Up to 7.30.26 a 3.1 client got
the QoS it asked for, as if granted (red: code 0).

The leak checks of these tests (*"system memory not free"* at the end) work
only in a build with `CONFIG_DEBUG_TRACK_MEMORY`; where it is off (the nodes'
builds, a staging node's release run) they pass whatever leaks.

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

`test_mqtt_gate_prot` (`main_gate_prot.c` + `c_gate_prot.c`) drives
`C_PROT_MQTT2` as the input of a gate: no broker, no queues
(`tranger_queues` NULL), `allow_anonymous`, `max_subscriptions_per_client` 3,
`max_topic_filter_length` 16, `max_inflight_messages` 2. The driver is its
upper level, as a gate's decoder is: it answers the CONNECT, writes the reason
codes of a (UN)SUBSCRIBE, takes the messages (returns 1), and sends one to the
client. On a `C_FAKE_TRANSPORT`, with the packets written by hand:

1. MQTT 3.1.1 with a username and a password nobody checks: CONNACK 0.
2. PUBLISH QoS 0, 1 and 2, each delivered once (PUBACK, PUBREC); the QoS 2
   sent again with DUP before its PUBREL gets PUBREC and is not delivered
   again; its PUBREL gets PUBCOMP.
3. A PUBREL, PUBACK, PUBREC and PUBCOMP of ids the server never used: PUBCOMP
   and PUBREL, one WARNING for the connection. No crash.
4. SUBSCRIBE of 5 filters: the one of 22 bytes and the one past 3 get `0x80`,
   the others QoS 0; a filter held is granted again and not counted; an
   UNSUBSCRIBE makes room.
5. A QoS 1 message to the client goes in QoS 0.
6. Two QoS 2 without PUBREL, then a third: the client is dropped at once
   (3.1.1: no DISCONNECT from the server), with a WARNING.
7. MQTT 5 on a new connection: `0x8F` and `0x97` in the SUBACK (the counts
   are of the connection), PUBACK Success for a message taken.
8. A PUBLISH with the property 127, which MQTT 5 does not define: ONE
   WARNING *"Mqtt unknown property"*, DISCONNECT `0x81`, the drop (up to
   7.30.26: two ERRORs, *"Mqtt unknown property"* and *"Mqtt empty
   property"*, and a WARNING).

9. MQTT 5 on a third connection: a CONNECT with the property 0x01
   (Payload Format Indicator, not of a CONNECT): one WARNING *"Mqtt invalid
   property of command"* whose `command` is `CMD_CONNECT` (up to 7.30.26 a
   number: a string printed with `%d`).

No log of an ACL (there is no broker). Up to 7.30.25: CONNACK 5 (the CONNECT
was authenticated against the broker's treedb) and nothing else.

`test_mqtt_gate_limits` (`main_gate_limits.c` + `c_gate_limits.c`) is what a
peer can do to the same gate input, on a `C_FAKE_TRANSPORT`, with
`max_subscriptions_per_client` 3, `message_size_limit` 16, `max_packet_size`
70000, `max_qos` 1 and `timeout_close` 2 s. Every line was red on 7.30.26:

1. MQTT 5 CONNACK carries Maximum QoS 1 (it was not sent, and an ERROR
   *"Mqtt property int16 unknown"* was logged).
2. Two QoS 1 PUBLISHes with three user properties past
   `message_size_limit`: PUBACK `0x95` each, ONE WARNING for the connection
   (one per PUBLISH), and no leak (its properties were not freed: the memory
   check at the end). The second connection (10a) says it again.
3. A SUBSCRIBE of 13000 copies of `a`: the upper level is asked once, 13000
   codes in the SUBACK (all went up; a 12 KB buffer: 8426 pairs of ERRORs and
   a SUBACK of mixed codes).
4. A filter the upper level refuses (`0x87`) is not held: two filters more fit
   under 3, the third gets `0x97`.
5. An UNSUBSCRIBE of 20000 filters: 20000 codes in the UNSUBACK (12287).
6. The upper level writes no code for one filter: ERROR *"The upper level did
   not answer one reason code per filter"*, and `0x80` for it (the UNSUBACK
   was short of a code, and nothing was said).
   6b. A SUBSCRIBE of `b/1 deny/2 b/1 deny/2 b/2` and an UNSUBSCRIBE of
   `q r q r q`: each distinct filter goes up once, and every occurrence takes
   its code, the refusal `0x87` too (each occurrence went up).
   6c. A SUBSCRIBE of `o/1` (QoS 1) and `o/1` again (options `0x2C`: retain
   handling 2, retain as published, no local): `o/1` goes up once, with the
   options of the last occurrence (`subscription_options` 44,
   `retain_handling` 32).
7. `EV_SEND_MESSAGE` to `cmnd/x/#`: `-1` and an ERROR (it was published). To a
   topic of 202 levels: `-1` and an ERROR, the server's own limit of 200
   separators (a client has none).
8. Five acks of unknown packet ids: one WARNING (five).
9. `EV_DROP` with `session_take_over`: DISCONNECT `0x8E`. Then, while the
   connection is closing, `EV_SEND_MESSAGE` is `-1` (twice, one WARNING) and another
   `EV_DROP` drops the transport as soon as the DISCONNECT is written, on the
   next cycle (both were *"Event NOT DEFINED"* ERRORs, and the drop waited for
   `timeout_close`).
10. A PUBLISH of 20000 bytes written in two parts, cut after its 1st to 5th
    byte: delivered every time (cut after the 3rd, the second byte of the
    length was read twice and the connection ended as malformed).
11. A DISCONNECT `e0 02 00 00` and a PUBLISH behind it in the same write: no
    ERROR, and the PUBLISH is not read (*"kw must be list or dict"* with a
    stack, and the PUBLISH delivered).
12. A CONNECT with client id `.`: CONNACK `0x85`, and `main` finds no dump
    *"MQTT malformed packet: CMD_CONNECT"* in the log (there was one, and an
    ERROR *"Mqtt proplist NULL"*).
13. An MQTT 5 CONNECT with properties and a will of QoS 2, past `max_qos`:
    CONNACK `0x9B`, and no leak (its properties were not freed).
14. A PUBLISH to `a/+`: DISCONNECT `0x81`, and the WARNING *"Mqtt: invalid
    topic in PUBLISH, disconnecting"* (it said *"Mqtt will: invalid topic"*).

`test_mqtt_client_noqueue` (`main_client_noqueue.c` + `c_client_noqueue.c`)
drives `C_PROT_MQTT2` as a CLIENT on a `C_FAKE_TRANSPORT`, with
`max_packet_size` 1000; the driver plays the broker (MQTT 5). Without
queues: its CONNECT announces Maximum Packet Size 1000 (not sent); its user
publishes in QoS 1 inside `EV_ON_OPEN`: `-1` and an ERROR *"qos not
supported"* (it went to the NULL queue: SIGSEGV). A QoS 0 message goes out; a
QoS 1 PUBLISH of the broker is delivered and acked; a SUBSCRIBE asked in QoS 2
goes out in QoS 0 (QoS 2); a QoS 2 PUBLISH disconnects the client (DISCONNECT
`0x93`, a WARNING) and is not delivered (its payload was released twice).
Then with queues (a `C_TRANGER` of the driver): the user's QoS 1 publish
inside `EV_ON_OPEN` goes out (the queues opened after it: SIGSEGV), and a
PUBLISH announced at 300000 bytes is refused at its header, DISCONNECT `0x82`
(a client kept what it was announced). Then a hostile broker, one connection
per step, each a WARNING: a second CONNACK in session, DISCONNECT `0x82`, with
queues and without (the queues were opened again and their first handles
leaked; the user heard `EV_ON_OPEN` again); a DISCONNECT with properties and
a PUBLISH behind it in the same read: the PUBLISH is not delivered and the
properties are freed (delivered, leaked); a PUBLISH of QoS 3 and one to
`a/#` and to `a/+/b`: DISCONNECT `0x81` (dropped in silence; delivered); a
PUBLISH of QoS 0 with DUP: DISCONNECT `0x81` (delivered); a CONNACK of
MQTT 5 that refuses (`0x87`): the user hears `EV_ON_CLOSE` with the reason
and nothing is sent (DISCONNECT `0x82`, no `EV_ON_CLOSE`). A PUBLISH whose
topic has 250 levels is delivered: MQTT puts no limit on levels (the client
refused more than 200 separators, the limit of the server, with DISCONNECT
`0x81`), and its user can publish to it, by `EV_MQTT_PUBLISH` and by
`EV_SEND_MESSAGE` (both refused it, `-1`); a wildcard there is still `-1` and
an ERROR. Last, with MQTT 3.1.1, a CONNACK with the reserved code `6`: the user
hears `EV_ON_CLOSE` with `reason_code` 6 and nothing is sent (a protocol
error: a WARNING and no `EV_ON_CLOSE`). Then, with MQTT 5 again, a CONNACK
with the code `0x06` and one with `0x7F`: MQTT 5 defines no code below `0x80`
but Success, so each is a protocol error (ONE WARNING *"Mqtt protocol error:
CONNACK reason code not defined in MQTT 5"* with a text `reason`, no dump,
DISCONNECT `0x82`), and the user hears `EV_ON_CLOSE` with the code (up to
7.30.26 it heard nothing). Last, a CONNACK in the form of 3.1.1 (no
properties) to the MQTT 5 client, with the code `1` and with `4`: the user
hears `EV_ON_CLOSE` with `0x84` and `0x86`, one WARNING *"Mqtt connection
refused"*, nothing sent (up to 7.30.26: *"Error in properties"*, DISCONNECT
`0x81` and no `EV_ON_CLOSE`). Then a CONNACK of MQTT 5 (with its
properties) with the code `2`, a code of 3.1.1: a protocol error as `0x06`
(up to 7.30.26 a refusal with the text *"Granted QoS 2"* and no DISCONNECT);
and a CONNACK in the form of 3.1.1 with the code `0` and with `6`: the user
hears `EV_ON_CLOSE` with `0x81`, one WARNING *"Mqtt protocol error: CONNACK
of MQTT 3.1.1 to an MQTT 5 client"*, nothing sent (up to 7.30.26:
*"Error in properties"*, a dump, DISCONNECT `0x81`, no `EV_ON_CLOSE`). The
alarm is 45 s (the run is about 20 s).

`test_mqtt_gate_cut` (`main_gate_cut.c` + `c_gate_cut.c`) is the cut of a
gate's MQTT 3.1.1 connection over a REAL socket (port 18115): the driver is
the upper level of a server `C_PROT_MQTT2` behind a `C_TCP_S`, and cuts a
connection as a gate's decoder does, posting `EV_DROP` to the protocol, on a
PUBLISH whose payload is `cut` and on a SUBSCRIBE of `cut/#`; after the cut
it takes nothing. A RAW client (a `C_TCP` of the driver) writes each case in
ONE write after its CONNACK. Three QoS 1 PUBLISH and the marker: the client
gets `PUBACK 1` to `PUBACK 4`, then the close, and the gate took the three
readings. `SUBSCRIBE a/b` and `SUBSCRIBE cut/#`: `SUBACK 0x00`, `SUBACK 0x80`,
then the close. No WARNING: the drop does not wait for `timeout_close`. Up to
7.30.26 a server of 3.1.1 (which sends no DISCONNECT) dropped its `C_TCP` at
once, and the `C_TCP` discarded what waited behind its write in flight: the
client got `[PUBACK 1, close]` and `[SUBACK 0x00, close]`. Three more cases
start the close otherwise, and get every answer before it too: three QoS 1
PUBLISH and a DISCONNECT with flags `1` (a protocol error), in 3.1.1 and in
MQTT 5 (where `DISCONNECT 0x82` comes after the three PUBACKs); three QoS 1
PUBLISH, the marker and a malformed packet (type 0, the WARNING *"Mqtt
malformed packet, payload required"*), whose close the `EV_DROP` of the
marker reaches while it waits. Up to 7.30.26 the three got `[PUBACK 1,
close]`. A fake transport cannot show it: it writes at once (its
`EV_TX_READY` comes on the next cycle, as a `C_TCP`'s at the end of its
writes).

`test_mqtt_gate_flood` (`main_gate_flood.c` + `c_gate_flood.c`) is a peer
that asks a gate for answers faster than it reads them, over a REAL socket
(port 18116), with the default `max_unwritten_bytes` (100000) and
`timeout_close` 10 s. A client that reads (twenty bursts of 200 PINGREQ, each
after the answers of the one before) gets its 4000 PINGRESP and is not
dropped: what is bounded is what waits, not the total. A flood of 131072
PINGREQ in one write, and one of 40000 QoS 1 PUBLISH, the client not reading
(`rx_paused`) for 3 s and then reading: the gate pauses its reading while the
answers wait (backpressure), the memory sampled every 20 ms (the yuno's
`timeout_periodic` is 10 ms) grows less than 2 MB, and the client gets every
PINGRESP / PUBACK and is not dropped. 20000 QoS 0 PUBLISH and a PINGREQ in one
write: all taken, the PINGRESP back, not dropped (QoS 0 asks for no answer).
No WARNING. Red on 7.30.26: no bound, the memory grown by 39 MB for the
PINGREQ and 13.6 MB for the PUBLISH (`MEM_MAX_SYSTEM_MEMORY` is 1 GB, so the
red is a failure and not an abort); red on the bounds that came after it
(never released): the client of the floods dropped.

`test_mqtt_gate_answers` (`main_gate_answers.c` + `c_gate_answers.c`) is the
bound on the answers against many connections and big answers, over REAL
sockets (port 18122): raw plain sockets that never read, each with a receive
buffer of 4 KB set before it connects, and the gate's side with a send buffer
of 4 KB, so the kernel takes little; `timeout_close` 500, `timeout_stop_tx`
3 s. 1. 50 connections, each sending exactly 10000 PINGREQ in one write: the
reading of each is paused, and each is dropped when nothing more is written
for `timeout_close`, with one WARNING; the memory grows less than 12 MB
(measured: 6.5-6.8 MB, the bound and one read each, and the clients' own
buffers); then the transport of each says its abort at `timeout_stop_tx`.
2. One connection subscribes 100 filters (granted), then sends SUBSCRIBEs of
16000 times the filter `a`, refused past `max_subscriptions_per_client`, each
answered by a SUBACK of 16 KB: paused, dropped, the memory growing less than
1 MB (measured: 0.2 MB). Red on the bound in packets: case 1 not dropped,
176 MB; case 2 not dropped after 150 SUBSCRIBEs, 2.3 MB; and the drop said a
second time, *"Timeout waiting mqtt disconnected"*.

`test_mqtt_gate_reader` (`main_gate_reader.c` + `c_gate_reader.c`) is the
bound on the answers against peers that pipeline AND read, over REAL sockets
(port 18125): 20 raw plain sockets with the kernel's default buffers, each
sending a CONNECT and then PINGREQ without end (up to 512 KB of them not yet
answered) and reading everything every 20 ms, for 20 s. The reading of each
is paused by the bound again and again and resumed when its answers are
written: none is dropped, each gets its CONNACK and one PINGRESP per PINGREQ,
in order (checked byte by byte), and the memory grows less than 5 MB
(measured: 2.6 MB, the bound and one read each; about 11 million PINGRESP in
the run). Then the clients stop, get the PINGRESP still owed to them, and
close. No WARNING. Red on the first pause (never released): the resume let
the `C_TCP` read before the rest held was read, the rest grew by a read on
each pause, and the memory passed 5 MB in under 2 s.

`test_mqtt_broker_answers` (`main_broker_answers.c` + `c_broker_answers.c`)
runs the broker on port 18123 and RAW MQTT 3.1.1 clients that read
everything: 1. S subscribes to `fc/#` and publishes 30000 QoS 0 to `fc/x` in
one write: it gets the 30000 back and is not dropped (the messages the broker
forwards are not answers). 2. L, as a client of a library, subscribes to
`lib/#` and publishes 2000 QoS 1 with a window of 20 (a new one at each
PUBACK), then 2000 QoS 2 with a window of 20 (PUBREC, PUBREL, PUBCOMP): it
gets every ack and the 4000 messages, and is not dropped. 3. P publishes 3000
QoS 1 in ONE write (no in-flight window, as mqtt.js by default): it gets the
3000 PUBACKs, its reading paused and resumed, not dropped. QoS 2 cannot be
sent that way: past `max_inflight_messages` the broker disconnects. 4. K
sends 30000 PINGREQ in ONE write, reading: the 30000 PINGRESPs, not dropped.
Red on the bound in packets: S dropped after 23 messages; on the bound in
bytes that dropped instead of pausing: P dropped after 2 PUBACKs.

`test_mqtt_broker_stop_drain` (`main_broker_stop_drain.c` +
`c_broker_stop_drain.c`) runs the broker on port 18124 and four raw clients on
plain sockets, receive buffer 4 KB, the broker's side of each with a send
buffer of 4 KB; none reads. A (3.1.1) and B (MQTT 5) send 8000 PINGREQ and
the byte `0xF0`: their reading is paused by the bound on the answers before
the `0xF0`, which is held. C (3.1.1) and D (MQTT 5) send 5 SUBSCRIBEs of 8000
times `a` (40 KB of SUBACKs) and `0xF0` (*"Command unknown or not
implemented"*): their closes wait for the SUBACKs. 1.5 s later the yuno
stops; the driver, stopped right after the broker, closes its sockets and the
kernel resets the connections: the four `EV_ON_CLOSE` reach the broker with
its database closed (four INFOs *"Mqtt connection closed after the broker
stopped, its session left as it is"*), and the reads held are released. No
ERROR, no crash, no leak. Red on 7.30.26: SIGSEGV at the stop (the broker
read its destroyed treedb); with only that fixed, the queues of the closing
connections leaked (*"system memory not free"*).

`test_mqtt_broker_stop_dropped` (`main_broker_stop_dropped.c` +
`c_broker_stop_dropped.c`) runs the broker on port 18127 and four raw clients
on plain sockets that keep their sockets open through the stop: A (3.1.1) and
B (MQTT 5) send 20 SUBSCRIBEs of 16000 times `a` and never read (receive
buffer 4 KB, the broker's side send buffer 4 KB, `timeout_close` 500): each is
dropped with one WARNING *"Mqtt: the peer does not read its answers,
connection dropped"*, its `C_TCP` waiting `timeout_stop_tx`; C subscribes to
`s/#` QoS 1 and never reads, while D (reading) publishes 20000 messages to
`s/x`, QoS 0 and 1 in turn. 3 s later the yuno stops: no transport has ended,
no `EV_ON_CLOSE` comes, and the protocol frees the queues at its destroy. No
ERROR, no leak, exit 0. Red up to the fix: *"system memory not free"*, 21452
blocks (the queues of the sessions, and the QoS 1 messages of C's), exit 255.

`test_mqtt_broker_sessions` (`main_broker_sessions.c` +
`c_broker_sessions.c`) runs the broker on port 18126 and raw clients on plain
sockets. 1 (MQTT 3.1.1) and 2 (MQTT 5): A keeps its session, subscribes to
`q<n>/#` QoS 1, publishes 5 QoS 1 messages to it and does not ack the
forwards; B takes the client id over with a CLEAN session: CONNACK with
session present 0 and nothing more. 3 and 4: the same, B keeping the session:
session present 1 and the 5 messages. 5 (MQTT 5): A as in 2 sends `0xF0`, the
broker answers DISCONNECT and drops it; B with a clean session gets nothing.
6 (3.1.1): 21 QoS 2 PUBLISH without PUBREL against `max_inflight_messages`
20: at most 20 PUBREC, then the close with one WARNING *"Mqtt: QoS 2 PUBLISH
past max_inflight_messages (Receive Maximum), disconnecting"*. 7 and 8 (3.1.1):
A as in 1, then STALLED (receive buffer 4 KB, the broker's side send buffer
4 KB): it reads nothing more and sends 5 SUBSCRIBEs of 8000 times `a` and
`0xF0`, so the broker's close waits for 40 KB of SUBACKs; B takes the session
over while A is still closing, with a clean session (7: nothing) or keeping
it (8: the 5 messages). 9 to 18: what the CLOSE of a connection does to the
queues of its session; B acknowledges what it gets and stays 2.5 s (the queue
backup looks every second). A comes with MQTT 5, Clean Start 1 and Session
Expiry 300 (a session kept): taken over, still connected, by B keeping the
session in MQTT 5 (9) and 3.1.1 (10): session present 1, the 5 messages, no
ERROR; 25 messages (20 in flight), DISCONNECT and close, 5 more published by C,
B back keeping it (11): the 30; 30 messages, STALLED in MQTT 5 as in 7, B
keeping it while A closes (12): the 30; DISCONNECT with Session Expiry 0,
still connected (13), or closing by its keepalive of 1 s (14: one WARNING
*"Client keepalive timeout, closing connection"*), B keeping it (the broker's
record still says 300): session present 1, the 5, no ERROR. What is not kept
is still deleted at the close, B keeping: 3.1.1 clean session (15), MQTT 5
without Session Expiry (16), with a DISCONNECT of Session Expiry 0 (17),
Session Expiry 1 and B 3 s later (18, the broker's expiry): session present 0
and nothing. About 42 s. Red on 7.30.26: B of 1, 2 and 7 got the 5 messages of
A's session; 6 closed with no log; the close of A deleted the topics B had
open: 9, 10, 13 and 14 the ERRORs *"Cannot open topic"* and *"kw must be list
or dict"* every second, 12 a SIGSEGV; and 11 B got 5 of 30.

`test_mqtt_broker_takeover` (`main_broker_takeover.c` +
`c_broker_takeover.c`) runs the broker on port 18120 with `timeout_close`
1000 and two RAW MQTT 5 clients of one client id, clean start: A connects; B
connects (A gets DISCONNECT `0x8E`, Session taken over, though its session is
not kept, and does not close); B disconnects and closes
(its session deleted); A is dropped at `timeout_close`, with the WARNING
*"Timeout waiting mqtt disconnected"*, and its late `EV_ON_CLOSE` finds no
session. No ERROR (up to 7.30.26: *"session NULL"*, and A got DISCONNECT
`0x00`).

`test_mqtt_broker_no_play` (`main_broker_no_play.c` + `c_broker_no_play.c`)
runs the broker started and never played (its service `autoplay` false),
and ends the yuno half a second later: the stop logs no ERROR (up to 7.30.26:
*"gobj NULL"* with a stack, from `broadcast_tranger_queues()` at the stop).
Port 18121, never opened.

`test_mqtt_broker_no_db` (`main_broker_no_db.c` + `c_broker_no_db.c`) runs a
`C_MQTT_BROKER` with `mqtt_persistent_db` false and `on_critical_error` 0:
one CRITICAL *"mqtt_persistent_db false is not supported: ..."* at its start,
the broker does not play, its commands that need the treedb, the input side
or the subscriptions (`list-channels`, `list-sessions`, `list-queues`,
`normal-subs`, `shared-subs`, `flatten-subs`, `list-retains`,
`remove-retains`, `clean-queues`) answer `-1` *"broker not playing"* with no
ERROR while `help` answers `0`, and nothing more is logged in the next two
seconds (up to 7.30.26 it played without a treedb, refused every CONNECT and
logged two ERRORs per tick; its commands logged about 25 ERRORs and answered
as an empty broker). No port, no store.

The tests of `C_PROT_MQTT` (`legacy_prot` and `legacy_sessions`) went with it
after 7.30.26: the gclass was removed once every gate was on `C_PROT_MQTT2`.

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
`out_flight`, `wrong_ack`, `client_pubrec`, `gate_prot`, `gate_limits`,
`client_noqueue`, `gate_cut`, `gate_flood`, `gate_answers`,
`broker_takeover`, `broker_no_play`, `broker_answers`, `broker_stop_drain`,
`gate_reader`, `broker_sessions`, `broker_stop_dropped`)
work in a dir of
their own run, `<$TMPDIR or /tmp>/test_mqtt_<name>.<pid>.<n>`
(`test_work_dir.c`), removed at the end; each binary has a port of its own.
So they run at once (`ctest -j`). Up to 7.25.20 each wiped a fixed
`/tmp/test_mqtt_<name>` at its start.

## Run

```bash
ctest -R '^c_mqtt/' --output-on-failure --test-dir build
```

Requires `CONFIG_MODULE_MQTT=y`.
