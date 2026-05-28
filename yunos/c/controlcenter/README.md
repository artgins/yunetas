# controlcenter

Central management point (yuno) for a fleet of distributed Yuneta nodes.
`C_CONTROLCENTER` is a **server**: the nodes' agents dial in to it — the
control center never dials out. Administrators then issue commands and query
stats against any connected node *through* the control center, which also keeps
a declared inventory of the fleet in its own treedb.

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

Registered in `src/c_controlcenter.c:96-111`:

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

Talk to it via `ycommand`:

```bash
ycommand -c 'command-yuno id=<cc> service=controlcenter command=list-agents'
ycommand -c 'command-yuno id=<cc> service=controlcenter command=command-agent agent_id=<host> cmd="list-yunos"'
```

The control center finds the matching connected agent (a `C_IEVENT_SRV` child
of its `__input_side__` gate in state `ST_SESSION`) and forwards the command;
the node's agent then runs it (and can itself target a specific yuno via its own
`command-yuno`).

## Data model

`src/treedb_schema_controlcenter.c` declares the control center's own treedb
`treedb_controlcenter`, with topics `systems`, `users`, `nodes`, `services`,
`lists` and `viewer_engines`. The `nodes` topic is the *declared* inventory and
is separate from the set of currently-connected agents (which the control
center discovers live by scanning its `__input_side__` gate for sessions).
Standard graph model — see
[`YUNO_TREEDB.md`](../yuno_agent/YUNO_TREEDB.md).

## Build & deploy

Standard yuneta build (`make install` from the yuno's `build/`), then the
agent's `install-binary` + `create-yuno` + `run-yuno` cycle. See
[`YUNO_LIFECYCLE.md`](../yuno_agent/YUNO_LIFECYCLE.md) §6.

## Where it runs

Typically on a dedicated **company** host (e.g. `artgins.com`), not on
production app hosts. Production nodes' agents (`yuno_agent` on :1994 and
`yuno_agent22` on :1995) dial out to this control center.
