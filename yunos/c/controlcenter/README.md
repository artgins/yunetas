# controlcenter

Central management console (yuno) for distributed Yuneta systems.
Connects to multiple remote agents over `yuno_agent22`'s PTY-backed
backdoor channel, lets administrators issue commands and query stats
against each, and tracks the fleet of agents in its own treedb.

Pairs with [`yuno_agent22`](../yuno_agent22/) — the alternate
backdoor agent that listens for inbound connections from the
controlcenter. The primary [`yuno_agent`](../yuno_agent/) is **not**
involved in this path; controlcenter talks to `yuno_agent22` so the
two channels stay independent (memory `controlcenter` administration
must not lose access if the primary agent goes down).

## Commands

Registered in `src/c_controlcenter.c:98-109`:

| Command          | Purpose                                                                       |
|------------------|-------------------------------------------------------------------------------|
| `help`           | List commands.                                                                |
| `authzs`         | Authz help for this service.                                                  |
| `logout-user`    | Force a connected user to log out.                                            |
| `list-agents`    | List currently connected agents (UUID, hostname, status).                     |
| `command-agent`  | Forward an arbitrary command to a specific agent (`SDF_WILD_CMD`). Agent id is a UUID or hostname. |
| `stats-agent`    | Get stats from a specific agent.                                              |
| `drop-agent`     | Drop the connection to a specific agent.                                      |
| `write-tty`      | Internal — write bytes to a PTY channel.                                      |

Talk to it via `ycommand`:

```bash
ycommand -c 'command-yuno id=<cc> service=controlcenter command=list-agents'
ycommand -c 'command-yuno id=<cc> service=controlcenter command=command-agent agent_id=<host> cmd="list-yunos"'
```

## Data model

`src/treedb_schema_controlcenter.c` declares the controlcenter's own
treedb (users, sessions, agents). Standard graph model — see
[`yunos/c/yuno_agent/TREEDB.md`](../yuno_agent/TREEDB.md).

## Build & deploy

Standard yuneta build (`make install` from the yuno's `build/`),
then the agent's `install-binary` + `create-yuno` + `run-yuno`
cycle. See [`LIFECYCLE.md`](../yuno_agent/LIFECYCLE.md) §6.

## Where it runs

Typically on a dedicated **company** host (e.g. `artgins.com`), not
on production app hosts. The host runs `controlcenter` + the
controlcenter's own auth stack (Keycloak realm); production hosts
run `yuno_agent22` listening for inbound from this controlcenter.
