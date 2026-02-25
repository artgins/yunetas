# MQTT — Succinct Developer Summary

**Model:** publish/subscribe over a central **broker** (server). **Clients** publish messages to **topics** and/or subscribe to topic **filters**.

## Transport
- Runs over **TCP** (also WebSocket). Default ports: **1883** (TCP), **8883** (TLS).
- Binary wire protocol; single long-lived connection per client ID.

## Core Concepts
- **Topic:** UTF-8 path (e.g., `sensors/room1/temp`). Payload is **opaque bytes**.
- **Wildcards (subscription only):** `+` = one level, `#` = multi-level. `$` topics are reserved (e.g., `$SYS/…`).
- **QoS levels:**
    - **0** At most once (no ACK).
    - **1** At least once (`PUBACK`); duplicates possible.
    - **2** Exactly once (`PUBREC`→`PUBREL`→`PUBCOMP`); heaviest.
- **Ordering:** In-order per connection; QoS 1/2 messages may be redelivered with `DUP` flag.
- **Retained messages:** Broker stores the **last** retained message per topic; delivered immediately to new subscribers.
- **Last Will:** Broker publishes a will message if the client disconnects ungracefully (optional delay in v5).

## Packets (control types)
`CONNECT/CONNACK, PUBLISH, PUBACK, PUBREC, PUBREL, PUBCOMP, SUBSCRIBE/SUBACK, UNSUBSCRIBE/UNSUBACK, PINGREQ/PINGRESP, DISCONNECT, AUTH(v5)`

## Sessions
- **ClientID** must be unique.
- **Persistent session:** stores subscriptions + in-flight QoS1/2 (and optionally queued msgs).
- **v3.1.1:** `cleanSession` (bool). **v5:** `cleanStart` + **session expiry** (seconds).

## MQTT v5 Highlights
- **Reason codes & properties** on most packets.
- **Flow control:** `Receive Maximum`, `Maximum Packet Size` (~256 MB limit of remaining length), `Topic Alias`.
- **Subscription options:** retain handling, retain-as-published, no-local, per-sub QoS.
- **User Properties** (key/value metadata).
- **Enhanced Auth (`AUTH`)** for challenge/response schemes.
- **Shared subscriptions:** `$share/<group>/<filter>` (load-balanced delivery).

## Reliability & Liveness
- **Keep Alive** (seconds) + `PINGREQ/PINGRESP`. Broker may disconnect on timeout.
- **Message expiry** (v5) and **payload format/content type** indicators (v5).

## Security
- **TLS** for confidentiality/integrity; optional client certs.
- **Username/Password** (simple), **Enhanced Auth** (v5) for SASL-like methods.
- Authorization commonly via broker ACLs per topic filter.

## Notes
- Brokers can **bridge** to other brokers (careful with loops/topic rewrites).
- MQTT-SN is a datagram variant for constrained networks (separate spec).



# MQTT Control Packets — Concise Field Guide

## Fixed header (all packets)
- Byte 1:
    - Bits 7–4: **Type** (1..15 below)
    - Bits 3–0: **Flags**
        - For **PUBLISH**: `DUP | QoS(2b) | RETAIN`
        - For others: **must** be specific constants (usually `0000`; `0010` for SUBSCRIBE/UNSUBSCRIBE/PUBREL)
- Bytes 2..N: **Remaining Length** (varint, max 268,435,455)

---

## MQTT Control Packets summary (v3.1.1 + v5)
Format: `Type (ID)  Dir  Flags  | Variable Header …  | Payload …`

Format: `Type (ID)  Dir  Flags  | Variable Header …  | Payload …`
Legend: C=Client, B=Broker.

1) CONNECT (1)  C→B  0000

Open a new session: conveys protocol level, client ID, auth, will, keep-alive, and (v5) properties.

    | Protocol Name+Level, **Connect Flags** (username, password, will QoS/retain, will flag, cleanSession/cleanStart), **Keep Alive**, **Properties(v5)**
    | **ClientID**, [Will Properties(v5), Will Topic, Will Payload], [Username], [Password]

2) CONNACK (2)  B→C  0000

Broker’s reply to CONNECT: success/failure, session-present, and negotiated (v5) properties/limits.

    | **Ack Flags** (Session Present), **Reason Code** (v5), **Properties(v5)**
    | (none)

3) PUBLISH (3)  C↔B  DUP/QoS/RETAIN

