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
| `run-scenario`   | Run the steps of an action of a scenario, in order (`scenario_id= action=`).   |
| `scenario-runs`  | The runs of a scenario, newest first (`scenario_id=`).                        |

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

`node` is the scenario's default; a yuno can carry its own. `agent_url`
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
ycommand -c 'command-yuno id=<cc> service=controlcenter command=run-scenario scenario_id=yunovatios-stress action=start'
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
