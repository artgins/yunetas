(yuno-controlcenter)=
# `controlcenter`

Central management point for a fleet of Yuneta nodes. Nodes **dial in** to the
control center (the agents are the clients). Operators then reach any connected
node *through* the control center. `C_CONTROLCENTER` is a **server** — it never
dials out.

```{figure} ../_static/controlcenter_topology.svg
:alt: Nodes dial in to the control center's __input_side__ (agent on 1988, agent22 on 1989); operators reach the __top_side__ on 1997; a command-agent is routed back down the dial-in link to one node's agent, which runs command-yuno on its yunos.
:width: 100%

Nodes **dial in** to `__input_side__`; operators reach `__top_side__`. A
`command-agent` rides **back down** the connection the node opened — which is
how an operator reaches a node it could never dial directly.
```

## How a node connects to a control center

A node connects only when it has an **owner**. The primary
[`yuneta_agent`](yuneta_agent.md) declares an outbound client service named
`controlcenter` (gclass [`C_IEVENT_CLI`](#gclass-c-ievent-cli), chain
`C_IEVENT_CLI → C_IOGATE → C_CHANNEL → C_PROT_TCP4H → C_TCP`, TLS). That service
is `autostart:false` and is started **only** when `node_owner` ≠ `"none"`. An
empty owner is forced to `"none"`, so by default a node does **not** dial out.

To enrol a node, set its `node_owner` (and, if not using the default control
center, its `__output_url__`).

### How the remote URL is built

The agent assembles the control-center URL from config variables:

```
tcps://(^^__sys_machine__^^).(^^__node_owner__^^).(^^__output_url__^^)
```

that is, **`tcps://<machine>.<node_owner>.<output_url>`** where:

| Variable | Source | Example |
|----------|--------|---------|
| `__sys_machine__` | `uname` machine | `x86_64` |
| `__node_owner__` | the node's `node_owner` | `artgins` |
| `__output_url__` | `"global": {"controlcenter.__json_config_variables__"}` of the agent's `main.c`, which its json file can override (7.30.5+) | `yunetacontrol.com:1988` |

→ `tcps://x86_64.artgins.yunetacontrol.com:1988` (TLS, default port **1988**).

The secondary [`yuneta_agent22`](yuneta_agent22.md) dials its own endpoint,
default `yunetacontrol.ovh:1989` (port **1989**), and additionally exposes
remote PTY consoles that the control center drives with `write-tty`.

**The agents trust only the control center's own certificate**,
`/yuneta/agent/certs/controlcenter.crt`, installed with the agent, checked
against the name `yuneta_agent.yuneta.io`. Its key stays on the control
center's machine, which serves it on 1988 and 1989 -- the `crypto` of those
listeners in its realm config:

```json
"crypto": {
    "ssl_certificate": "/yuneta/store/certs/controlcenter.crt",
    "ssl_certificate_key": "/yuneta/store/certs/private/controlcenter.key"
}
```

Up to 7.30.3 the agents dialled **1994** / **1995** and trusted
`yuneta_agent.crt`, the certificate every agent serves on 1993. A control
center keeps a listener on 1994 and 1995, with that certificate, for the
agents of before.

### Each node's key (7.30.10+)

Each agent shows who it is with its own pair: the client certificate of its
link, made on each node. The control center pins every node uuid to the key
that an operator accepted. Without that, anybody with the agents' shared
token could connect claiming the uuid of any node, and receive that node's
commands.

**1 — the listeners of the agents ask for the certificate.** The `crypto` of
1988 and 1989 adds `"ssl_verify_mode": "request"`. A C_TCP_S in that mode asks
the client for its certificate and takes it as it comes: the certificate is
self-signed, so its chain is neither verified nor reported, and the control
center pins its key. Leave the listener of the agents of before (1994) as it
is.

```json
"crypto": {
    "ssl_certificate": "/yuneta/store/certs/controlcenter.crt",
    "ssl_certificate_key": "/yuneta/store/certs/private/controlcenter.key",
    "ssl_verify_mode": "request"
}
```

**2 — the `nodes` topic.** The control center keeps one record per node
uuid. Each record holds:

- `state`: `pending`, `accepted` or `refused`;
- `spki_sha256`: the accepted key;
- `presented_spki`: the key that the last session written in the record
  presented. For a `pending` or `refused` node this is any key, or empty
  when the session presented none. For an `accepted` node it is the other
  key (or empty) that its last session presented, and it is empty when that
  session presented the accepted key. `accept-node` empties it. With
  `enforce_node_keys`, a refused open writes nothing, so the field keeps the
  value of the last open that was let in;
- `hostname` and `peername`: the node's host name and address, as its agent
  gave them. For an `accepted` node only a session that logged in by its key
  writes them: the hostname names the node, so a session of a token cannot
  rename it;
- `first_seen`, `last_seen`, `decided_by` and `decided_at`. An agent that
  opens a session again with the same hostname, address and key writes
  nothing, unless `last_seen` is an hour old: a link that comes and goes does
  not add a record each time. The port of the address is no change: it is
  new at every connection (in 7.30.15 every open from a new port wrote the
  record).

An open that the control center REFUSES writes nothing in `nodes` (7.30.16+).
A node never seen that presents a key is kept in memory instead, as a node
that knocks (`list-nodes` shows it `knocking`, see below). In 7.30.15 a
refused open created or renamed the record of a node that is not accepted,
so whoever held the token could take the hostname of a node without a key.

When an agent opens its session, the control center checks it as follows:

| The agent presents | The node is | Result |
|--------------------|-------------|--------|
| a key | never seen | recorded `pending`, INFO *"A node to accept"*; with `enforce_node_keys` refused, and not recorded: kept `knocking`, INFO *"A node knocking"* |
| its accepted key | `accepted` | let in |
| another key, or none | `accepted` | WARNING; refused if `enforce_node_keys` |
| anything | `refused` | refused if `enforce_node_keys` |
| no key | not accepted | let in while `allow_nodes_without_key` (an agent before 7.30.10) |

When the control center refuses an agent, it logs a WARNING (*"Agent refused:
its node key is not the one accepted"*) and drops the agent's channel. The
agent reconnects on its own schedule.

The identity card of every session is checked when it opens, also of a
session of a token and with `enforce_node_keys` off. When its `id` or its
`host` is not a string, or is longer than a node uuid (63) or a host name
(64), the control center refuses the session with a WARNING (*"Agent refused:
its identity card is not valid"*, the reason in `refusal`) and drops it. For
example, a card `{"id": 7, ...}` gives `refusal: its id is not a string`. In
7.30.15 such a session was let in, and every command that looked for an agent
after it logged an ERROR with a stack.

| Attribute | Default | Meaning |
|-----------|---------|---------|
| `enforce_node_keys` | `false` | Refuse the agents whose key is not the accepted one. Off: the keys are only recorded. |
| `allow_nodes_without_key` | `true` | Let in a node that presents no key and has none accepted. |
| `max_knocking_nodes` | `256` | The nodes knocking (a key not accepted, no token) kept in memory for `list-nodes`; a new one evicts the one that knocked least recently. |

The three are read-only at run time: set them in the `kw` of the
`controlcenter` service in its realm config, and restart it. For example, once
every node has its key accepted, refuse the agents of before and keep a
smaller table of the boxes that knock:

```json
{
    "name": "controlcenter",
    "kw": {
        "enforce_node_keys": true,
        "allow_nodes_without_key": false,
        "max_knocking_nodes": 64
    }
}
```

**3 — accept the nodes** (permission `manage-nodes`; `list-nodes` asks
`list-agents`). Before you accept a node, compare the key it presented with the
key of its own pair, read on the node:

```bash
# on the node
openssl x509 -in /yuneta/agent/certs/yuneta_agent.crt -noout -pubkey \
    | openssl pkey -pubin -outform DER | sha256sum

# through the control center
list-nodes state=pending
accept-node node_id=0f1e2d3c-4b5a-6978-8a9b-0c1d2e3f4a5b spki_sha256=<the key read on the node>
refuse-node node_id=0f1e2d3c-4b5a-6978-8a9b-0c1d2e3f4a5b
```

`accept-node` pins the key you give in `spki_sha256`, read on the node
itself. It never pins the key that was presented, because anyone who reaches
the port can make a key. Each decision is an INFO, *"Node key decided"*.

**One key, one node.** `accept-node` refuses a key that is accepted for
another node, with a WARNING (*"Node not accepted: its key is accepted for
another node"*):

```
accept-node node_id=0f1e2d3c-4b5a-6978-8a9b-0c1d2e3f4a5b spki_sha256=<the key accepted for 3c4d5e6f-...>
# -> -1 "controlcenter^cc: node '0f1e2d3c-...': its key is already accepted for node '3c4d5e6f-7a8b-4c0d-9e2f-3a4b5c6d7e8f'
#        (a copy of that node? give the copy its own pair, and accept that key)"
```

A copy of a node's disk carries the node's pair. Under its own uuid, the copy
could then log in as both nodes. Make a new pair on the copy, and accept that
key.

**A decision holds for the sessions in course.** The control center drops
each session of the node that the decision does not let in any more, with an
INFO (*"A session of the node dropped: its key is not accepted now"*):

- `accept-node` with a new key drops a session that presents the old key;
- `refuse-node` drops every session of the node.

Without `enforce_node_keys`, only a session that logged in by its key
(`agent:<uuid>`, below) is dropped: a session of a token is let in with any
key then, as when it opens. Up to 7.30.14 a session stayed after its key was
revoked, until it reconnected, and `refuse-node` dropped one session of the
node, and only with `enforce_node_keys`.

**The transition, with nobody cut off:**

1. Deploy with `enforce_node_keys` off: every node that connects is recorded
   with its key.
2. Accept each node after you compare its key.
3. Then set `"C_CONTROLCENTER.enforce_node_keys": true` in the config.

The agent and the agent22 of a node reach two control centers (1996 and
1997), and each control center keeps its own `nodes`. Accept the node in both.

**An accepted key is also the agent's login** (7.30.11+). The control center
authenticates an agent's identity card itself (its `mt_authenticate`) when the
agent presents the key accepted for the node uuid of its card. The session is
`agent:<node uuid>` and reaches the service `controlcenter` only. No token is
asked for, so a node needs no `controlcenter.jwt` in its agent's json. When an
agent presents a key that is not accepted and sends no token, the control
center refuses it. Because that agent is not authenticated, nothing of it goes
to the store. The control center keeps it in memory (at most
`max_knocking_nodes`, 256: a new one evicts the one that knocked least
recently; the first key of a uuid is kept), logs one INFO (*"A node
knocking"*), and `list-nodes` shows it with the state `knocking`. What it says
there (its hostname) is not authenticated: it is only a hint. A new box
therefore connects with nothing but its key, and an operator accepts it with
the key read on the box. `accept-node` creates the record of a node that has
none, whether it is knocking or not. When the key is
for an accepted node but is not the accepted key, the control center logs a
WARNING (*"Agent not authenticated: its node key is not the one accepted"*).
Everything else goes to `C_AUTHZ` as before: the operators, and the agents of
before with their token.

The card of an agent that presents a key is checked before anything of it is
kept, because its agent is not authenticated yet. The control center refuses
the card, with a WARNING (*"Agent not authenticated: its identity card is not
valid"*, and the reason in `refusal`), when:

- its `id` is not a string of 1 to 63 letters, digits and `-` (what
  `node_uuid()` gives);
- its `host` is not a string, or is longer than a host name (64).

Up to 7.30.14 a value that was not a string gave an ERROR with a stack, and
the control center kept an id or a hostname of any length in memory.

### The user the agents log in as

Since 7.30.11 an agent with an accepted key logs in by that key (see above),
not as a user. The user below is for the agents that still present a token:
those of before 7.30.10, and those not yet accepted.

An agent of before opens its session with a JWT of a user of the control
center's `C_AUTHZ`. That session needs a role in the service `controlcenter`, and
nothing more: an agent runs no command on the control center, it only sends
answers and events. Give that user a role whose permission no command asks
for, never `*`:

```json
{
    "id": "agent",
    "disabled": false,
    "description": "A node's agent: its session with the controlcenter, and no command",
    "realm_id": "(^^__realm_id__^^)",
    "parent_role_id": "",
    "service": "controlcenter",
    "permission": "agent-session"
}
```

Every command of an operator asks its own permission (`list-agents`,
`command-agent`, `drop-agent`, `write-tty`, the scenario permissions). So the
token of that user, which is on every node, opens an agent's session and
nothing else: from the operators' port it answers *"No permission to
'list-agents' in service 'controlcenter'"*. A role with `*` there let anybody
who took the token from one node command every node.

That token is old and cannot be renewed on the nodes. With a
`max_token_age` (see [Configuration](#controlcenter-configuration)), put
its user in `Authz.token_age_exempt_users`, or the agents of before cannot
log in.

### Who asks: the operator assertion (7.30.9+)

When the control center forwards a command to an agent, it says who asks in
`__username__`. With `operator_assertion_key` it also signs an **operator
assertion**. The agent verifies the assertion and believes it, not the
`__username__`. The assertion is a JWT (ES256) in `__operator_assertion__`:

| Claim | Value |
|-------|-------|
| `iss` | `operator_assertion_issuer` (`controlcenter`) |
| `sub` | the operator: the user that `C_IEVENT_SRV` authenticated (an email, or a local `yuneta`), or the user who started a scenario run. The stop that a scenario's `max_run` schedules is a run of the control center itself, so its `sub` is `<role>^<name>` of the control center (`controlcenter^cc`); the run's comment names the user who started the scenario |
| `aud` | the node uuid of that agent (the `id` of its identity card) |
| `iat`, `exp` | now, and now + `operator_assertion_lifetime` (60 s) |

The control center adds an assertion to `command-agent`, `stats-agent` and
every step of a scenario run. It adds one only for an agent whose identity
card says `knows_operator_assertion` (7.30.9+). An older agent would hand the
unknown key to its command. A request with no user gets no assertion.

The operator's login token never leaves the control center. An assertion is
good on one node only, for a minute, and never at a login. A node that is
taken can replay it nowhere.

The private key is a JWK of EC P-256. It stays on the control center's
machine, mode 600, and its public half goes into the agents' `main.c`
(`Authz.assertion_jwks`). The config of the `controlcenter` service:

```json
{
    "name": "controlcenter",
    "kw": {
        "operator_assertion_key": "/yuneta/store/certs/private/controlcenter-assertion.jwk"
    }
}
```

| Attribute | Default | Meaning |
|-----------|---------|---------|
| `operator_assertion_key` | `""` | File of the private key. Empty: the control center sends no assertion, and the agents run every request as their principal `controlcenter`. |
| `operator_assertion_issuer` | `controlcenter` | The `iss`: the `kid` of the public key in the agents. |
| `operator_assertion_lifetime` | `60` | Seconds an assertion lives. An agent refuses more than its `max_assertion_lifetime` (300). |

A key that cannot be used makes the control center log an ERROR, *"The key of
the operator assertions is not usable: none is sent"*. Examples are a file
that is not a private EC P-256 JWK, or a yuno whose `C_AUTHZ` did not set
libjwt's allocators.

To make a key, write a P-256 private JWK with `openssl` and a few lines of
script:

```bash
openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out k.pem
openssl pkey -in k.pem -text -noout    # priv: d, pub: 04 || x || y (base64url each)
```

Keep an offline copy of the key. Replacing the key means a new agent release
with the new public half.

### One node, one uuid

A node is known by its node uuid (the `id` of its agent's identity card), and
`command-agent`, `stats-agent` and the scenarios reach a node by it. Two
agents with the same uuid are taken for one node: the first one found gets
the command. A copy of a node's disk gives the same uuid, and so does a board
whose hardware gives every board of its model the same identifier. When an
agent opens its session with the uuid of an agent in session from **another**
host, the control center logs a WARNING (an ERROR from 7.30.3 to 7.30.14:
an agent can cause it):

```
Two agents with the same node uuid on other hosts: a copy of a node, give each its own uuid (yuneta-node-uuid)
    uuid: 5e6f7a8b-9c0d-4e1f-8a2b-3c4d5e6f7a8b, hostname: board-b, hostname2: board-a
```

The same host is not reported: that is a node whose link came back before its
old session was seen to close. To give the copy its own uuid, see
[`node_uuid()`](../api/helpers/misc.md#node_uuid) (`yuneta-node-uuid show` / `ensure` /
`set`), then restart both agents and every yuno of the node.

(controlcenter-node-names)=
### A node is reached by its uuid

`command-agent`, `stats-agent`, `write-tty` and the scenarios name a node by
its uuid or by its hostname, in that order:

- **The uuid** is the identity that the agent's session was accepted for: by
  its key at the login, and against the node's record in `nodes` when the
  session opens.
- **The hostname** is only what the agent writes in its identity card. So it
  names:
  - an **accepted** node through its record in `nodes`, also while the node is
    away. That record takes its hostname only from a session that logged in
    by the node's key;
  - any **other** node (pending, the agents before 7.30.10 with their token)
    through the card of an agent of it in session, and only while it is in
    session. Its record is written by any session of its uuid, a token's
    included, so the record names nobody.

  A `refused` node names nothing.

The uuid of an accepted node always names that node. When two sessions give
the uuid of an accepted node (with `enforce_node_keys` off, a session of a
token is let in with it), the session that logged in by the node's key gets
what goes to the node. Any other name is
refused when two nodes give it as their hostname, or when it is the uuid of a
session and the hostname of another node: a session of a token whose card says
`id: board-b` does not take the commands of the node named `board-b`.
`drop-agent` names a node the same way, and drops every session of it. When
two agents of different nodes in session give one hostname, the control
center logs a WARNING (*"Two agents of different nodes give one hostname:
reach them by their node uuid"*):

```
command-agent agent_id=board-b cmd2agent=list-yunos
# -> -1 "controlcenter^cc: 'board-b' is the hostname of more than one node: give its node uuid"
command-agent agent_id=3c4d5e6f-7a8b-4c0d-9e2f-3a4b5c6d7e8f cmd2agent=list-yunos
# -> 0 "Command sent to 1 nodes"
```

A name that two accepted nodes give stays refused while both records keep it,
also while one of the two nodes is away, until the node that took the other's
name opens its session with its own. A name that a node without a key gives
is refused only while that session is open: when it goes, the name is the
other node's again. For example, the node `node-p` (no key) is in session,
and a session of a token with another uuid gives `node-p`:

```
command-agent agent_id=node-p cmd2agent=list-yunos
# -> -1 "controlcenter^cc: 'node-p' is the hostname of more than one node: give its node uuid"
# ... the other session goes:
command-agent agent_id=node-p cmd2agent=list-yunos
# -> 0 "Command sent to 1 nodes"
```

In 7.30.15 the records of the nodes without a key named them too: a session
of a token that gave the uuid of `node-p` with another hostname renamed its
record, and a session with a new uuid that gave `node-p` got its commands and
its console keys. A session that went away left its record, and the name was
two nodes' for ever, until a `refuse-node` of the other uuid.

The uuid of a PENDING node that is away names no node: an accepted node that
gives that uuid as its hostname gets what is sent to it. Protecting it would
let any session of a token block the hostname of an accepted node for ever,
with one record whose uuid is that hostname.

When `accept-node` accepts a node that was not accepted, its record keeps
a hostname and an address only if the key accepted wrote them: the last
session written in the record presented that key, or the node knocked with
it. Otherwise both are emptied. Any session of the uuid, a token's included,
writes the record of a `pending` node. The node gets its name back at its
next login by its key. For example, node P presented its key once, and then
a session of a token claimed P's uuid and gave `board-b`:

```
list-nodes            # uuid-p  pending  hostname: board-b  presented_spki: (empty)
accept-node node_id=uuid-p spki_sha256=<the key read on P>
list-nodes            # uuid-p  accepted  hostname: (empty)  peername: (empty)
command-agent agent_id=board-b cmd2agent=list-yunos
# -> 0 "Command sent to 1 nodes"    (board-b: the node that has that name)
```

In 7.30.16 and before, P was accepted with the name `board-b`. `board-b`
then named two nodes, and nothing was sent to `board-b` until P logged in by
its key. `write-tty` goes to ONE agent, and is refused without
`agent_id` (a WARNING, *"write-tty without agent_id, nothing written"*). Up to
7.30.14 the first agent whose uuid or hostname matched got the command, so an
agent could take the commands of another node by giving its hostname, or its
uuid as hostname; `write-tty` went to every agent that matched, to all of
them without `agent_id`; and `drop-agent` dropped every agent whose uuid or
hostname matched.

## Reaching nodes through the control center (ycommand)

The control center exposes two gates (defined in its realm config, not in the
binary): `__input_side__` where agents connect, and `__top_side__` where
operators (web / [`ycommand`](#util-ycommand)) connect. In production there
are two control centers, one per agent plane: **1996** (the agents, fed on
1988, and on 1994 by agents before 7.30.4) and **1997** (`agent22`, fed on 1989,
and on 1995 before 7.30.4).

**1 — point `ycommand` at the control center** (role and service are both
`controlcenter`):

```bash
ycommand --url=wss://<cc-host>:1997 --yuno-role=controlcenter --yuno-service=controlcenter -c 'list-agents'
```

or, through an agent that already holds the control-center connection, with the
passthrough form shipped in the controlcenter `README`:

```bash
ycommand -c 'command-yuno id=<cc> service=controlcenter command=list-agents'
```

**2 — list the connected nodes** with `list-agents` (UUID, hostname, status).
Connected nodes are discovered live: the control center scans `__input_side__`
for [`C_IEVENT_SRV`](#gclass-c-ievent-srv) children in state `ST_SESSION` and reads each one's
`identity_card` — a node is addressed by its **UUID** (`identity_card.id`) or
its **hostname** (see [A node is reached by its uuid](#controlcenter-node-names)).

**3 — forward a command to one node** with `command-agent` (a `SDF_WILD_CMD`):

```bash
ycommand -w 10 -c 'command-yuno id=<cc> service=controlcenter command=command-agent agent_id=<host-or-uuid> cmd2agent="list-yunos"'
```

Parameters: `agent_id` (UUID or hostname), `agent_service`, and `cmd2agent`
(the command to run on the node — this is the exact key the handler reads). The
control center routes the command to the one connected agent of that node
(`gobj_command`). The node's agent then executes it, and can
itself target a specific yuno via its own `command-yuno` (`command=… service=…`).

`command-agent` answers AT ONCE, before the node does: the first answer is
the control center's own, *"Command sent to 1 nodes"* (or *"... to 0 nodes"*
when no connected agent matches). The node's answer comes later, as a second
answer along the same route.

When the agent's link is closing (the agent sent its Close, and the link is
still in session until the connection ends), the link refuses the request at
once and nothing is sent. The first answer is then the only one: -1 with the
link's reason, which names the agent's channel in the control center
(`input-7` here). `stats-agent` answers the same way (*"Stats sent to 0
nodes"*):

```text
-1 "controlcenter^cc1: service 'input-7' not in session: Command sent to 0 nodes"
```

Up to 7.30.26 the control center dropped that refusal and answered *"Command
sent to 1 nodes"*: the operator waited for an answer nobody would send. A
`write-tty` refused that way is answered with the link's reason too
(`EV_MT_COMMAND_ANSWER`, -1); up to 7.30.26 the keystrokes were lost with no
answer. A non-interactive `ycommand` exits `--wait`
seconds after an answer (`-w`, default 1), so it shows the node's answer only
if it arrives in that time. Give it room, or stay interactive (`-i`):

```bash
ycommand -w 10 -c 'command-yuno id=<cc> service=controlcenter command=command-agent agent_id=<host-or-uuid> cmd2agent="list-yunos"'
```

The agent answers its own requests with an event of its own, not with
`EV_MT_COMMAND_ANSWER`: `read-file`, `running-keys` and `running-bin` with
`EV_READ_FILE`, `read-json` with `EV_READ_JSON`, `read-binary-file` with
`EV_READ_BINARY_FILE`, `edit-config`, `view-config`, `edit-yuno-config` and
`view-yuno-config` with the event of the same name (`EV_EDIT_CONFIG`, ...).
The control center relays them to the client that asked, as it relays an
answer (`ycommand` takes the seven):

```bash
ycommand -w 10 -c 'command-yuno id=<cc> service=controlcenter command=command-agent agent_id=node-a cmd2agent="read-file filename=/yuneta/agent/yuneta_agent.json"'
# -> 0 "Command sent to 1 nodes", then the file (EV_READ_FILE)
```

Up to 7.30.26 none of those events was public in the control center: the
agent's channel refused the frame (*"Frame of an event that is not public,
refused"*, a WARNING), and the client had only *"Command sent to 1 nodes"*.

### Events pushed by a node, relayed to the web client

Some of what a node's agent sends is not an answer but a stream, pushed along
the route of the request that opened it: the PTY of `open-console`
(`EV_TTY_OPEN`, `EV_TTY_DATA`, `EV_TTY_CLOSE`) and, since 7.25.13, the stats
of a `watch-yuno-stats` (`EV_YUNO_STATS`). The control center relays each of
them to the web client at the end of the route. It must KNOW the event: a
control center that receives one it does not know drops the agent's
connection. And the client at the end must know it too: `ycommand` logs
*"Event NOT DEFINED"* on every reading. So the web client says it takes the
event, in its request:

```json
{"agent_id": "node-a",
 "cmd2agent": "watch-yuno-stats ids=7601,7501 period=2000",
 "__relays__": ["EV_YUNO_STATS"]}
```

`command-agent` removes the client's `__relays__` and, only if it named
`EV_YUNO_STATS`, writes its own `__relays__: ["EV_YUNO_STATS"]` in the kw it
forwards (the client's other entries are not passed on). The agent refuses a
watch without it (*"the client, or the control center in between, does not say
it takes EV_YUNO_STATS (__relays__), ask stats-yuno instead"*), and the client
polls `stats-yuno`. Up to 7.25.20 the control center said it for every client,
so a `ycommand` watch through it got readings it could not handle.

**Deploy order:** gui_agent 0.29.7 or later first, or together with this
control center. An older gui_agent does not send `__relays__` through the
control center: its Monitor is refused the watch and polls instead, the same
fallback it takes with an agent that refuses a direct watch.

Who gets what the agent sends back is the requester named in the control
center's OWN hop of the route: the web client's channel of `__top_side__`, or
the control center's link to its own agent (`agent_client`, a `C_IEVENT_CLI`)
for a request made through the local agent (it is answered twice, see
above: `-w` gives the node's answer time to arrive):

```bash
ycommand -w 10 -c 'command-yuno id=<cc> service=controlcenter command=command-agent agent_id=node-a cmd2agent="list-yunos"'
```

That link is believed only for an agent it asked: the agent's channel
remembers the links whose requests it was sent, and forgets them when it
closes. An agent that routes an answer to a link that never asked it is
dropped with the same warning.

A route that names anything else is dropped with the warning *"answer of an
agent for no requester of this control center, dropped"* (the PTY output with
*"PTY output of an agent for no requester of this control center, dropped"*,
once a minute). Up to 7.25.20 it fell back to a service named by the hop
below, which the client writes, so the answers of a request through the local
agent were sent to the control center itself and lost.

An event that only an agent sends, sent by a web client from `__top_side__`,
is dropped: *"event of an agent not from the agents' side, dropped"*, at most
once a minute, with `dropped=` counting the ones since the last warning.

A stats reading for a web client that is gone (a closed tab) is expected --
the agent learns it only when the watch expires, not renewed -- and is
counted, not logged per reading: one warning a minute at most,

```text
yuno stats for a web client that is gone, dropped (the agent's watch expires)  dropped=42
```

Each capped warning (this one, the PTY of a client that is gone, PTY output
routed to nobody, agent events sent by a web client) says what it counted when
it speaks next. So the count left since the last one is said when its window
ends, or when the control center stops, with `when`:

```text
stream for a web client that is gone, dropped  when="its window ended"  dropped=17
```

The window is the attribute `drops_warning_window`, in milliseconds (default
`60000`: one warning a minute per kind). A new value applies at once to the
windows that are open, and a value under 1 is refused (logged, `60000` put
back). `drops_timer_armed` (read-only) says whether a count is waiting for its
window to end:

```bash
ycommand -c 'command-yuno id=<cc> service=__yuno__ command=write-attr gobj_name=controlcenter attribute=drops_warning_window value=10000'
ycommand -c 'command-yuno id=<cc> service=__yuno__ command=view-attrs gobj_name=controlcenter attribute=drops_timer_armed'
```

Not when a connection closes: a client that connects, sends one, and leaves,
in a loop, would get one warning per loop.

**A web client is its CONNECTION, not its channel name** (since 7.25.15).
The channel a browser holds in `__top_side__` (`top-12`) is taken by the next
client once it closes, so a stream still pushed for the tab that left would
reach whoever connected after it -- up to `watch_ttl` of another user's node
data. Each connection gets a number when it opens, `command-agent` and
`stats-agent` stamp it on the request (`cc_connection`, in the first frame of
the ievent stack, which the agent gives back with its answer and keeps with a
watch or a console), and what comes back -- the answer of a command or of a
stats, the PTY of `open-console` (`EV_TTY_OPEN`, `EV_TTY_DATA`,
`EV_TTY_CLOSE`), a relayed `EV_YUNO_STATS`, the answer of a `run-scenario` --
is delivered only while the channel still holds that connection. A stream
(`EV_YUNO_STATS`, `EV_TTY_DATA`) for another connection is dropped and counted
with the gone ones (`reconnected=its channel holds another connection now` in
the warning); an answer is dropped with a warning each (*"answer for a web
client that is gone, dropped: its channel holds another connection now"*).
When an agent's connection closes, the client of each console mirrored
through it is dropped, once, and only if it is still the connection that
opened that console. Several consoles go through one agent's connection; the
control center keeps their clients per console name, and a console opened
again by another client is that client's -- the agent routes it the same way
(one route per console and control-center channel, refreshed by the re-open).
Up to 7.25.20 the control center kept one client per agent connection: the
last console opened, cleared by the close of any console. Up to 7.25.20 only
`EV_YUNO_STATS` and the run's answer were checked: a slow answer, or a PTY
stream, of the client that left reached the next one.

These events are public because the agents send them, but only an agent may:
one that arrives from `__top_side__` -- a web client sending it itself, with a
route of its own writing -- is dropped with a warning (*"event of an agent
not from the agents' side, dropped"*). Up to 7.25.20 a client could push an
answer or console frames of its making to any other client's channel.

## Scenarios (TreeDB)

The control center's treedb, `treedb_controlcenter` (`schema_version` 3),
keeps the **scenarios** of the fleet and the **runs** of their actions. It
holds neither the connected nodes (discovered live, above) nor the users (who
may use the control center is its `authz` store, [`C_AUTHZ`](#gclass-c-authz)).

```
                      scenarios
            ┌───────────────────────────┐
            │* id                       │
            │  description, group       │
            │  node, agent_url          │
            │  yunos, links             │
            │  actions, view            │
            │                   runs {} │ ◀─┐
            │  created_by/at            │   │
            │  updated_by/at            │   │
            └───────────────────────────┘   │
                    scenario_runs           │
            ┌───────────────────────────┐   │
            │* id                       │   │
            │           scenario_id (↖) │ ──┘
            │  action, username         │
            │  started_at, ended_at     │
            │  result, comment, steps   │
            └───────────────────────────┘
```

A **scenario** is a set of yunos on one or several nodes, how the messages
flow between them, and the commands of each action -- a test to run and
watch, or just a group of yunos to watch. It is a document saved whole:

```json
{
    "id": "stress-demo",
    "description": "sim_load -> gate -> db_events, both nodes",
    "group": "demo",
    "node": "node-a",
    "yunos": [
        {"key": "sim",      "id": "gen1",   "service": "sim_load", "rate": "tx"},
        {"key": "gate_a",   "id": "7601",   "label": "gate",
         "queue_service": "__output_side__"},
        {"key": "events_a", "id": "7501",   "label": "db_events"},
        {"key": "gate_b",   "id": "7601",   "label": "gate", "node": "node-b"},
        {"key": "events_b", "id": "7501",   "label": "db_events", "node": "node-b"}
    ],
    "links": [["sim", "gate_a"], ["gate_a", "events_a"], ["gate_b", "events_b"]],
    "actions": {
        "start":  [{"yuno": "sim", "command": "set-clients clients=10"},
                   {"yuno": "sim", "command": "resume-load"}],
        "stop":   [{"yuno": "sim", "command": "set-clients clients=0"}],
        "report": [{"yuno": "gate_a", "service": "__yuno__", "command": "view-config"}]
    },
    "view": {"mode": "graph"}
}
```

| Key | Meaning |
|-----|---------|
| `id` | The scenario's name: no blanks, no `^`, no `` ` ``. |
| `node` | The node (agent hostname or UUID) of every yuno that does not say its own. |
| `agent_url` | Instead of `node`: a DIRECT scenario, watched by the web console through its own link to that agent. `run-scenario` refuses it. |
| `yunos` | `id` (the agent's yuno id) and optionally `key` (how the rest of the scenario names it, needed when two nodes carry the same id), `node`, `service`, `label`, `rate` (`rx` or `tx`), `queue_service` (the service that holds its output queue, when it is not `service`: `__output_side__` for a gate; 7.27.21+), `switch` (`{on: [steps], off: [steps]}`, turned by hand; 7.28.6+, below). |
| `links` | `[from, to]` pairs of keys: how the messages flow. |
| `actions` | `start`, `pause`, `resume`, `stop`, `report`: each a list of steps `{yuno, service?, command}`, one command line with its `key=value` parameters. |
| `view` | How the console shows it (`mode`: `graph` or `cards`; `node_labels`, a short name per node on its cards, `{"node-b": "b"}`). |
| `max_run` | Seconds a start may run before the control center stops it (0 = no limit, a week at most; 7.27.19+). |

`save-scenario` validates it -- the same rules the console's editor applies --
and writes every column: a key the new document leaves out is emptied, not
kept.

**Two editors of one scenario** (since 7.25.16). `scenarios` answers each
scenario with its treedb metadata, and its `__md_treedb__.g_rowid` is the
REVISION it was read at: every save moves it. Sent back as `revision=`, a save
over a scenario somebody saved (or deleted) since is refused, naming who, and
the editor reads it again instead of the last writer silently winning.
Without `revision` (or with 0) there is no check: a new scenario, or an
overwrite asked for on purpose. `updated_at` is not used for this: it counts
seconds, and two saves in one second look the same.

```bash
ycommand ... -c 'scenarios scenario_id=t1'                # ... "__md_treedb__": {"g_rowid": 7, ...}
ycommand ... -c 'save-scenario revision=7 scenario={...}' # saved: the answer carries g_rowid 8
ycommand ... -c 'save-scenario revision=7 scenario={...}' # -> scenario 't1' was saved by ana since you read it (revision 8, yours 7): read it again
```

```bash
ycommand --url=wss://<cc-host>:1996 --yuno-role=controlcenter --yuno-service=controlcenter \
    -c 'save-scenario scenario={"id":"t1","node":"node-a","yunos":[{"id":"7301"}],"actions":{"report":[{"yuno":"7301","command":"help"}]}}'
ycommand ... -c 'scenarios'
ycommand ... -c 'scenarios scenario_id=t1'
ycommand ... -c 'delete-scenario scenario_id=t1'      # and its runs
```

**Running an action.** `run-scenario scenario_id=<id> action=<action>` sends
each step, in order, to the yuno's node as

```
command-yuno id=<yuno> [service=<service>] command=<command>
```

carrying the user who asked (`__username__`, what the agent's audit records).
How the steps go depends on the action:

- **`start` and `resume` are serial.** A step goes only when the one before
  it answered; a step that fails, does not answer in `run_step_timeout`
  (30000 ms), or whose node's agent disconnects meanwhile, ends the run
  there -- the last at once, not at the deadline. The order is the order of
  the start: what one yuno needs is started first.
- **`stop`, `pause` and `report` send every step at once** (7.28.4+), and
  give them all one `run_step_timeout`. Each step's result is recorded; a
  step that fails does not keep the others from going. The run ends when
  every step has answered, failed, or timed out, with result `-1` if any
  did not succeed, and the comment names them (*"2 of 16 steps failed; step
  0 (gen1): ...; step 5 (gen6): not answered in 30000 ms"*). A step on a node
  whose agent is not connected fails at once; an agent that disconnects
  fails only its own steps. A step whose agent's link is closing fails at
  once too, with the link's reason (*"... not in session"*), in a serial run
  and in a run at once; up to 7.30.26 it waited for `run_step_timeout`. Up to 7.28.3 these were serial too, so a stop
  ended at the first yuno that did not answer, and the yunos after it never
  got the stop (the max_run stop of a scenario whose first simulator was
  already stopped failed every minute, and stopped nothing).

A node of a step that is a name more than one node gives (see
[A node is reached by its uuid](#controlcenter-node-names)) is said as such
(7.30.16+). A serial run (`start`, `resume`, `on`) does not start: it would do
the steps before it and stop there. A run at once does every step it reaches,
and fails the steps on that name:

```
run-scenario scenario_id=t1 action=start
# -> -1 "controlcenter^cc: cannot run 'start' of 't1': node 'board-b' is the hostname of more than one node: give its node uuid"
run-scenario scenario_id=t1 action=stop
# -> -1 "... stop failed, 1 of 2 steps done: 1 of 2 steps failed; step 1 (command-yuno id=5678 command=pause-load): node 'board-b' is the hostname of more than one node: give its node uuid"
```

In 7.30.15 both said *"node 'board-b' is not connected"*, and a start did its
steps up to that one.

The run is written to `scenario_runs` -- action, user, start and
end, the result and the answer of every step, and for a `report` what each
step answered (`data`) -- and the requester is answered with it (the command
answers when the run is OVER). One run at a time.

A step is marked in its `__md_iev__` (`cc_run`, `cc_step`) and its answer is
taken only from the channel of the agent it went to: `command-agent` removes
those two keys from what a client forwards, and an answer that carries them
from anywhere else is dropped with a warning (*"answer of a scenario run step
not from the agent of the step, dropped"*). Up to 7.25.20 a client with
`command-agent` could end, or advance, another user's run with an answer of
its own.

```bash
ycommand ... -c 'run-scenario scenario_id=t1 action=report'
ycommand ... -c 'scenario-runs scenario_id=t1'        # newest first
```

The parameter is `scenario_id`, never `id`: reached through an agent's
`command-yuno`, the whole kw is the filter that picks the yuno, and there `id`
is the yuno's.

The permissions `read-scenarios`, `write-scenarios` and `run-scenarios` are
checked by the commands themselves, always -- not only when the yuno turns
`enable_command_authz` on. The agent runs each step on the control center's
own session, so **`write-scenarios` together with `run-scenarios` is as much
as `command-agent`**: whoever may write a scenario and run it may send any
command to any yuno of a connected node. Grant them as you grant
`command-agent`.

(controlcenter-one-scenario)=
### A user who may see one scenario (7.27.15+)

A role can grant a permission for some scenarios only: its `parameters`
**scope** what it grants ([`C_AUTHZ`](#gclass-c-authz), see
[*Scoped permissions*](#authz-scoped-permissions)). The control center asks
every scenario permission with the `scenario_id` the command acts on -- a kw
it builds, with the authenticated user and nothing else of the request, so a
`scenario_id` put beside another scenario answers nothing; the three
permissions declare `scenario_id` as the key they may be scoped on (7.27.15)
-- and a
role row like this one, in the control center's `authz` store, lets its users
read `load-test` and nothing else:

```json
{
    "id": "load-test-viewer",
    "description": "Sees the scenario load-test",
    "realm_id": "<the realm of the control center>",
    "service": "controlcenter",
    "permission": "read-scenarios",
    "parameters": {"scenario_id": ["load-test"]}
}
```

To let them run its actions too, the same role carries
`"permissions": [{"permission": "run-scenarios"}]` -- an entry without
`parameters` takes the role's. Never `service: "*"` (a superuser) and never
`command-agent` for such a user: that one sends any command to any node.

What the scope does in each command:

| Command | A user whose `read-scenarios` is scoped |
|---------|------------------------------------------|
| `scenarios` | Lists only the scenarios in scope; asked by `scenario_id` for another one, `-1` *"no permission to 'read-scenarios'"* |
| `scenario-runs`, `watch-scenario` | Only for a scenario in scope |
| `save-scenario`, `delete-scenario` | Need `write-scenarios` on that scenario (scoped the same way, on the id saved or deleted), and `run-scenarios` too while it runs |
| `run-scenario` | Needs `run-scenarios` on that scenario |
| `command-agent`, `stats-agent`, `list-agents` | Refused: not granted |

**Watching a scenario without `command-agent`.** The live view of the console
watched the yunos of a scenario with `command-agent cmd2agent="watch-yuno-stats
ids=..."` to each node -- which needs `command-agent`. `watch-scenario` asks
only `read-scenarios` on the scenario, and the control center BUILDS the
watch itself from the scenario it keeps: the yunos of that node (`keys=`, a
comma-separated list of yuno keys, narrows it), with `period=`; `stop=1` ends
it. A yuno with a `queue_service` is watched on that service too, so the
queue of a gate towards its store comes in the same reading. The readings
come back as `EV_YUNO_STATS`, as through `command-agent` (the client says it
takes them in its `__relays__`). The agent never gets a
line the client wrote:

```bash
ycommand ... -c 'watch-scenario scenario_id=load-test node=node-b period=2000'
# -> the agent of node-b gets: watch-yuno-stats ids=gen3:sim_load,gen4:sim_load,gen5:sim_load,7601,7601:__output_side__,7604,7604:__output_side__,7605,7605:__output_side__,7501 period=2000
ycommand ... -c 'watch-scenario scenario_id=load-test node=node-b keys=events'
# -> watch-yuno-stats ids=7501 period=2000
ycommand ... -c 'watch-scenario scenario_id=load-test node=node-b stop=1'
```

**What may I do here?** `my-permissions` answers, for the user who asks, every
permission of the control center: `true`, `false`, or the list of the
scenarios it is granted for. It needs no permission. The console asks it
before it builds its shell, and a user without `command-agent` gets one of
Scenarios only:

```bash
ycommand ... -c 'my-permissions'
# -> {"list-agents": false, "command-agent": false, "drop-agent": false, "logout-user": false,
#     "write-tty": false, "read-scenarios": ["load-test"],
#     "write-scenarios": false, "run-scenarios": ["load-test"]}
```

**When the permissions change** (7.27.16+). The control center watches its
`authz` store and publishes `EV_PERMISSIONS_CHANGED` (public, no data) when a
role is created, changed or deleted, or a role is given, taken, or made the
child of another. A client subscribes to it on the service `controlcenter`
and asks `my-permissions` again; the console does, so a user given
`run-scenarios` while their console is open sees the actions appear without
a reload. An update of a user's own record (its sessions, at every login) is
not announced. Only the store itself is believed: the same event from any
other source is ignored, with a WARNING at most once per 10 s (*"event of the
users store not sent by the store, ignored"*, `MSGSET_AUTH`). A peer cannot
send it anyway: a session's `EV_TREEDB_NODE_*` is refused at the frontier
(7.30.27+).

```js
gobj_subscribe_event(iev, "EV_PERMISSIONS_CHANGED", {__service__: "controlcenter"}, subscriber);
```

**A link to a scenario.** The console shows a saved scenario at
`https://<console>/#/scenarios/live/<id>`, so that address is a link to send:
the user signs in and lands on it.

**A test left running: `max_run`** (7.27.19+). A scenario may say how long a
start may run, in seconds (0 = no limit, a week at most):

```json
{"id": "load-test", "node": "node-b", "max_run": 2100, "yunos": [...], "actions": {...}}
```

A `start` that did any step schedules the scenario's `stop` `max_run` seconds
after it; a `stop` that did any step cancels it; pause and resume leave the
clock alone. When it is due the control center runs `stop` itself, as a run of
its own -- user the control center, the reason in the comment (*"max_run of
2100 s reached (started by ops@example.com); run ...: stop done, 16 steps"*)
-- a few seconds later if
another run is in flight. A stop that could do nothing is written once, with
a warning, and tried again:

- **A node of it is connected** (its yuno refused the stop, or did not
  answer -- restarting, perhaps): it is tried again in a minute (*"the stop
  of a scenario's max_run did nothing, tried again in a minute"*). A save
  of the scenario, a report or a switch inside that minute keeps it (up to
  7.29.3 they brought the stop forward to at once).
- **No node of it is connected:** it waits (*"the stop of a scenario's
  max_run did nothing, tried again when a node of it connects"*), and runs
  again when the agent of one of its nodes connects, not on a clock. A save
  of the scenario or another run of it does not end the wait.
- **A node of it is a name that more than one node gives** (see
  [A node is reached by its uuid](#controlcenter-node-names)): its agents are
  connected, so it is tried again in a minute, like a refused stop. Its step
  says *"node 'board-b' is the hostname of more than one node: give its node
  uuid"*. In 7.30.15 it waited for a node to connect, and nothing ended the
  wait.

A stop that did some steps but not all of them is not the end of the
`max_run` either. The steps that it did not do stay due, and they are tried
again until each one is done. The other steps are not sent again. The rules
are the same as above, applied to the nodes of those steps (*"the stop of a
scenario's max_run did not do every step, the rest tried again in a
minute"*, or *"... the rest tried again when a node of it connects"*). The
schedule rebuilt at start keeps those steps too. For example, the yunos of
`load-test` are on `node-a` and `node-b`, and `node-b` is down when the stop
is due:

```
# the stop of the max_run: node-a stopped, node-b not connected
scenario-runs scenario_id=load-test
# -> "max_run of 2100 s reached (started by ops@example.com); run ...: stop failed, 1 of 2 steps done: ..."
scenarios scenario_id=load-test     # stop_waits_node: true
# node-b connects: the stop goes to node-b only
# -> "...: stop done, 1 steps"
```

While those steps are due the scenario runs, so saving or deleting it needs
`run-scenarios` too (below). The steps that stay due are those that the
scenario still has: a save that takes out or changes one of them drops it,
and when none is left nothing is scheduled. The control center never runs a
step that the run did not have. In 7.30.17 a stop that did some steps made
the scenario look stopped: a user with `write-scenarios` and no
`run-scenarios` could save a new `stop`, and when no step of it matched the
steps due, the control center ran the whole new `stop` by itself. For
example, a user with both permissions changes the stop of the yuno on
`node-b` while that step is due:

```
scenarios scenario_id=load-test     # stop_waits_node: true (node-b's step is due)
save-scenario scenario={... "stop": [..., {"yuno": "gen-b", "command": "stop-load"}]}
scenarios scenario_id=load-test     # no stop_at: node-b's step is not in the scenario now
```

A step answered *"Yuno not found"* did not do its work. The agent gives
that answer for a yuno that is not running, and also for a yuno that is
alive but is not connected to the agent yet: after a restart of the agent,
its yunos come back some seconds later, and the stops that waited for that
agent are tried when it connects. So the stop is tried again in a minute,
as a stop that the yuno refused. In 7.30.18 such a step was done, and the
`max_run` of a yuno that was not connected again yet was lost at each
deploy of the agent.

The tries end:

- **10 tries in a row that did nothing** (each a run, a WARNING and an INFO,
  a minute apart or at each connection of a node): the stop is not tried
  again, with one WARNING, *"the stop of a scenario's max_run did nothing in
  its last tries: not tried again"* (`tries`, and the comment of the last
  one). A try that did some steps starts the count again. `scenarios` then
  answers `stop_given_up: true` (and no `stop_at`), and the scenario runs:
  saving or deleting it needs `run-scenarios` too. A save of the scenario
  tries the stop again, for 10 more tries; a stop by a user ends it. A
  restart of the control center counts all the tries again from the runs,
  so the stop is given up again. In 7.30.18 a stop given up was shown
  nowhere, and a save did not try it again:

  ```
  scenarios scenario_id=load-test     # stop_given_up: true
  save-scenario scenario={"id": "load-test", ...}   # the same scenario
  scenarios scenario_id=load-test     # stop_at (past): the stop is tried again at once
  ```
- **A stop that cannot even start** (the scenario lost its `stop`, or none of
  the steps left is in it, changed outside `save-scenario`) writes no run.
  Each attempt says *"the stop of a scenario's max_run cannot run, tried
  again in a minute"*, and counts towards the same 10, with the tries that the
  runs show; the last one says *"the stop of a scenario's max_run cannot run:
  not tried again"*. These attempts are counted in memory: a save of the
  scenario, or a restart, counts again from the runs. In 7.30.17 such a stop
  was tried every minute, for ever.

In 7.30.17 a step that answered an error for ever was tried every minute,
for ever.

A stop by a user that did some steps cancels the `max_run`, as before: the
steps that it did not do are the user's to run again. In 7.30.16 and before,
a `max_run` stop that did some steps ended the schedule with no log, and the
yunos on a node that was down at that time continued to run.

Up to 7.29.1 every one was tried every minute, and every try wrote a run
while the node was down; 7.29.2 made every one wait for an agent, so a stop
refused on a connected node was never tried again. The schedule is rebuilt
from the runs at start, so a restart does not lose it; a stop already due
then waits for an agent, because no agent is connected yet. `scenarios`
answers it as `stop_at` (epoch seconds; `stop_waits_node: true` while it
waits for a node), and the run in flight of
the scenario, if any, as `run_in_flight` (`id`, `action`, `username`,
`started_at`; 7.28.4+), so a console can say that a start is still going and
not offer another. Every run that ends
publishes `EV_SCENARIO_RUN_ENDED` (public, no data): the console says *"stops
at 18:45"* while it runs, and sees it stopped -- by the control center, or by
anybody -- at once. While a scenario runs (a run of it in flight, its
`max_run` stop due, or its last run that did anything is not a stop), saving
it needs `run-scenarios` on it too (7.27.19; the stop due, 7.30.18): a
max_run saved then schedules the stop steps saved then, so writing them
would be running them. So does deleting it (after 7.29.1): the
delete takes its stop steps and its max_run stop, and leaves its yunos as
they are -- *"scenario 'x' runs: deleting it needs 'run-scenarios' too"*:

```bash
ycommand ... -c 'delete-scenario scenario_id=load-test'   # stop it first, or hold run-scenarios
```

**A generator turned on or off by hand: the `switch` of a yuno** (7.28.6+).
To climb the load one fleet at a time, a yuno may carry a switch: two lists
of steps, `on` and `off`, as those of an action, where a step that names no
`yuno` goes to the switch's own:

```json
{"key": "gen2", "id": "gen2", "service": "sim_load", "rate": "tx", "node": "node-a",
 "switch": {
     "on":  [{"command": "set-streams streams=20"}, {"command": "set-interval interval=1000"},
             {"command": "set-clients clients=100"}, {"command": "resume-load"}],
     "off": [{"command": "pause-load"}, {"command": "set-clients clients=0"}]
 }}
```

```bash
ycommand ... -c 'run-scenario scenario_id=load-test action=on scenario_yuno=gen2'
ycommand ... -c 'run-scenario scenario_id=load-test action=off scenario_yuno=gen2'
```

`save-scenario` refuses a switch without both lists, or with a step that
names no yuno of the scenario. `on` runs in order and ends at the first step
that fails, as a start does; `off` sends every step at once, as a stop does.
`on` is refused unless the scenario RUNS -- its last run that changed its
state is a `start` or a `resume` (*"a switch is turned on only while its
scenario runs"*) -- because a scenario that is not running has no `max_run`
stop to turn it off again. `off` runs in any state. A switch run is a run
like the others (one at a time, under `run-scenarios`, written in
`scenario_runs` with its steps, answered *"run ...: on gen2 done, 4 steps"*,
told by `EV_SCENARIO_RUN_ENDED`), but it changes neither the state of the
scenario nor its `max_run` clock: the scenario's `stop` is what turns every
switch off, so its steps must reach every yuno that has one. `run_in_flight`
carries the `scenario_yuno` of a switch run. The web console draws the switch
on the yuno's card, ON while its throughput is not zero.

**What a step may carry.** A scenario id is letters, digits and `_ . @ -`, not
starting with a dot, 200 characters at most (a run id is `<id>.<ms>`). The
`id`, `key` and `service` of a yuno, and the `service` of a step, are letters,
digits and `_ . ^ -`. A step's `command` is a name and `key=value` parameters
without blanks, and a parameter may NOT be named like a parameter of
`command-yuno` (`id`, `service`, `command`) or like a column of the agent's
`yunos` topic (`yuno_name`, `date`, `global`, `realm_id`, ...): the agent takes
the whole kw of `command-yuno` as the filter that selects the yuno, so such a
parameter would pick another yuno or none. `save-scenario` refuses it:

```bash
ycommand ... -c 'save-scenario scenario={"id":"t2","node":"node-a","yunos":[{"id":"7301"}],"actions":{"stop":[{"yuno":"7301","command":"set-x date=1"}]}}'
# -> ... actions.stop[0]: command: the parameter 'date' is command-yuno's or a yuno's field: it would select the yuno
```

Nor may a parameter start with `__` (`__md_iev__`, `__username__`,
`__md_command__`, ...): those are the framework's keys, and the agent's command
parser sets the parameters of the line over the kw -- a `__md_iev__` would
replace the routing of the step's answer (the run waits for its deadline), a
`__username__` the user the control center stamps.

```bash
ycommand ... -c 'save-scenario scenario={"id":"t3","node":"node-a","yunos":[{"id":"7301"}],"actions":{"stop":[{"yuno":"7301","command":"set-x __username__=root"}]}}'
# -> ... actions.stop[0]: command: the parameter '__username__' is a framework key: it would replace what the control center sets
```

`run-scenario` checks the steps of the action again before it sends the first
one, so a scenario saved before a check existed (by 7.25.14, which checked no
step) is refused by name until it is saved again:

```bash
ycommand ... -c 'run-scenario scenario_id=t1 action=stop'
# -> ... cannot run 'stop' of 't1': step 0: command: the parameter 'date' is command-yuno's or a yuno's field: it would select the yuno; save the scenario again
```

(controlcenter-configuration)=
## Configuration

The yuno composes `authz` ([`C_AUTHZ`](#gclass-c-authz)) + `controlcenter` (`C_CONTROLCENTER`,
default service). `Authz.max_sessions_per_user` defaults to 4.

An operator's token opens a session only while it is young, if
`Authz.max_token_age` is set (7.30.14+, [details](#gclass-c-authz)). The
identity provider can give a token a long life, and this is the control
center's own limit. The agents of before 7.30.10 log in with a token that
cannot be renewed, so their user is exempt:

```json
"global": {
    "Authz.max_token_age": 3600,
    "Authz.token_age_exempt_users": ["agent@example.com"]
}
```

`ycommand` logs in with a new token on each run. The agent console refreshes
its token and connects again when the control center refuses an old one.

Key attributes of `C_CONTROLCENTER`:

| Attribute | Purpose |
|-----------|---------|
| `run_step_timeout` | Milliseconds a step of a scenario run may take to answer (30000) |
| `drops_warning_window` | Milliseconds of the window of a capped warning (60000) |
| `timeout` | Period of the rate tick, ms (1000): `rxMsgsec`/`txMsgsec` and their maxima |

The listen URLs/ports live in the realm config (`__top_side__` /
`__input_side__`), not in the binary. Neither side is autostarted nor
autoplayed: the control center starts both in its `mt_play`, where it
subscribes to them, and stops them in its `mt_pause`. An autostarted
`__input_side__` listens from the yuno's start (`C_TCP_S` listens in its
`mt_start`), so an agent that connects to a control center run with `play=0`
logs *"Publish event WITHOUT subscribers"*, and a paused control center
accepts agents.

```json
{
    "name": "__input_side__",
    "gclass": "C_IOGATE",
    "autostart": false,
    "autoplay": false,
    "kw": {"persistent_channels": false}
}
```

### Stats

The messages it relays, counted once each way:

- `rxMsgs`, in: a client's `command-agent`, `stats-agent`, `write-tty` and
  `run-scenario`, and every answer or stream an agent sends (command and stats
  answers, the console PTY, `EV_YUNO_STATS`, a run step's answer);
- `txMsgs`, out: each `command-agent`/`stats-agent`/`write-tty` request sent to
  an agent and each step of a run (not one the agent's link refused at once),
  and each answer or stream relayed to a client, the answer of a run included.

`rxMsgsec`/`txMsgsec` are their rates in messages by second, computed on a
tick every `timeout` ms (1000) over the EXACT interval since the previous
tick: a reading only returns the last tick's value, so it is the same
whoever reads and however often. `maxrxMsgsec`/`maxtxMsgsec` keep the highest
rate of any tick, a burst nobody read included; write 0 to start them again.
`stats=__reset__` zeroes the counters, the rates and the maxima. `drops_timer_armed` says whether a capped
warning has a count waiting for its window.

The tick is a real period (a `C_TIMER0`, io_uring), and runs only while the
control center plays: paused, the rates read 0. A `timeout` under 1 ms, in the
config or written, is refused with an ERROR and 1000 is put back. Up to
7.25.21 the tick was a `C_TIMER`, which checks its deadline on the yuno's
`timeout_periodic` (1000 ms) and re-arms from when it was served, so a tick of
1000 came every 1 or 2 s and a burst of one second in a tick of two was halved
in the maxima; a `timeout` under 1 in the config was not checked (the tick was
off and the rates read 0), and paused, the rates kept the last tick's value.

```bash
ycommand -c 'stats-yuno id=<cc> service=controlcenter'
# ... "rxMsgs": 1520, "txMsgs": 1519, "rxMsgsec": 12, "txMsgsec": 12, "maxrxMsgsec": 140, ...
```

Up to 7.25.20 nothing counted them and the tick that computed the rates was
never armed: all six read 0.

## Commands

| Command | Description |
|---------|-------------|
| `list-agents` | List currently connected nodes (UUID, hostname, status) |
| `command-agent` | Forward a command to one node (`agent_id`, `agent_service`, `cmd2agent`) |
| `stats-agent` | Fetch stats from a node |
| `drop-agent` | Drop every session of a node (`agent_id`: its uuid, or a hostname that one node has; empty: every agent) |
| `write-tty` | Write to the PTY console of one node (via `agent22`); `agent_id` is required. It is an event (`EV_WRITE_TTY`), not a command line; without the permission `write-tty` it is answered as a command is, `EV_MT_COMMAND_ANSWER` with -1 *"No permission to 'write-tty' in service 'controlcenter'"* (in 7.30.17 the answer was an `EV_WRITE_TTY` that no client takes, and the refusal freed the request twice) |
| `logout-user` | Log out a user (`username`; permission `logout-user`). `disabled=1` also disables the user and `disabled=0` enables it; without `disabled` the user stays as it is |
| `scenarios` | List the scenarios, or one (`scenario_id`) |
| `save-scenario` | Create or replace a scenario (`scenario`) |
| `delete-scenario` | Delete a scenario and its runs (`scenario_id`) |
| `run-scenario` | Run the steps of an action, in order (`scenario_id`, `action`) |
| `scenario-runs` | The runs of a scenario, newest first (`scenario_id`) |
| `watch-scenario` | Watch the yunos of a scenario on one node, the watch built here (`scenario_id`, `node`, `keys`, `period`, `stop`) |
| `my-permissions` | The permissions of the user who asks: `true`, `false`, or the scenarios it is granted for; `EV_PERMISSIONS_CHANGED` says when to ask again |
| `authzs` | Authorization help |
| `help` | Command help |

`logout-user` drops the sessions of a user. `disabled` is passed on only when
it is given:

```bash
ycommand -c 'command-yuno id=<cc> service=controlcenter command=logout-user username=bob@example.com'
ycommand -c 'command-yuno id=<cc> service=controlcenter command=logout-user username=bob@example.com disabled=1'
```

The first leaves bob as he is: up to 7.30.14 it also enabled him if he was
disabled, and no permission was asked.

## Ports

| Port | Used by |
|------|---------|
| `1991` | Agent local plaintext control plane (`ws://127.0.0.1:1991`) |
| `1993` | Agent secure control plane (`wss://0.0.0.0:1993`) |
| `1988` | Primary agent → control center (default `__output_url__`) |
| `1989` | `agent22` → control center |
| `1994` | Primary agent → control center, agents before 7.30.4 |
| `1995` | `agent22` → control center, agents before 7.30.4 |
| `1996` | Control center listener, agents plane (production realm config) |
| `1997` | Control center listener, `agent22` plane (production realm config) |
| `1992` | UDP log sink |

## Debugging

| GClass | Level | Shows |
|--------|-------|-------|
| `C_CONTROLCENTER` | `messages` | Message flow |