Application message on a topic; may be retained and delivered at requested QoS.

    | **Topic Name**, [**Packet Identifier** if QoS>0], **Properties(v5)**
    | **Application Payload** (opaque bytes)

4) PUBACK (4)   C↔B  0000   — QoS1 ack

Acknowledge a QoS1 PUBLISH; may include a reason code (v5).

    | **Packet Identifier**, [Reason Code(v5)], [Properties(v5)]
    | (none)

5) PUBREC (5)   C↔B  0000   — QoS2 step 1

QoS2 receipt marker: broker/sender records the message to avoid duplicate processing.

    | **Packet Identifier**, [Reason Code(v5)], [Properties(v5)]
    | (none)

6) PUBREL (6)   C↔B  **0010** — QoS2 step 2

Sender requests release/forwarding of the stored QoS2 message; requires flags `0010`.
    
    | **Packet Identifier**, [Reason Code(v5)], [Properties(v5)]
    | (none)

7) PUBCOMP (7)  C↔B  0000   — QoS2 step 3

Final QoS2 completion; both sides can drop in-flight state for the PID.

    | **Packet Identifier**, [Reason Code(v5)], [Properties(v5)]
    | (none)

8) SUBSCRIBE (8) C→B **0010**

Request one or more subscriptions with per-filter options and (v5) properties.
   
    | **Packet Identifier**, **Properties(v5)**
    | Repeated: **Topic Filter**, **Options** (max QoS, NL/no-local, RAP/retain-as-published, RH/retain-handling)

9) SUBACK (9)   B→C  0000
   
Grant/deny each requested subscription (granted QoS in v3.1.1; reason codes in v5).

    | **Packet Identifier**, **Properties(v5)**
    | Repeated: **Reason Code per subscription** (or granted QoS in v3.1.1)

10) UNSUBSCRIBE (10) C→B **0010**

Remove one or more subscriptions; supports (v5) properties.
    
    | **Packet Identifier**, **Properties(v5)**
    | Repeated: **Topic Filter**

11) UNSUBACK (11) B→C 0000

Confirm unsubscription; (v5) can return per-topic reason codes.
    
    | **Packet Identifier**, **Properties(v5)**
    | Repeated: **Reason Code** (v5) (v3.1.1 has none)

12) PINGREQ (12) C→B 0000
    
Keep-alive/liveness probe from client during idle periods.

    | (none)
    | (none)

13) PINGRESP (13) B→C 0000
    
Broker’s response to PINGREQ indicating the connection is alive.

    | (none)
    | (none)

14) DISCONNECT (14) C↔B 0000

Orderly shutdown; may carry reason and (v5) session-expiry instructions.

    | [**Reason Code(v5)**], [**Properties(v5)** e.g., Session Expiry Interval]
    | (none)

15) AUTH (15) (v5) C↔B 0000

Optional extended authentication (e.g., challenge/response) beyond username/password.
    
    | **Reason Code**, **Properties(v5)** (Auth Method, Auth Data, etc.)
    | (none)

## Fixed header (all packets)
- Byte 1: Bits 7–4 **Type**, Bits 3–0 **Flags** (PUBLISH uses `DUP|QoS|RETAIN`; many others must be `0000`, except SUBSCRIBE/UNSUBSCRIBE/PUBREL=`0010`)
- Bytes 2..N: **Remaining Length** (varint, max 268,435,455)

---

## Notes & gotchas
- **Ordering:** In-order per connection. QoS1/2 may be **redelivered** with `DUP=1`.
- **Session:** v3.1.1 `cleanSession`; v5 `cleanStart` + **Session Expiry** property.
- **Flow control (v5):** Receive Maximum, Maximum Packet Size, Topic Alias, Message Expiry.
- **Keep Alive:** Idle > KA ⇒ broker may close; use `PINGREQ/PINGRESP`.
- **Retain:** Broker stores last retained message per topic; retain behavior configurable per-subscription (v5).
- **Shared subs (v5):** `$share/<group>/<filter>` for load-balanced delivery.

Legend: C=Client, B=Broker.



# MQTT Traffic Flows by QoS (concise)

## Prelims
- **Effective delivery QoS** to each subscriber = `min(publish_QoS, subscription_QoS)`.
- **Packet Identifier (PID)** present only when QoS > 0.
- **Redelivery:** If an ack step is missed, sender retries with `DUP=1` (same PID).
- **Offline subscribers:** Brokers may queue QoS1/2 for **persistent sessions**; QoS0 is typically not queued.

---

