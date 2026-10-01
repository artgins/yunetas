(yuno-controlcenter)=
# `controlcenter`

Central management point for a fleet of Yuneta nodes. Nodes **dial in** to the
control center (the agents are the clients). Operators then reach any connected
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
| `__output_url__` | agent config (default `yunetacontrol.com:1994`) | `yunetacontrol.com:1994` |

→ `tcps://x86_64.artgins.yunetacontrol.com:1994` (TLS, default port **1994**).

The secondary [`yuneta_agent22`](yuneta_agent22.md) dials its own endpoint,
default `yunetacontrol.ovh:1995` (port **1995**), and additionally exposes
remote PTY consoles that the control center drives with `write-tty`.

## Reaching nodes through the control center (ycommand)

The control center exposes two gates (defined in its realm config, not in the
binary): `__input_side__` where agents connect, and `__top_side__` where
operators (web / [`ycommand`](#util-ycommand)) connect. In production there
are two control centers, one per agent plane: **1996** (the agents, fed on
1994) and **1997** (`agent22`, fed on 1995).

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
ycommand -w 10 -c 'command-yuno id=<cc> service=controlcenter command=command-agent agent_id=<host-or-uuid> cmd2agent="list-yunos"'
```

Parameters: `agent_id` (UUID or hostname), `agent_service`, and `cmd2agent`
(the command to run on the node — this is the exact key the handler reads). The
control center routes the command to the one matching connected agent
(`gobj_command`, first match only). The node's agent then executes it, and can
itself target a specific yuno via its own `command-yuno` (`command=… service=…`).

`command-agent` answers AT ONCE, before the node does: the first answer is
the control center's own, *"Command sent to 1 nodes"* (or *"... to 0 nodes"*
when no connected agent matches). The node's answer comes later, as a second
answer along the same route. A non-interactive `ycommand` exits `--wait`
seconds after an answer (`-w`, default 1), so it shows the node's answer only
if it arrives in that time. Give it room, or stay interactive (`-i`):

```bash
ycommand -w 10 -c 'command-yuno id=<cc> service=controlcenter command=command-agent agent_id=<host-or-uuid> cmd2agent="list-yunos"'
```

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
{"agent_id": "node1",
 "cmd2agent": "watch-yuno-stats ids=2120,5120 period=2000",
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
ycommand -w 10 -c 'command-yuno id=<cc> service=controlcenter command=command-agent agent_id=node1 cmd2agent="list-yunos"'
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
    "id": "yunovatios-stress",
    "description": "sim_controllers -> gate_central -> db_tracks_ce",
    "group": "yunovatios",
    "node": "yunovatios-controlador",
    "yunos": [
        {"key": "sim",    "id": "stress", "service": "sim_controllers", "rate": "tx"},
        {"key": "gate",   "id": "2120",   "label": "gate_central"},
        {"key": "tracks", "id": "5120",   "label": "db_tracks_ce"}
    ],
    "links": [["sim", "gate"], ["gate", "tracks"]],
    "actions": {
        "start":  [{"yuno": "sim", "command": "set-controllers controllers=10"},
                   {"yuno": "sim", "command": "resume-generation"}],
        "stop":   [{"yuno": "sim", "command": "set-controllers controllers=0"}],
        "report": [{"yuno": "gate", "service": "__yuno__", "command": "view-config"}]
    },
    "view": {"mode": "graph"}
}
```

| Key | Meaning |
|-----|---------|
| `id` | The scenario's name: no blanks, no `^`, no `` ` ``. |
| `node` | The node (agent hostname or UUID) of every yuno that does not say its own. |
| `agent_url` | Instead of `node`: a DIRECT scenario, watched by the web console through its own link to that agent. `run-scenario` refuses it. |
| `yunos` | `id` (the agent's yuno id) and optionally `key` (how the rest of the scenario names it, needed when two nodes carry the same id), `node`, `service`, `label`, `rate` (`rx` or `tx`). |
| `links` | `[from, to]` pairs of keys: how the messages flow. |
| `actions` | `start`, `pause`, `resume`, `stop`, `report`: each a list of steps `{yuno, service?, command}`, one command line with its `key=value` parameters. |
| `view` | How the console shows it (`mode`: `graph` or `cards`). |

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
    -c 'save-scenario scenario={"id":"t1","node":"wattyzer","yunos":[{"id":"1620"}],"actions":{"report":[{"yuno":"1620","command":"help"}]}}'
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
A step goes only when the one before it answered; a step that fails, does not
answer in `run_step_timeout` (30000 ms), or whose node's agent disconnects
meanwhile, ends the run there -- the last at once, not at the deadline. The run is written to `scenario_runs` -- action, user, start and
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
ycommand ... -c 'save-scenario scenario={"id":"t2","node":"wattyzer","yunos":[{"id":"1620"}],"actions":{"stop":[{"yuno":"1620","command":"set-x date=1"}]}}'
# -> ... actions.stop[0]: command: the parameter 'date' is command-yuno's or a yuno's field: it would select the yuno
```

Nor may a parameter start with `__` (`__md_iev__`, `__username__`,
`__md_command__`, ...): those are the framework's keys, and the agent's command
parser sets the parameters of the line over the kw -- a `__md_iev__` would
replace the routing of the step's answer (the run waits for its deadline), a
`__username__` the user the control center stamps.

```bash
ycommand ... -c 'save-scenario scenario={"id":"t3","node":"wattyzer","yunos":[{"id":"1620"}],"actions":{"stop":[{"yuno":"1620","command":"set-x __username__=root"}]}}'
# -> ... actions.stop[0]: command: the parameter '__username__' is a framework key: it would replace what the control center sets
```

`run-scenario` checks the steps of the action again before it sends the first
one, so a scenario saved before a check existed (by 7.25.14, which checked no
step) is refused by name until it is saved again:

```bash
ycommand ... -c 'run-scenario scenario_id=t1 action=stop'
# -> ... cannot run 'stop' of 't1': step 0: command: the parameter 'date' is command-yuno's or a yuno's field: it would select the yuno; save the scenario again
```

## Configuration

The yuno composes `authz` ([`C_AUTHZ`](#gclass-c-authz)) + `controlcenter` (`C_CONTROLCENTER`,
default service). `Authz.max_sessions_per_user` defaults to 4. Key attributes:

| Attribute | Purpose |
|-----------|---------|
| `run_step_timeout` | Milliseconds a step of a scenario run may take to answer (30000) |
| `drops_warning_window` | Milliseconds of the window of a capped warning (60000) |
| `timeout` | Period of the rate tick, ms (1000): `rxMsgsec`/`txMsgsec` and their maxima |

The listen URLs/ports live in the realm config (`__top_side__` /
`__input_side__`), not in the binary.

### Stats

The messages it relays, counted once each way:

- `rxMsgs`, in: a client's `command-agent`, `stats-agent`, `write-tty` and
  `run-scenario`, and every answer or stream an agent sends (command and stats
  answers, the console PTY, `EV_YUNO_STATS`, a run step's answer);
- `txMsgs`, out: each `command-agent`/`stats-agent`/`write-tty` request sent to
  an agent and each step of a run, and each answer or stream relayed to a
  client, the answer of a run included.

`rxMsgsec`/`txMsgsec` are their rates in messages by second, computed on a
tick every `timeout` ms (1000) over the EXACT interval since the previous
tick: a reading only returns the last tick's value, so it is the same
whoever reads and however often. `maxrxMsgsec`/`maxtxMsgsec` keep the highest
rate of any tick, a burst nobody read included; write 0 to start them again.
`stats=__reset__` zeroes the counters, the rates and the maxima. `drops_timer_armed` says whether a capped
warning has a count waiting for its window.

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
| `drop-agent` | Drop a node's connection |
| `write-tty` | Write to a node's PTY console (via `agent22`) |
| `logout-user` | Log out a user session |
| `scenarios` | List the scenarios, or one (`scenario_id`) |
| `save-scenario` | Create or replace a scenario (`scenario`) |
| `delete-scenario` | Delete a scenario and its runs (`scenario_id`) |
| `run-scenario` | Run the steps of an action, in order (`scenario_id`, `action`) |
| `scenario-runs` | The runs of a scenario, newest first (`scenario_id`) |
| `authzs` | Authorization help |
| `help` | Command help |

## Ports

| Port | Used by |
|------|---------|
| `1991` | Agent local plaintext control plane (`ws://127.0.0.1:1991`) |
| `1993` | Agent secure control plane (`wss://0.0.0.0:1993`) |
| `1994` | Primary agent → control center (default `__output_url__`) |
| `1995` | `agent22` → control center |
| `1996` | Control center listener, agents plane (production realm config) |
| `1997` | Control center listener, `agent22` plane (production realm config) |
| `1992` | UDP log sink |

## Debugging

| GClass | Level | Shows |
|--------|-------|-------|
| `C_CONTROLCENTER` | `messages` | Message flow |
