# controlcenter

`controlcenter` is the yuno that manages a fleet of distributed Yuneta nodes
from one central point. Its gclass `C_CONTROLCENTER` is a server: the agents
of the nodes connect to it, and the control center never connects out to them.
Administrators send commands and stats queries to any connected node through
the control center. The control center also keeps the scenarios of the fleet
in its own treedb. A scenario is a set of yunos that you watch and test
together. It holds the commands of its actions and the runs of those actions.

## Usage

1. Build and install the yuno on a company host. See "Build & deploy" and
   "Where it runs" below.
2. Accept the key of each node with `accept-node`, or open an enrollment. See
   "Each node's key" below.
3. Send commands to the control center with `ycommand`. `<cc>` is the id of
   the controlcenter yuno:

   ```bash
   ycommand -c 'command-yuno id=<cc> service=controlcenter command=list-agents'
   ycommand -w 10 -c 'command-yuno id=<cc> service=controlcenter command=command-agent agent_id=<host> cmd2agent="list-yunos"'
   ```

4. For `command-agent`, give `ycommand` a wait of some seconds (`-w 10`). The
   node answers after the control center.
5. To run an action of a scenario, use `run-scenario`. See "Data model:
   scenarios and their runs" below.

## How nodes connect

A node connects only when it has an owner (`node_owner` ≠ `none`). Two client
services on the node connect out to the control center over TLS:

- The primary [`yuno_agent`](../yuno_agent/). Its client is a `controlcenter`
  `C_IEVENT_CLI` service, with the default endpoint port 1988
  (`yunetacontrol.com:1988`).
- The secondary [`yuno_agent22`](../yuno_agent22/). It has its own client,
  with the default endpoint port 1989 (`yunetacontrol.ovh:1989`). It also
  exposes remote consoles on a PTY, and the control center drives them with
  `write-tty`.

Both agents trust ONLY the certificate of the control center,
`/yuneta/agent/certs/controlcenter.crt`. This certificate comes with the
agent, and its key stays on the machine of the control center. The agents
test it against the name `yuneta_agent.yuneta.io`. The control center serves
that certificate on 1988 and 1989:

```json
"crypto": {
    "ssl_certificate": "/yuneta/store/certs/controlcenter.crt",
    "ssl_certificate_key": "/yuneta/store/certs/private/controlcenter.key"
}
```

Up to 7.30.3 the agents connected to 1994 / 1995 and trusted
`yuneta_agent.crt`. That is the certificate that every agent serves on its own
port. A control center continues to serve 1994 and 1995, with that
certificate, for the agents of those versions.

The remote URL has the form
`tcps://<__sys_machine__>.<__node_owner__>.<__output_url__>`. Both agents
connect, so the control plane stays reachable when one of them is down or in
an upgrade. The two agents upgrade each other, and never both at once.

The control center knows a node by its node uuid, and `command-agent` goes to
the first agent with it. An agent can open its session with the uuid of an
agent that is in session from another host. That happens with a copy of the
disk of a node. The control center then logs an ERROR:
*"Two agents with the same node uuid on other hosts"*. Give the copy its own
uuid with `yuneta-node-uuid`.

## Each node's key (7.30.10+)

The agents present their own key pair as the client certificate of their link
(1988/1989 with `"ssl_verify_mode": "request"`). The control center binds each
node uuid to the key that an operator accepted. The `nodes` topic of its
treedb keeps the keys. The commands are `list-nodes`, `accept-node` and
`refuse-node`, with the permission `manage-nodes`. With `enforce_node_keys`,
the control center drops a node that presents another key, or that is not
accepted. Without it, the control center only records the keys, so you can
accept the running nodes first.

```text
list-nodes state=pending
list-nodes state=knocking
list-nodes state=other-key   # accepted nodes whose agent presents another key (a node reinstalled)
list-nodes hostname=^box- version=^7.31.   # uuid, version, hostname, os: regular expressions (PCRE2)
list-nodes hostname='^web|^db-[0-9]{2}$'        # a PCRE2 pattern of 256 bytes at most, run with limits
accept-node node_id=<uuid> spki_sha256=<the key read on the node>
delete-node node_id=<uuid>    # a node refused or pending, not connected: a machine that is gone
```

