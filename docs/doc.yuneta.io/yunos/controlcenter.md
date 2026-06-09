(yuno-controlcenter)=
# `controlcenter`

Central management point for a fleet of Yuneta nodes. Nodes **dial in** to the
control center (the agents are the clients); operators then reach any connected
node *through* the control center. `C_CONTROLCENTER` is a **server** — it never
dials out.

```{figure} ../_static/controlcenter_topology.svg
:alt: Nodes dial in to the control center's __input_side__ (agent on 1994, agent22 on 1995); operators reach the __top_side__ on 1997; a command-agent is routed back down the dial-in link to one node's agent, which runs command-yuno on its yunos.
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
is `autostart:false` and is started **only** when `node_owner` ≠ `"none"`; an
empty owner is forced to `"none"`, so by default a node does **not** dial out.

To enrol a node, set its `node_owner` (and, if not using the default control
center, its `__output_url__`).

### How the remote URL is built

The agent assembles the control-center URL from config variables:

```
tcps://(^^__sys_machine__^^).(^^__node_owner__^^).(^^__output_url__^^)
```

i.e. **`tcps://<machine>.<node_owner>.<output_url>`** where:

| Variable | Source | Example |
|----------|--------|---------|
| `__sys_machine__` | `uname` machine | `x86_64` |
| `__node_owner__` | the node's `node_owner` | `artgins` |
| `__output_url__` | agent config (default `yunetacontrol.com:1994`) | `yunetacontrol.com:1994` |

→ `tcps://x86_64.artgins.yunetacontrol.com:1994` (TLS, default port **1994**).

The secondary [`yuneta_agent22`](yuneta_agent22.md) dials its own endpoint,
default `yunetacontrol.ovh:1995` (port **1995**), and additionally exposes
remote PTY consoles that the control center drives with `write-tty`.

## Reaching nodes through the control center (ycommand)

The control center exposes two gates (defined in its realm config, not in the
binary): `__input_side__` where agents connect, and `__top_side__` where
operators (web / [`ycommand`](#util-ycommand)) connect. In production it listens on port
**1997**.

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
its **hostname**.

**3 — forward a command to one node** with `command-agent` (a `SDF_WILD_CMD`):

```bash
ycommand -c 'command-yuno id=<cc> service=controlcenter command=command-agent agent_id=<host-or-uuid> cmd2agent="list-yunos"'
```

Parameters: `agent_id` (UUID or hostname), `agent_service`, and `cmd2agent`
(the command to run on the node — this is the exact key the handler reads). The
control center routes the command to the one matching connected agent
(`gobj_command`, first match only). The node's agent then executes it, and can
itself target a specific yuno via its own `command-yuno` (`command=… service=…`).

## Inventory (TreeDB)

Beyond the live-connection scan, the control center keeps a declared inventory
in `treedb_controlcenter` with topics: `systems`, `users`, `nodes`, `services`,
`lists`, `viewer_engines`. The `nodes` topic (pkey `id`) records
[`description`](https://github.com/artgins/yunetas/blob/7.5.6/utils/c/yuno-skeleton/make_skeleton.c#L199), `provider`, `provider_url`, `properties`, `ip` and links to
`services` / `systems`. This inventory is separate from the set of currently
connected agents.

## Configuration

The yuno composes `authz` ([`C_AUTHZ`](#gclass-c-authz)) + `controlcenter` (`C_CONTROLCENTER`,
default service); `Authz.max_sessions_per_user` defaults to 4. Key attributes:

| Attribute | Purpose |
|-----------|---------|
| `enabled_new_devices` | Auto-accept unknown nodes/devices |
| `enabled_new_users` | Auto-accept unknown users |
| `use_internal_schema` | Use the hardcoded TreeDB schema |
| `timeout` | Periodic tick |

The listen URLs/ports live in the realm config (`__top_side__` /
`__input_side__`), not in the binary.

## Commands

| Command | Description |
|---------|-------------|
| `list-agents` | List currently connected nodes (UUID, hostname, status) |
| `command-agent` | Forward a command to one node (`agent_id`, `agent_service`, `cmd2agent`) |
| `stats-agent` | Fetch stats from a node |
| `drop-agent` | Drop a node's connection |
| `write-tty` | Write to a node's PTY console (via `agent22`) |
| `logout-user` | Log out a user session |
| `authzs` | Authorization help |
| `help` | Command help |

## Ports

| Port | Used by |
|------|---------|
| `1991` | Agent local plaintext control plane (`ws://127.0.0.1:1991`) |
| `1993` | Agent secure control plane (`wss://0.0.0.0:1993`) |
| `1994` | Primary agent → control center (default `__output_url__`) |
| `1995` | `agent22` → control center |
| `1997` | Control center listener (production realm config) |
| `1992` | UDP log sink |

## Debugging

| GClass | Level | Shows |
|--------|-------|-------|
| `C_CONTROLCENTER` | `messages` | Message flow |
