# controlcenter

Central management point (yuno) for a fleet of distributed Yuneta nodes.
`C_CONTROLCENTER` is a **server**: the nodes' agents dial in to it — the
control center never dials out. Administrators then issue commands and query
stats against any connected node *through* the control center, which also keeps
the **scenarios** of the fleet in its own treedb: sets of yunos to watch and
test together, with the commands of their actions, and the runs of those
actions.

## How nodes connect

A node connects only when it has an owner (`node_owner` ≠ `none`). Two client
services on the node dial **out** to the control center over TLS:

- The primary [`yuno_agent`](../yuno_agent/) — a `controlcenter` `C_IEVENT_CLI`
  service, default endpoint port **1994** (`yunetacontrol.com:1994`).
- The secondary [`yuno_agent22`](../yuno_agent22/) — its own client, default
  endpoint port **1995** (`yunetacontrol.ovh:1995`), which additionally exposes
  PTY-backed remote **consoles** that the control center drives with
  `write-tty`.

The remote URL is assembled as
`tcps://<__sys_machine__>.<__node_owner__>.<__output_url__>`. Both agents
connect so that control-plane access survives even if one of them is down or
being upgraded (the two agents upgrade each other, never both at once).

## Commands

Registered in the `command_table` of `src/c_controlcenter.c`:

| Command          | Purpose                                                                       |
|------------------|-------------------------------------------------------------------------------|
| `help`           | List commands.                                                                |
| `authzs`         | Authz help for this service.                                                  |
| `logout-user`    | Force a connected user to log out.                                            |
| `list-agents`    | List currently connected nodes (UUID, hostname, status).                      |
| `command-agent`  | Forward an arbitrary command to a specific node (`SDF_WILD_CMD`). `agent_id` is a UUID or hostname. |
| `stats-agent`    | Get stats from a specific node.                                               |
| `drop-agent`     | Drop the connection to a specific node.                                       |
| `write-tty`      | Write bytes to a node's PTY console (the `yuno_agent22` backdoor).            |
| `scenarios`      | List the scenarios, or one (`scenario_id=`).                                  |
| `save-scenario`  | Create or replace a scenario, whole (`scenario={...}`), validated; `revision=` (the `__md_treedb__.g_rowid` it was read at) refuses it when somebody saved it since. |
| `delete-scenario`| Delete a scenario and its runs (`scenario_id=`).                              |
| `run-scenario`   | Run the steps of an action of a scenario, in order (`scenario_id= action=`); `action=on|off scenario_yuno=<key>` turns the switch of one yuno (7.28.6). |
| `scenario-runs`  | The runs of a scenario, newest first (`scenario_id=`).                        |
| `watch-scenario` | Watch the yunos of a scenario on one node (`scenario_id= node= [keys=] [period=] [stop=1]`): the agent's `watch-yuno-stats` is built HERE from the scenario, so it needs only `read-scenarios` on it. |
| `my-permissions` | The permissions of the user who asks: `true`, `false`, or the list of scenarios a scoped role grants it for. |

Besides answers, the control center relays what a node's agent PUSHES along
the route of a request: the PTY of `open-console` (`EV_TTY_OPEN/DATA/CLOSE`)
and, since 7.25.13, the readings of a `watch-yuno-stats` (`EV_YUNO_STATS`).
`command-agent` removes the `__relays__` of the client and writes its own
`__relays__: ["EV_YUNO_STATS"]` only when the client's request said it takes
that event (its own `__relays__` names it). The agent refuses a watch whose kw
does not say it: a control center that receives an event it does not know
drops the agent's connection, and a client that does not know it (`ycommand`)
logs *"Event NOT DEFINED"* on every reading. Up to 7.25.20 the control center
said it for every client, so a `ycommand ... command-agent
cmd2agent="watch-yuno-stats ids=..."` was sent readings it could not handle.