A node installed again keeps its uuid and makes a new key. `list-nodes` shows
that key in a row with the state `other-key`, and its comment gives the
command: compare the start of the key with the one read on the node
(`yuneta-agent-cert key`), then `accept-node node_id=<uuid>
presented=<start>` (7.31.26+).

One key is for one node. `accept-node` refuses a key that is accepted for
another node, because a copy of the disk of a node carries its key pair. A
decision drops the sessions of the node that it no longer lets in. A new key
drops the session of the old key, and `refuse-node` drops the agent of the
node. Without `enforce_node_keys`, only a session that logged in by its key is
dropped.

To accept many nodes at once (a migration, a batch of new boxes), open an
enrollment for some minutes (7.31.15+). An enrollment is a time window for
automatic acceptance. While it is open, the control center accepts the key
that the agent presents, for a node with no accepted key. It never accepts the
key in these cases:

- A node that is accepted with another key.
- A refused node.
- A node that was let in with no key (an older agent).
- A uuid that came with two keys.
- A key of another node.

The enrollment closes after its minutes, at `max_nodes`, with
`close-enrollment`, or when the control center pauses. It exists in memory
only.

The table of knocking nodes holds `max_knocking_nodes` uuids (256 by default).
When the table is full, a new knock evicts the oldest one. The evicted uuid
keeps the key that it knocked with, in a second table of `max_evicted_knocks`
uuids (1024 by default). If the evicted uuid knocks again with another key,
the uuid has two keys. The control center keeps no knock of that uuid, and
the enrollment refuses both keys. To accept the node, use `accept-node
node_id=<uuid> spki_sha256=<the key read on the node>`, or forget the uuid with
`delete-node`. If the uuid knocks again with its own key only, the enrollment
accepts it. Thus a batch of nodes larger than the table enrolls. If the second
table is full, a new eviction overwrites a kept key: that is a flood, and the
enrollment closes for `mark_evicted_quarantine` seconds (3600 by default). A flood of knocks cannot put another key under
a uuid. Up to 7.31.29, the flood erased the key, and an enrollment accepted
another key under that uuid. A restart empties both tables.

The control center logs a refused knock the first time in each enrollment.
After that, it logs it once every `knocking_warning_period` seconds, not at
each knock. The same pace applies to the agent of a refused node.

```text
open-enrollment minutes=10 max_nodes=20
close-enrollment
```

Detail: `docs/doc.yuneta.io/yunos/controlcenter.md`, "Each node's key".

## A node is reached by its uuid

`command-agent`, `stats-agent`, `write-tty` and the scenarios name a node by
its uuid or by its hostname.

The uuid is the identity for which the control center accepted the session of
the agent. If an accepted uuid has two sessions, the uuid names the session
that logged in by the key.

The hostname is only what the agent writes in its card. For an accepted node,
the hostname names the node through its record. Only a session that logged in
by the key of the node writes that hostname. For any other node, the hostname
names the node only through the card of its agent in session. The reason is
that any session of its uuid writes the record of a node without a key, so
that record names nobody. An open that is refused writes nothing.