## 0) QoS 0 — “at most once”
Publisher C1 → Broker B → Subscribers (effective QoS = 0)

Seq:

C1→B: `PUBLISH(topic, QoS=0, [RETAIN], payload)`

B→S*: For each matching sub: `PUBLISH(..., QoS=0)`
    - No PID, no ACKs, no retries at protocol level.

ASCII:

    C1  ->  B  ->  S*
    | PUBLISH(QoS0)
    |--------------> PUBLISH(QoS0)

---

## 1) QoS 1 — “at least once”
Handshake: `PUBLISH` ↔ `PUBACK`

### Online delivery
C1→B: `PUBLISH(topic, QoS=1, PID=n, [RETAIN], payload)`

B→C1: `PUBACK(PID=n)`

B→S*: Deliver per subscriber at `min(1, subQoS)` (i.e., QoS0 or QoS1).

ASCII:

    C1                B                 S*
    |--PUBLISH(n,Q1)->|                 |
    |<-PUBACK(n)------|                 |
    |                 |--PUBLISH(Q0/1)->

### Redelivery (missed PUBACK)
- C1 retries `PUBLISH(PID=n, DUP=1, QoS=1)` until `PUBACK(n)` arrives (on same or a resumed session).

---

## 2) QoS 2 — “exactly once”
Four-step handshake: `PUBLISH` → `PUBREC` → `PUBREL` → `PUBCOMP`

### Online delivery

C1→B: `PUBLISH(topic, QoS=2, PID=n, [RETAIN], payload)`

B→C1: `PUBREC(PID=n)`        // B has recorded receipt, must not reprocess the app message twice

C1→B: `PUBREL(PID=n)` (flags **0010**)

B→C1: `PUBCOMP(PID=n)`

B→S*: Deliver per subscriber at `min(2, subQoS)` (0/1/2).

ASCII:

    C1                    B                          S*
    |--PUBLISH(n,Q2)----->|                          |
    |<----PUBREC(n)-------|                          |
    |-----PUBREL(n)------>|                          |
    |<---PUBCOMP(n)-------|                          |
    |                     |---PUBLISH(Q0/1/2)------->

### Redelivery (mid-flight loss)
- If `PUBREC` lost: C1 retries `PUBLISH(DUP=1, PID=n)`.
- If `PUBREL` lost: C1 retries `PUBREL(PID=n)` until `PUBCOMP`.
- If C1 reconnects with persistent session, in-flight state continues by PID.

---

## Retained message flow (any QoS)
C1→B: `PUBLISH(..., RETAIN=1, QoS=q)`  → B stores **last** retained message per topic.
- Online subs: get it immediately as normal delivery.
- New subscriber Sx:
  Sx→B: `SUBSCRIBE(filter, options)`
  B→Sx: (if match) immediate `PUBLISH(retained copy, QoS = min(stored_q, granted_qos), RETAIN=1)`

---

## Multi-subscriber example
- C1 publishes `QoS2`.
- S0 subscribed with `QoS0` → receives at QoS0.
- S1 subscribed with `QoS1` → receives at QoS1 (its own PUBACK path with broker).
- S2 subscribed with `QoS2` → receives full QoS2 handshake with broker.
  (Each subscriber’s ACKs are on its own connection with its own PIDs.)

---

## Session & reconnect notes
- v3.1.1: `cleanSession=false` → persistent; v5: `cleanStart=false` + `Session Expiry > 0`.
- On reconnect with a persistent session:
    - Unacked QoS1 `PUBLISH` are redelivered with `DUP=1`.
    - QoS2 in-flight state resumes (re-send missing step by PID).
- If session is **not** present (clean start / expired), both sides drop in-flight state; publisher restarts sends from scratch.

---

## Minimal end-to-end examples

### QoS1 end-to-end (happy path)
1. C1 CONNECT → CONNACK
2. C1 SUBSCRIBE `sensors/#` (maxQoS1) → SUBACK(granted=1)
3. C2 PUBLISH `sensors/r1/temp` (QoS1, PID=10)
4. Broker → C2: PUBACK(10)
5. Broker → C1: PUBLISH(QoS1, PID=77) → C1 PUBACK(77)

### QoS2 with mid-loss of PUBCOMP
1–3 as above but QoS2, PID=20
4. B sends PUBREC(20)
5. C2 sends PUBREL(20)
6. B sends PUBCOMP(20) **lost**
7. C2 retries PUBREL(20) after timeout/reconnect
8. B re-sends PUBCOMP(20); C2 completes.
