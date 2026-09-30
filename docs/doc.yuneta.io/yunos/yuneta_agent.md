(yuno-yuneta_agent)=
# `yuneta_agent`

The primary agent — the supervisor of a Yuneta node. It manages the lifecycle
of every other yuno (create / run / kill / update / delete), owns the realms
and their on-disk layout, exposes the control plane, and coordinates inter-yuno
communication. [`ycommand`](#util-ycommand), [`ystats`](#util-ystats), [`ylist`](#util-ylist) and [`ybatch`](#util-ybatch) talk to this yuno by
default.

## Control plane ports

| URL | Purpose |
|-----|---------|
| `ws://127.0.0.1:1991` | Local, plaintext control plane (default for `ycommand`) |
| `wss://0.0.0.0:1993` | Secure control plane (TLS + OAuth2) |

## Connecting out to a control center

When the node has an owner (`node_owner` ≠ `"none"`), the agent starts an
outbound [`C_IEVENT_CLI`](#gclass-c-ievent-cli) client service named `controlcenter` and dials the
control center. The remote URL is assembled from config variables:

```
tcps://(^^__sys_machine__^^).(^^__node_owner__^^).(^^__output_url__^^)
```

that is, `tcps://<machine>.<node_owner>.<output_url>`, where `__output_url__`
defaults to `yunetacontrol.com:1994`. With an empty/`none` owner the agent does
not dial out. See [`controlcenter`](controlcenter.md) for the operator side.

## Audit files

The agent writes every command that it runs to a daily audit file in
`/yuneta/realms/agent/agent/audit/` (`use_audit_command_file`, on by default).

- A read-only command (`list-*`, `view-*`, `stats`, `nodes`, `help`, …) is
  recorded with the command, the date and the user only. A command with a
  `__reset__` value (`stats-yuno stats=__reset__`) is not read-only.
- Any other command also gets its `source` (the console purpose and each
  inter-yuno hop: role, yuno, service, user, host) and its parameters.
- A `content64` (a binary, a config) is never written: only its size and the
  sha256 of the decoded content.
- A secret (a parameter named like `password`, `pwd`, `secret`, `token`, `jwt`,
  `api_key`, `cookie`, `authorization`, `private_key`, …) is never written:
  its value is `<redacted>`. So are the token after `Bearer ` and anything
  with the shape of a JWT, wherever they are: also in the `user`, the hops of
  `source`, the console purpose and the console name, which come from a peer
  (a field of those longer than 1024 bytes is written as its size and
  sha256 only). For example a hop whose user is a JWT and whose host has
  4095 bytes is written `"user":"<redacted>"`,
  `"host":"<4095 bytes, not scanned, sha256:HEX>"`.
- The command word is taken as the command parser takes it (any case,
  quotes, aliases): `WRITE-TTY` and `EV_WRITE_TTY` are `write-tty`.
- The scan of a command is one pass in linear time, and one record scans at
  most 128 MB: a longer string is written as its size and sha256 only.
- Every record is flushed to the file before the command runs.
- A console keystroke (`write-tty`) keeps only the fact: who, when, which
  console, how many writes and bytes. Nothing of what was typed, not even a
  hash. The writes of one user into one console make one burst of up to 60
  seconds: its first write is recorded at once, the rest in one record when
  the burst ends.

```json
{"command":"list-yunos","date":"2026-09-24T10:00:00.000000000+0200","user":"claudia@artgins.com"}
{"command":"install-binary id=auth_bff content64='<33554432 bytes sha256:401b36b9…>'","date":"…","user":"yuneta","kw":{}}
{"command":"set-user-pwd username=bob password=<redacted>","date":"…","user":"yuneta","kw":{}}
{"command":"write-tty","date":"…","user":"claudia@artgins.com","console":"console-1","writes":1,"bytes":1}
{"command":"write-tty","date":"…","user":"claudia@artgins.com","console":"console-1","writes":212,"bytes":230,"until":"…"}
```

Up to 7.25.4 an `install-binary` of a 32 MB yuno wrote 134 MB (the base64 three
times); now it writes about 500 bytes.

A day that crosses `max_megas_audit_file` (500 MB) continues in `.OLD.1`,
`.OLD.2`, …; no piece of a day is removed. The attribute `audit_keep_days`
(default `7`, `0` = keep all) is the retention: at start, and when a new audit
file begins (inside the write of its first record, also while the disk is
below `min_free_disk_percentage`), the agent removes the
audit files older than that and logs one INFO line with their names. Only
files with the name shape of the audit mask are removed. A clock set back
across midnight empties no audit file. For example, to keep 30 days, put this in
`/yuneta/agent/yuneta_agent.json` and restart the agent:

```json
{
    "global": {
        "agent.audit_keep_days": 30
    }
}
```

An old `audit/` directory from 7.25.4 or earlier needs no manual cleaning: the
first start removes what is older than `audit_keep_days`. Details and the full
list of read-only commands in [the agent's audit files](#agent-audit-files).

## Listing the node's directories

The `dir-*` commands list a directory tree of the node: `dir-yuneta`,
`dir-realms`, `dir-repos`, `dir-store` (`subdirectory=` below `/yuneta`,
`/yuneta/realms`, `/yuneta/repos`, `/yuneta/store`, and `match=` a regular
expression of the names), `dir-logs` and `dir-local-data` (`id=` of a yuno).
The data is the list of full paths, sorted:

```bash
ycommand -c 'dir-store subdirectory=mqtt_broker match=.*\.json'
```

A tree that cannot be listed -- the directory does not exist, cannot be
opened or cannot be read, a `match` that is not a regular expression, no
memory for an entry -- answers `-1`, and says which directory; the cause is
in the agent's log:

```text
-1: yuneta_agent^agent: cannot list '/yuneta/store/nope', see the log
```

Up to 7.25.4 each of them answered an EMPTY list with result `0`, which reads
as "the directory is empty". A SUBdirectory the agent cannot open is skipped,
as before. (`tests/c/helpers/test_dir_listing`.)

## A yuno that runs but is not connected

`command-yuno`, `stats-yuno` and `authzs-yuno` go to every yuno that matches
and is running, through the channel the yuno opened to the agent. A yuno that
runs but has no channel -- starting, not yet connected, or going -- used to
get nothing and answer nothing, so the requester waited for ever (a control
center running a scenario, its step's whole deadline). Since 7.25.16 the
command is answered at once when it reached none of them,

```text
yuneta_agent^controlador: set-period: yuno stress runs but is not connected to the agent (starting or stopping): nothing sent
```

and when it reached only some, the others are named in a warning of the
agent's log (*"yuno running but not connected to the agent, nothing sent to
it"*): the ones that got it answer on their own.

## Stats pushed to a watcher (`watch-yuno-stats`)

Since 7.25.13 a client can ask the agent to SEND it the stats of some yunos
every period, instead of asking `stats-yuno` again and again. It is what the
Monitor workspace of gui_agent uses when it talks to an agent directly.

```bash
# every 2 s: the state, the cpu and the stats of each yuno's service
watch-yuno-stats ids=stress,2120,5120 period=2000
# `id:service` reads another service than the one named as the role
watch-yuno-stats ids=5120:db_tracks_ce,2 period=5000
# end it (closing the connection ends it too)
watch-yuno-stats stop=1
```

The answer says what is watched:

```text
yuneta_agent^controlador: watching 3 yunos every 2000 ms
```

and from then on the agent sends `EV_YUNO_STATS` along the route the
request came from -- the way the Terminal's PTY mirror sends `EV_TTY_DATA`
-- three kinds per yuno and period, the event's `data` being:

```json
{"yuno_id": "2120", "kind": "state", "missing": false,
 "yuno_running": true, "yuno_playing": true, "yuno_disabled": false}
{"yuno_id": "2120", "kind": "cpu", "service": "", "result": 0, "comment": "",
 "data": {"cpu": 15, "start_date": "...", "uptime": 31022741, "...": "..."}}
{"yuno_id": "2120", "kind": "app", "service": "", "result": 0, "comment": "",
 "data": {"txMsgs": 3966, "rxMsgs": 3966, "rxMsgsec": 500, "...": "..."}}
```

A yuno that does not run gets only its `state`. One watch per requester --
its connection AND the client at the far end of the route, because every web
client of a control center shares that control center's connection: a new
call replaces the previous one. The client is told by the hop the agent can
trust: directly connected, the hop of the agent's own input channel; through
a control center, the hop stamped by the control center -- the channel the
client came in by there (`input_channel`, since 7.25.15: before, two tabs of
one browser were one requester and shared a watch) and its connection
(`cc_connection`). Hops deeper in the stack are written by the client as it
likes and are never read: up to 7.25.20 the agent took the LAST one, so a
client could add a hop naming another user's watch, and stop or replace it. One yuno may be named with several services
(`ids=5120,5120:db_tracks_ce`), and an id that is not a yuno of this agent no
longer refuses the whole watch: it is watched, its `state` says `missing`, and
the answer names it (`data.missing`,
*"watching 3 yunos every 2000 ms, 1 of them not found here"*). A watch lives `watch_ttl` (60000 ms) unless it
is asked again: behind a control center the agent never sees a browser leave,
so the requester renews it (the answer carries the `ttl`; gui_agent renews
every third of it), and one not renewed goes with *"watch-yuno-stats not
renewed, expired"* in the agent's log. The readings are taken by the agent on
loopback (`stats-yuno` to its own yunos), ONCE per yuno and service however
many requesters watch it, at the shortest period asked; `period` is never
under the attribute `watch_min_period` (1000 ms), `max_watches` (30)
bounds the requesters, and `max_watch_ids` (256) the names in one `ids`
(*"too many yuno ids: 300, max_watch_ids is 256"*). A `max_watch_ids` under 1
is a configuration error: every watch is refused (*"max_watch_ids is 0, it
must be 1 or more: no watch taken"*) and the agent logs it. The answer goes first:
the first readings of a new or renewed watch follow it, to that requester
only (up to 7.25.20 they went out before it, and every renewal read every
watch of every requester again). It is not a subscription on purpose: a subscription
travels only from a client to a server, and the agent is the server of its
yunos.

A client that asks for a watch must know `EV_YUNO_STATS`, and SAY it: a C
client that receives an event it does not know drops the connection, or logs
*"Event NOT DEFINED in state"* on every one (`ycommand`, until the watch
expired). So a watch is accepted only if its kw carries `__relays__` with the
event -- written by a control center in between (see
[the control center](controlcenter.md): it removes the web client's own, and
writes it only when the web client's request named the event), or by a client
connected directly:

```json
{"ids": "2120,5120", "period": 2000, "__relays__": ["EV_YUNO_STATS"]}
```

Otherwise it is refused (*"this client does not say it takes EV_YUNO_STATS
(__relays__), ask stats-yuno instead"*, or through a control center *"the
client, or the control center in between, does not say it takes EV_YUNO_STATS
(__relays__), ask stats-yuno instead"*) and the requester polls.

## Redundancy

Every node also runs [`yuneta_agent22`](yuneta_agent22.md), a minimal second
agent kept alive as an escape hatch so a node is never left unmanageable.

## Deep dive

The agent is documented at length under **Operating Yuneta**:

- [Entry point (main + watcher)](../../../yunos/c/yuno_agent/ENTRY_POINT.md)
- [Yuno lifecycle](../../../yunos/c/yuno_agent/YUNO_LIFECYCLE.md)
- [Debugging a yuno](../../../yunos/c/yuno_agent/DEBUGGING.md)
- [Inter-process communication](../../../yunos/c/yuno_agent/IPC.md)
- [Realms (multi-tenancy)](../../../yunos/c/yuno_agent/REALMS.md)
- [Scaffolding new yunos](../../../yunos/c/yuno_agent/SCAFFOLDING.md)
- [Auth, permissions, TLS](../../../yunos/c/yuno_agent/YUNO_AUTH.md)
- [gobj framework crash course](../../../yunos/c/yuno_agent/GOBJ.md)
- [timeranger2 + treedb crash course](../../../yunos/c/yuno_agent/YUNO_TREEDB.md)