Deploy order: **gui_agent 0.29.7 or later first** (or together with this
control center). An older gui_agent does not send `__relays__` through the
control center, so its watch is refused and its Monitor falls back to polling
`stats-yuno`, as it does for an agent that refuses a direct watch.

What an agent sends back reaches a web client's channel of `__top_side__`, or,
for a request that came in by the control center's link to its own agent
(`ycommand -w 10 -c 'command-yuno id=<cc> service=controlcenter command=command-agent ...'`),
that `C_IEVENT_CLI` link, and only for an agent it sent a request to (the
agent's channel remembers the link until it closes). Nothing else: a route
that names any other local service, or a link that did not ask that agent, is
dropped with a warning. Up to 7.25.20 the control center fell back
to a service named by the client's own hop of the route, and the answers of the
request through the local agent went nowhere.

An event that only an agent sends (an answer, the PTY, `EV_YUNO_STATS`) sent by
a web client is dropped with the warning *"event of an agent not from the
agents' side, dropped"*, at most once per `drops_warning_window` (ms, default
60000; `dropped=` counts them). What such a capped warning counted after it
last spoke is said when its window ends, or when the control center stops
(`when=`, `dropped=`); never per connection close, which a client could loop.
A new `drops_warning_window` applies at once to the open windows; under 1 it
is refused and 60000 put back. `drops_timer_armed` (read-only) says whether a
count waits for its window.

Talk to it via `ycommand`:

```bash
ycommand -c 'command-yuno id=<cc> service=controlcenter command=list-agents'
ycommand -w 10 -c 'command-yuno id=<cc> service=controlcenter command=command-agent agent_id=<host> cmd2agent="list-yunos"'
```

`command-agent` answers AT ONCE, before the node does: the first answer is
the control center's own, *"Command sent to 1 nodes"* (or *"... to 0 nodes"*
when no connected agent matches). The node's answer comes later, as a second
answer along the same route. A non-interactive `ycommand` exits `--wait`
seconds after an answer (`-w`, default 1), so it shows the node's answer only
if it arrives in that time. Give it room (`-w 10`, as in the second line
above), or stay interactive (`-i`).

The control center finds the matching connected agent (a `C_IEVENT_SRV` child
of its `__input_side__` gate in state `ST_SESSION`) and forwards the command;
the node's agent then runs it (and can itself target a specific yuno via its own
`command-yuno`).

## Stats

`rxMsgs`/`txMsgs` count the messages it relays, once each way (in: a client's
`command-agent`/`stats-agent`/`write-tty`/`run-scenario`, an agent's answer or
stream; out: a request to an agent, a step of a run, an answer or stream to a
client, the answer of a run). `rxMsgsec`/`txMsgsec` are their rates, computed
on a tick every `timeout` ms (1000) over the exact interval since the last
one, so a reading returns the same whoever reads; `maxrxMsgsec`/`maxtxMsgsec`
the highest of any tick (write 0 to start again);
`stats=__reset__` zeroes them all. Up to 7.25.20 nothing counted them and all
six read 0.

## A generator turned by hand: the `switch` of a yuno (7.28.6)