The uuid of an accepted node always names it. The control center refuses any
other name that two nodes give. It also refuses a name that is the uuid of a
session and the hostname of another node (*"is the hostname of more than one
node: give its node uuid"*). `drop-agent` names a node the same way.
`write-tty` goes to one agent only, and the control center refuses it without
`agent_id`. A scenario run in order does not start on such a name. A stop does
what it reaches and names the rest. The stop of a `max_run` tries again in a
minute, and does not wait for a node.

```text
command-agent agent_id=board-b cmd2agent=list-yunos
# -> -1 "...: 'board-b' is the hostname of more than one node: give its node uuid"
```

## Who asks: operator assertions (7.30.9+)

The control center forwards the command of an operator to an agent with
`__username__`. With `operator_assertion_key`, it also adds an operator
assertion, `__operator_assertion__`. This is an ES256 JWT that the control
center signs for that node. Its `aud` is the node uuid, its `sub` is the
operator, and it lives 60 s. The agent verifies it with the public key in its
`main.c`, and believes the `sub`, not the `__username__`. The control center
adds an assertion only for an agent whose identity card says
`knows_operator_assertion`. A `write-tty` carries one too (7.32.5+): the
agents ask its operator for the permission `open-console`, so deploy the
control center before the agents. The key is a private JWK of EC P-256 on
the machine of the control center. In production:

```json
"operator_assertion_key": "/yuneta/store/certs/private/controlcenter-assertion.jwk"
```

Detail and attributes: `docs/doc.yuneta.io/yunos/controlcenter.md`, "Who asks".

## Commands

The commands are in the `command_table` of `src/c_controlcenter.c`:

| Command          | Purpose                                                                       |
|------------------|-------------------------------------------------------------------------------|
| `help`           | List commands.                                                                |
| `authzs`         | Authz help for this service.                                                  |
| `logout-user`    | Force a user to log out (`username=`, permission `logout-user`). `disabled=1` also disables the user, and `disabled=0` enables it. Without `disabled`, the user stays as it is. |
| `list-agents`    | List the connected nodes (UUID, hostname, status). `uuid=`, `version=`, `hostname=`, `os=` are regular expressions (PCRE2, 256 bytes, run with a match limit and a budget of 50 ms per command: a filter that hits its limit is named in the answer, and a spent budget is said; `(*UCP)`, a back-reference, `\G`, `(*COMMIT)`, `(*SKIP)`, `(*NOTEMPTY_ATSTART)` and a newline other than LF are refused), and the agent must match all of them (`list-agents hostname=^box- version=^7.31.`, `list-agents os=^Rocky`). For the hardware of a node, use `command-agent agent_id=<node> agent_service=__yuno__ cmd2agent=info-machine`. |
| `command-agent`  | Forward any command to one node (`SDF_WILD_CMD`). `agent_id` is a UUID, or a hostname that one node has. |
| `stats-agent`    | Get stats from a specific node.                                               |
| `info-machine`   | Show the machine of a node: os, cpu, memory, disks, network interfaces, uptime (`info-machine agent_id=<uuid or hostname>`, permission `list-agents`). Give `ycommand` a `-w`. |
| `drop-agent`     | Drop every session of a node (`agent_id` as for `command-agent`, empty for every agent). |
| `write-tty`      | Write bytes to the PTY console of one node (the `yuno_agent22` backdoor). `agent_id` is required. The agent asks the operator for `open-console` (7.32.5+). |
| `scenarios`      | List the scenarios, or one (`scenario_id=`).                                  |
| `save-scenario`  | Create or replace a whole scenario (`scenario={...}`), after validation. With `revision=` (the `__md_treedb__.g_rowid` it was read at), the command refuses the save when somebody saved the scenario after that revision. |
| `delete-scenario`| Delete a scenario and its runs (`scenario_id=`).                              |
| `run-scenario`   | Run the steps of an action of a scenario, in order (`scenario_id= action=`). `action=on|off scenario_yuno=<key>` turns the switch of one yuno (7.28.6). |
| `scenario-runs`  | The runs of a scenario, newest first (`scenario_id=`).                        |
| `watch-scenario` | Watch the yunos of a scenario on one node (`scenario_id= node= [keys=] [period=] [stop=1]`). The control center builds the `watch-yuno-stats` of the agent HERE, from the scenario, so the command needs only `read-scenarios` on it. |
| `my-permissions` | The permissions of the user who asks: `true`, `false`, or the list of scenarios a scoped role grants it for. |

Besides answers, the control center relays what the agent of a node PUSHES
along the route of a request. That is the PTY of `open-console`
(`EV_TTY_OPEN/DATA/CLOSE`) and, since 7.25.13, the readings of a
`watch-yuno-stats` (`EV_YUNO_STATS`). `command-agent` removes the
`__relays__` of the client. It writes its own `__relays__: ["EV_YUNO_STATS"]`
only when the request of the client said that it takes that event, that is,
when its own `__relays__` names it. The agent refuses a watch whose kw does
not say it. The reason is that a control center that receives an event that
it does not know drops the connection of the agent. Also, a client that does
not know the event (`ycommand`) logs *"Event NOT DEFINED"* on every reading.
Up to 7.25.20 the control center said it for every client. So a `ycommand ...
command-agent cmd2agent="watch-yuno-stats ids=..."` received readings that it
was not able to handle.

Deploy order: deploy gui_agent 0.29.7 or later first, or together with this
control center. An older gui_agent does not send `__relays__` through the
control center. So the agent refuses its watch, and its Monitor uses polling
of `stats-yuno` instead. The Monitor does the same for an agent that refuses a
direct watch.

What an agent sends back goes to one of two places. Usually it reaches the
channel of a web client of `__top_side__`. A request can also come in by the
link of the control center to its own agent
(`ycommand -w 10 -c 'command-yuno id=<cc> service=controlcenter command=command-agent ...'`).
Then the answer reaches that `C_IEVENT_CLI` link, but only for an agent that
the link sent a request to. The channel of the agent remembers the link until
it closes. Nothing else gets the answer. The control center drops with a
warning a route that names any other local service, or a link that did not ask
that agent. Up to 7.25.20 the control center used a service named by the own
hop of the client in the route instead. So the answers of the request through
the local agent went nowhere.

A web client can send an event that only an agent sends (an answer, the PTY,
`EV_YUNO_STATS`). The control center drops it with the warning *"event of an
agent not from the agents' side, dropped"*. It logs this warning at most once
per `drops_warning_window` (ms, default 60000), and `dropped=` counts the
drops. When its window ends, or when the control center stops, the control
center tells what such a capped warning counted after it last spoke (`when=`,
`dropped=`). It never tells it at each connection close, because a client can
loop on that. A new `drops_warning_window` applies at once to the open
windows. A value under 1 is refused, and 60000 is put back.
`drops_timer_armed` (read-only) tells whether a count waits for its window.

To talk to it with `ycommand`, use the commands in "Usage" above.

`command-agent` answers AT ONCE, before the node does. The first answer comes
from the control center itself: *"Command sent to 1 nodes"*. When no connected
agent matches, the answer is -1 and tells why: *"node 'web-01' (11111111-…,
accepted) is not connected, last seen 2026-11-18T23:36:02.0+0100"*, or *"'x'
names no node connected"* (since 7.31.19). The answer of the node comes later,
as a second answer along the same route. A non-interactive `ycommand` exits
`--wait` seconds after an answer (`-w`, default 1). So it shows the answer of
the node only if that answer arrives in that time. Give it room (`-w 10`, as
in the second command of "Usage"), or stay interactive (`-i`).

