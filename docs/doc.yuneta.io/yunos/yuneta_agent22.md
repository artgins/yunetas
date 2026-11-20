(yuno-yuneta_agent22)=
# `yuneta_agent22`

A minimal **second agent** — a redundant remote entry point for the control
center. Every node runs it next to the primary [`yuneta_agent`](yuneta_agent.md).
The init scripts deliberately leave `agent22` alive when stopping the node, so
a remote operator is never stranded if the primary agent is down or being
upgraded (the two agents upgrade each other, never both at once).

It runs in its own systemd unit, `yuneta_agent22.service`: `sudo systemctl
restart yuneta_agent22` (or `sudo service yuneta_agent22 restart`) restarts it
alone. How the two units are started, stopped and deployed is in
[`yuneta_agent`: starting and stopping it](#yuneta-agent-systemd).

## Control-center link

Like the primary agent, `agent22` dials the control center when the node has an
owner, but on its **own** endpoint — `__output_url__` defaults to
`yunetacontrol.ovh:1989` (port **1989**, vs the primary agent's `…com:1988`):

```
tcps://<machine>.<node_owner>.yunetacontrol.ovh:1989
```

It trusts only the control center's own certificate, as the primary agent does
(see [`controlcenter`](controlcenter.md)). Up to 7.30.3 the port was **1995**.

## Remote consoles (PTY)

`agent22`'s distinctive feature is a remote **console** facility built on a PTY
([`C_PTY`](#gclass-c-pty), `forkpty`). The control center can open an interactive console on the
node and stream keystrokes/output to it:

| Command | Parameters | Description |
|---------|------------|-------------|
| `open-console` | `name`, … | Open a PTY console |
| `write-tty` | `name`, `content64` | Write (base64) data to a console's tty |
| `list-consoles` | — | List open consoles |
| `close-console` | `name` | Close a console |
| `upgrade-agent` | `content64` | Replace the binary of the main agent and restart it ([Upgrading the main agent](#agent22-upgrade-agent)) |
| `help` | — | Command help |

A `write-tty` that cannot be written is refused, and the link to the control
center stays up:

- A `name` that is no open console (a key typed right after the console
  closed), or a service that is not a console (`__yuno__`, `agent22`), logs
  one WARNING (*"console not found"*, *"not a console"*). The requester gets
  `EV_TTY_CLOSE` with that `name`, so its terminal can close.
- A `write-tty` without `content64` logs one WARNING (*"content64 required"*)
  and is dropped.

```text
write-tty name=nosuch content64=YQ==      -> EV_TTY_CLOSE {"name": "nosuch"}
write-tty name=__yuno__ content64=YQ==    -> EV_TTY_CLOSE {"name": "__yuno__"}
```

Up to 7.30.26 a console not found and a missing `content64` were an ERROR and
an `EV_DROP` of the whole link to the control center. A service that is no
console was sent the write: `__yuno__` answered *"Event NOT DEFINED in
state"* with a stack, and `agent22` wrote to itself. The primary agent does
the same since this version (see [`yuneta_agent`](yuneta_agent.md)).

### A control center that does not read

A console has **flow control**, as the primary agent's (see
[`yuneta_agent`](yuneta_agent.md), *Consoles*). When the link to the control
center has `console_pause_tx_queue` messages or more waiting (default `256`,
and a message carries up to 1 KB of output, about 2 KB on the wire), or
`console_pause_tx_bytes` bytes or more (default `1048576`, the link's
`cur_tx_bytes`), the console stops reading its terminal: its process waits on its write, and nothing is lost. It reads
again when the link has sent everything, or when the link drops its
connection (what it had waiting goes with it). The link is the only route of
`agent22`'s consoles, and the one that opened them, so it freezes nobody
else: a console waits for it as long as the control center is away, with no
deadline.

`console_max_tx_queue` (default `8192`) and `console_max_tx_bytes` (default
`33554432`, 32 MB) stay as a hard bound whatever paused the console (they
matter only with flow control off): over either the console is closed, with
ONE WARNING *"console output not taken by its session, console closed for
it"* (with `tx_queue` and `tx_bytes`). `0` turns any of the four attributes
off:

```json
{
    "global": {
        "agent22.console_pause_tx_queue": 256,
        "agent22.console_pause_tx_bytes": 1048576,
        "agent22.console_max_tx_queue": 8192,
        "agent22.console_max_tx_bytes": 33554432
    }
}
```

The route of a console keeps only what routes its events back of the
request that opened it: the stack of its hops (`ievent_gate_stack`) and its
`__msg_type__`. Any other key of the request's `__md_iev__` (a marker of the
control center) does not return with the console's events. Before the fix
the whole `__md_iev__` was kept and copied into every chunk of output, and
the bounds counted messages only: a request padded with 1 MB made every
chunk of 1 KB a message of 1 MB, and 256 of them took `agent22` from 11 to
264 MB.

Up to 7.30.26 there was neither: a control center that did not read its link
grew `agent22` with all the output of its consoles (some 50 MB/s with `yes`)
until it aborted. Test: `tests/c/c_agent22_write_tty` (8 s of `yes` with the
link not read: `agent22` stays flat, and the console goes on when the link is
read again. The same with the request padded with 1 MB: under +25 MB, and
frames of output of 2 KB when the padding is not in a hop).

```{danger}
`write-tty` / consoles are a real backdoor (an interactive shell on the node).
Use Yuneta only on private networks, or over encrypted **and** authenticated
public links.
```

(agent22-upgrade-agent)=
## Upgrading the main agent

`agent22` replaces the binary of the main agent and restarts it (7.31.27+).
Send the command through the control center to the `agent22` of the node:

```bash
ycommand --url=wss://<control center>:1997 <login options> -w 120 \
  -c 'command-agent agent_id=<node> cmd2agent="upgrade-agent content64=$$(/yuneta/agent/yuneta_agent)"'
```

`ycommand` reads the file and sends it in base64. Give `-w` enough seconds
for the binary to arrive, or the answer is lost. A 36 MB binary takes some
20 seconds.

| Parameter | Type | Description |
|-----------|------|-------------|
| `content64` | string | The new `yuneta_agent` binary in base64. |

The command does these steps:

1. It writes the binary as `/yuneta/agent/upgrade/yuneta_agent`.
2. It runs the binary with `--print-role`. If the role is not
   `yuneta_agent`, it deletes the binary and refuses the command.
3. It keeps the binary on disk as `yuneta_agent.bak-pre-<new version>`, a
   hard link. If that backup is already there, it stays as it is.
4. It renames the new binary over `/yuneta/agent/yuneta_agent`.
5. It restarts `yuneta_agent.service` with `sudo -n systemctl restart
   --no-block`.

The answer comes before the main agent is back:

```text
yuneta_agent22^deploycenter: yuneta_agent 7.31.26 -> 7.31.26, the one before
kept as /yuneta/agent/yuneta_agent.bak-pre-7.31.26; restart of
yuneta_agent.service queued
```

The yunos of the node do not restart. The unit stops only the main agent.

Each step has a reason:

- A yuno does not run under a name that is not its role. So the new binary
  keeps the name `yuneta_agent`, in its own directory. That directory is on
  the same file system as `/yuneta/agent`, so the `rename()` is atomic. At no
  time is the node without a binary.
- A binary that runs cannot be written to (`ETXTBSY`), but it can be
  renamed over. The running agent keeps its old binary until the restart.
- The package installs `/etc/sudoers.d/90-yuneta`, so `sudo -n` works for
  the user `yuneta`. The restart stays inside the unit, so systemd and the
  next boot see the agent.
- If the main agent runs outside its unit, `agent22` does not restart it.
  The answer says so, and the binary on disk is the new one.

The command refuses a node that runs from a package. It uses the test of the
preinst of the package: `/yuneta/development/yunetas` has `kernel/` and
`.git`. On a package node, the agent comes in the package. Then the package
database tells the truth about the node.

The permission is `upgrade-agent`. The role `root` of the configuration of
`agent22` has every permission.

Before 7.31.27, the answer to a large `content64` did not arrive. Each
answer carried the whole command line back, and the control center dropped
the client (see [`msg_iev_stack_command_line()`](#msg_iev_stack_command_line)).

## Variant

The same sources also build **`yuneta_agent44`** ([`main44.c`](https://github.com/artgins/yunetas/blob/7.31.26/yunos/c/yuno_agent22/src/main44.c)), a second flavour
of the minimal agent.
