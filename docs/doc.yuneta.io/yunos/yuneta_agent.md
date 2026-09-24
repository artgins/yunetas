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
  with the shape of a JWT.
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