When the link of the agent is closing, the link refuses the request at once.
The first answer is then the only one, -1 with its reason:
*"controlcenter^cc1: service 'input-7' not in session: Command sent to 0
nodes"*. The same occurs for `stats-agent`, a `write-tty`, and a step of a
scenario run, which fails at once. Up to 7.30.26 the refusal was dropped, and
the operator or the run waited for an answer that nobody sent.

The control center finds the matching connected agent and forwards the
command. That agent is a `C_IEVENT_SRV` child of its `__input_side__` gate in
state `ST_SESSION`. The agent of the node then runs the command. The agent can
itself send it to a specific yuno with its own `command-yuno`.

## Stats

`rxMsgs`/`txMsgs` count the messages that the control center relays, once in
each direction. In: a `command-agent`/`stats-agent`/`write-tty`/`run-scenario`
of a client, or an answer or stream of an agent. Out: a request to an agent, a
step of a run, an answer or stream to a client, or the answer of a run.

`rxMsgsec`/`txMsgsec` are their rates. The control center computes them on a
tick every `timeout` ms (1000), over the exact interval since the last tick.
So a reading returns the same value, whoever reads it. `maxrxMsgsec`/`maxtxMsgsec`
are the highest rates of any tick (write 0 to start again). `stats=__reset__`
sets them all to zero. Up to 7.25.20 nothing counted them, and all six read 0.

## A generator turned by hand: the `switch` of a yuno (7.28.6)

A yuno of a scenario can carry `switch: {on: [steps], off: [steps]}`. The
steps are like those of an action, and a step that names no `yuno` goes to the
yuno of the switch. So you turn one generator on or off while the others
continue. That lets you increase the load one fleet at a time:

```json
{"key": "gen2", "id": "gen2", "service": "sim_load", "rate": "tx", "node": "node-a",
 "switch": {"on":  [{"command": "set-clients clients=100"}, {"command": "resume-load"}],
            "off": [{"command": "pause-load"}, {"command": "set-clients clients=0"}]}}
```

```bash
ycommand ... -c 'run-scenario scenario_id=load-test action=on scenario_yuno=gen2'
```