A yuno of a scenario may carry `switch: {on: [steps], off: [steps]}`, steps as
those of an action (a step that names no `yuno` goes to the switch's own), so
one generator is turned on or off while the others go on -- to climb the load
one fleet at a time:

```json
{"key": "gen2", "id": "gen2", "service": "sim_load", "rate": "tx", "node": "node-a",
 "switch": {"on":  [{"command": "set-clients clients=100"}, {"command": "resume-load"}],
            "off": [{"command": "pause-load"}, {"command": "set-clients clients=0"}]}}
```

```bash
ycommand ... -c 'run-scenario scenario_id=load-test action=on scenario_yuno=gen2'
```

`on` runs in order, and only while the scenario RUNS (its last run that changed
its state is a start or a resume), so the `max_run` stop is always there to
turn it off; `off` sends its steps at once, in any state. A switch run is
written and told like any other run (*"on gen2 done, 2 steps"*), but changes
neither the state of the scenario nor its `max_run` clock: the scenario's
`stop` must reach every yuno with a switch. gui_agent draws the switch on the
yuno's card. Details: `docs/doc.yuneta.io/yunos/controlcenter.md`.

## A test left running: `max_run`

A scenario's `max_run` (seconds, 0 = none, at most a week) is a safeguard: a
`start` that did any step schedules its `stop` `max_run` seconds after the
start ended; a `stop` that did any step cancels it; pause and resume leave the
clock as it is. When it is due the control center runs `stop` itself -- one
more run, written with its own name as the user and the reason in the comment
(*"max_run of 2100 s reached (started by ops@example.com); run ...: stop done,
16 steps"*) -- a few seconds
later if another run is in flight. A stop that could do nothing is written
once, with a warning, and tried again. When a node of it is connected (its
yuno refused the stop, or did not answer: restarting, perhaps), it is tried
again in a minute; a save, a report or a switch inside that minute keeps
it (up to 7.29.3 they brought the stop forward to at once). When none is, it waits for the agent of one of its nodes
to connect, and runs again at that connection, not on a clock (up to 7.29.1
it was tried every minute, and every try wrote a run for as long as the node
was down; 7.29.2 made every one wait, so a stop refused on a connected node
was never tried again). A save of the scenario or another run of it does not
end the wait. The schedule is rebuilt from the runs when the control center
starts, so a restart does not lose it; a stop already due then waits for an
agent, as no agent is connected yet. `scenarios` answers it as `stop_at`
(epoch seconds), with `stop_waits_node: true` while it waits for a node. Every run that ends publishes
`EV_SCENARIO_RUN_ENDED` (public, no data): a console watching a scenario reads
its state again, and sees it stopped by the control center, or by anybody.

**While a scenario runs, saving it needs `run-scenarios` too** (7.27.19): it
runs when a run of it is in flight, or its last run that did anything is not a
stop. Whatever max_run the save leaves, the control center may run the stop
steps that save writes, so writing them would be running them -- a user who
may write a scenario but not run it gets *"scenario 'x' runs: saving it needs
'run-scenarios' too"*. **Deleting it too** (after 7.29.1; up to then
`write-scenarios` was enough): its delete takes its stop steps and the stop
its max_run schedules, so nobody stops its yunos from the control center any
more -- *"scenario 'x' runs: deleting it needs 'run-scenarios' too"*. Deleted
by a user who may run it, the answer says *"it was running: its yunos are
left as they are"*, and a WARNING names the user.

```json
{"id": "load-test", "max_run": 2100, "node": "node-b", ...}
```

## Permissions scoped to scenarios

Every scenario command checks its permission (`read-scenarios`,
`write-scenarios`, `run-scenarios`) with the `scenario_id` it acts on, so a
role of the `authz` store whose `parameters` scope a permission
(`{"scenario_id": ["load-test"]}`, C_AUTHZ 7.27.15+) grants it for
those scenarios only: `scenarios` lists only them, the others are refused by
id, and `save-scenario` checks the id it saves. Such a user watches a scenario
live with `watch-scenario` -- the control center builds the agent's watch from
the scenario it keeps -- and never needs `command-agent`, which sends anything
to any node. `my-permissions` tells the console what to offer. Details and the
role row: doc.yuneta.io, *controlcenter* -> *A user who may see one scenario*.

**When it changes** (7.27.16+): the control center watches its `authz` store
(`treedb_authzs`) and publishes `EV_PERMISSIONS_CHANGED` -- a public event with
no data -- when a role is created, changed or deleted, or a link whose parent
is a role is made or undone (a user given or taken a role, a child role). A
client subscribed to it on the service `controlcenter` asks `my-permissions`
again: the console shows the new controls without a reload. A user's own
record is not watched -- it changes at every login (its sessions) -- and
disabling or deleting a user already drops its sessions.

## Data model: scenarios and their runs

`src/treedb_schema_controlcenter.c` declares the control center's own treedb
`treedb_controlcenter` (`schema_version` 3): two topics, `scenarios` and
`scenario_runs`, a run linked to its scenario (`scenarios.runs` hook,
`scenario_runs.scenario_id` fkey). The connected agents are NOT in it: they
are discovered live, scanning the `__input_side__` gate for sessions. Nor are
the users: who may use the control center is its `authz` store (`C_AUTHZ`).

A scenario is a document saved whole:

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

`node` is the scenario's default; a yuno can carry its own. `queue_service`
(7.27.21+) names the service that holds a yuno's output queue when it is not
the one it is watched on: a gate's queue towards its store is in
`__output_side__`, and its main service reports none. `watch-scenario` adds
it to the watch (`ids=7601,7601:__output_side__`), and the console shows that
queue on the card and in a chart of its own. `agent_url`
instead of `node` makes a DIRECT scenario, which the web console watches
through its own link to that agent (and which `run-scenario` refuses). An
action is `start`, `pause`, `resume`, `stop` or `report`, each a list of
steps: a yuno of the scenario (its `key`, default its `id`), an optional
service, and one command line with its `key=value` parameters. A parameter
may not be named like one of `command-yuno`'s (`id`, `service`, `command`) or
a column of the agent's `yunos` topic (it would select the yuno), nor start
with `__` (a framework key: `__md_iev__`, `__username__`, ...). `run-scenario`
checks the steps again, so a scenario saved before a check existed is refused
naming the step until it is saved again.

`run-scenario scenario_id=<id> action=<action>` sends each step as
`command-yuno id=<yuno> [service=<svc>] command=<command>` to the yuno's
node, carrying the user who asked, one at a time: a step goes only when the
one before it answered, and a step that fails, does not answer in
`run_step_timeout` (30000 ms), or whose agent disconnects, ends the run. The
run is then written to `scenario_runs` -- action, user, start and end, the
result and answer of every step, and for a `report` what each step answered
(`data`) -- and the requester is answered with it. One run at a time. A
step's answer is taken only from the agent the step went to: `command-agent`
removes the marks of a step (`__md_iev__` `cc_run`, `cc_step`) from what a
client forwards.

A web client is told apart by its connection, not by its channel name (which
the next client takes): answers (`command-agent`, `stats-agent`), streams (the
PTY of `open-console`, `EV_YUNO_STATS`) and run answers reach the channel only
while it holds the same connection (7.25.15 for the stats and the runs, since
7.25.20 all of them). Those events are taken only from `__input_side__`: a web
client that sends one itself is dropped with a warning. Several consoles can
be mirrored through one agent's connection; when it closes, the client of
each console is dropped (once, and only if it is still the same connection). `write-scenarios` together with
`run-scenarios` is as much as `command-agent`: the steps run on the control
center's session.

Permissions `read-scenarios`, `write-scenarios` and `run-scenarios` are
checked by the commands themselves, always.

```bash
ycommand -c 'command-yuno id=<cc> service=controlcenter command=run-scenario scenario_id=stress-demo action=start'
```

Standard graph model — see [`YUNO_TREEDB.md`](../yuno_agent/YUNO_TREEDB.md).

## Build & deploy

Standard yuneta build (`make install` from the yuno's `build/`), then the
agent's `install-binary` + `create-yuno` + `run-yuno` cycle. See
[`YUNO_LIFECYCLE.md`](../yuno_agent/YUNO_LIFECYCLE.md) §6.

## Where it runs

Typically on a dedicated **company** host (e.g. `artgins.com`), not on
production app hosts. Production nodes' agents (`yuno_agent` on :1994 and
`yuno_agent22` on :1995) dial out to this control center.