`on` runs its steps in order, and only while the scenario RUNS. The scenario
runs when its last run that changed its state is a start or a resume. So the
`max_run` stop is always there to turn it off. `off` sends its steps at once,
in any state. The control center writes and reports a switch run like any
other run (*"on gen2 done, 2 steps"*). But a switch run changes neither the
state of the scenario nor its `max_run` clock. So the `stop` of the scenario
must reach every yuno with a switch. gui_agent draws the switch on the card of
the yuno. Details: `docs/doc.yuneta.io/yunos/controlcenter.md`.

## A test left running: `max_run`

The `max_run` of a scenario is a safeguard, in seconds (0 = none, at most a
week). These rules apply to its clock:

- A `start` that did any step schedules its `stop` `max_run` seconds after the
  start ended.
- A `stop` that did any step cancels the scheduled stop.
- Pause and resume leave the clock as it is.

When the stop is due, the control center runs `stop` itself. This is one more
run, written with the name of the control center as the user and the reason
in the comment (*"max_run of 2100 s reached (started by ops@example.com); run
...: stop done, 16 steps"*). If another run is in flight, the stop runs a few
seconds later. A stop that was not able to do anything is written once, with a
warning, and tried again.

If a node of the scenario is connected, the control center tries the stop
again in a minute. That happens when its yuno refused the stop, or did not
answer (perhaps because it was restarting). A save, a report or a switch
inside that minute keeps that delay. Up to 7.29.3 they brought the stop
forward to at once.

If no node is connected, the stop waits for the agent of one of its nodes to
connect. It runs again at that connection, not on a clock. Up to 7.29.1 the
control center tried it every minute, and every try wrote a run for as long as
the node was down. Version 7.29.2 made every stop wait, so a stop refused on a
connected node was never tried again. A save of the scenario or another run of
it does not end the wait.

The control center rebuilds the schedule from the runs when it starts, so a
restart does not lose it. A stop that is already due then waits for an agent,
because no agent is connected yet. `scenarios` shows the schedule as
`stop_at` (epoch seconds), with `stop_waits_node: true` while it waits for a
node. Every run that ends publishes `EV_SCENARIO_RUN_ENDED` (public, no data).
A console that watches a scenario then reads its state again. It sees it
stopped by the control center, or by anybody.

While a scenario runs, saving it needs `run-scenarios` too (7.27.19). A
scenario runs when a run of it is in flight, or when its last run that did
anything is not a stop. Whatever max_run the save leaves, the control center
can run the stop steps that the save writes. So writing them is the same as
running them. A user who can write a scenario but cannot run it gets
*"scenario 'x' runs: saving it needs 'run-scenarios' too"*.

Deleting a running scenario needs `run-scenarios` too (after 7.29.1). Up to
7.29.1, `write-scenarios` was enough. The delete takes away its stop steps and
the stop that its max_run schedules. So nobody can stop its yunos from the
control center any more: *"scenario 'x' runs: deleting it needs
'run-scenarios' too"*. If a user who can run the scenario deletes it, the
answer says *"it was running: its yunos are left as they are"*, and a WARNING
names the user.

```json
{"id": "load-test", "max_run": 2100, "node": "node-b", ...}
```

## Permissions scoped to scenarios

Every scenario command asks for its permission (`read-scenarios`,
`write-scenarios`, `run-scenarios`) with the `scenario_id` that it acts on. So
a role of the `authz` store can scope a permission with its `parameters`
(`{"scenario_id": ["load-test"]}`, C_AUTHZ 7.27.15+). Such a role grants the
permission for those scenarios only. Then `scenarios` lists only them, the
control center refuses the others by id, and `save-scenario` asks for the
permission on the id that it saves.

Such a user watches a scenario live with `watch-scenario`. The control center
builds the watch of the agent from the scenario that it keeps. So the user
never needs `command-agent`, which sends anything to any node.
`my-permissions` tells the console what to offer. Details and the role row:
doc.yuneta.io, *controlcenter* -> *"A user who may see one scenario"*.

The permissions can change while the control center runs (7.27.16+). The
control center watches its `authz` store (`treedb_authzs`). It publishes
`EV_PERMISSIONS_CHANGED`, a public event with no data, in two cases. One is
that a role is created, changed or deleted. The other is that a link whose
parent is a role is made or undone (a user given or taken a role, a child
role). A client subscribed to it on the service `controlcenter` asks
`my-permissions` again. The console then shows the new controls without a
reload. The control center does not watch the own record of a user, because it
changes at every login (its sessions). Also, disabling or deleting a user
already drops its sessions.

## Data model: scenarios and their runs

`src/treedb_schema_controlcenter.c` declares `treedb_controlcenter`, the own
treedb of the control center (`schema_version` 3). It has two topics,
`scenarios` and `scenario_runs`. A run is linked to its scenario
(`scenarios.runs` hook, `scenario_runs.scenario_id` fkey). The connected
agents are NOT in it. The control center discovers them live, when it scans
the `__input_side__` gate for sessions. The users are not in it either. The
`authz` store (`C_AUTHZ`) of the control center holds who can use it.

A scenario is a document that is saved whole:

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

`node` is the default node of the scenario, and a yuno can carry its own.
`queue_service` (7.27.21+) names the service that holds the output queue of a
yuno, when it is not the service that the yuno is watched on. For example, the
queue of a gate towards its store is in `__output_side__`, and its main
service reports none. `watch-scenario` adds that service to the watch
(`ids=7601,7601:__output_side__`). The console shows that queue on the card
and in a chart of its own.

`agent_url` instead of `node` makes a DIRECT scenario. The web console watches
a DIRECT scenario through its own link to that agent, and `run-scenario`
refuses it.

An action is `start`, `pause`, `resume`, `stop` or `report`, and each action
is a list of steps. A step has a yuno of the scenario (its `key`, default its
`id`), an optional service, and one command line with its `key=value`
parameters. A parameter cannot have the name of a parameter of
`command-yuno` (`id`, `service`, `command`). It also cannot have the name of a
column of the `yunos` topic of the agent, because then it selects the yuno. It
cannot start with `__` either (a framework key: `__md_iev__`, `__username__`,
...). `run-scenario` tests the steps again. So it refuses a scenario saved
before such a test existed, and names the step, until the scenario is saved
again.

`run-scenario scenario_id=<id> action=<action>` sends each step to the node of
the yuno as `command-yuno id=<yuno> [service=<svc>] command=<command>`, with
the user who asked. It sends one step at a time: a step goes only when the
step before it answered. The run ends at a step that fails, that does not
answer in `run_step_timeout` (30000 ms), or whose agent disconnects. The
control center then writes the run to `scenario_runs`. The record holds the
action, the user, the start and end, and the result and answer of every step.
For a `report`, it also holds what each step answered (`data`). The requester
gets this record as the answer. Only one run occurs at a time. The control
center takes the answer of a step only from the agent that the step went to.
`command-agent` removes the marks of a step (`__md_iev__` `cc_run`,
`cc_step`) from what a client forwards.

The control center tells a web client apart by its connection, not by its
channel name, because the next client takes that name. Answers
(`command-agent`, `stats-agent`), streams (the PTY of `open-console`,
`EV_YUNO_STATS`) and run answers reach the channel only while it holds the
same connection. This applies since 7.25.15 for the stats and the runs, and
since 7.25.20 for all of them. The control center takes those events only from
`__input_side__`. If a web client sends one itself, the control center drops
that event with a warning. Several consoles can be mirrored through the connection of
one agent. When that connection closes, the client of each console is dropped,
once, and only if it is still the same connection. `write-scenarios` together
with `run-scenarios` is as much as `command-agent`, because the steps run on
the session of the control center.

The commands themselves always ask for the permissions `read-scenarios`,
`write-scenarios` and `run-scenarios`.

```bash
ycommand -c 'command-yuno id=<cc> service=controlcenter command=run-scenario scenario_id=stress-demo action=start'
```

The treedb uses the standard graph model. See
[`YUNO_TREEDB.md`](../yuno_agent/YUNO_TREEDB.md).

## Build & deploy

Use the standard Yuneta build (`make install` from the `build/` of the yuno).
Then run the `install-binary` + `create-yuno` + `run-yuno` cycle of the agent.
See [`YUNO_LIFECYCLE.md`](../yuno_agent/YUNO_LIFECYCLE.md) §6.

## Where it runs

It usually runs on a dedicated company host (for example `artgins.com`), not
on the hosts of production applications. The agents of production nodes
connect out to this control center: `yuno_agent` on :1988 and `yuno_agent22`
on :1989. Agents before 7.30.4 use :1994 and :1995.
