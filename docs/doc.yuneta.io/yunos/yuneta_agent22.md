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
center has `console_pause_tx_queue` messages or more waiting (default `256`;
a message carries up to 1 KB of output, about 2 KB on the wire), or
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
control center) does not come back with the console's events. Before the fix
the whole `__md_iev__` was kept and copied into every chunk of output, and
the bounds counted messages only: a request padded with 1 MB made every
chunk of 1 KB a message of 1 MB, and 256 of them took `agent22` from 11 to
264 MB.

Up to 7.30.26 there was neither: a control center that did not read its link
grew `agent22` with all the output of its consoles (some 50 MB/s with `yes`)
until it aborted. Test: `tests/c/c_agent22_write_tty` (8 s of `yes` with the
link not read: `agent22` stays flat, and the console goes on when the link is
read again; the same with the request padded with 1 MB: under +25 MB, and
frames of output of 2 KB when the padding is not in a hop).

```{danger}
`write-tty` / consoles are a real backdoor (an interactive shell on the node).
Use Yuneta only on private networks, or over encrypted **and** authenticated
public links.
```

## Variant

The same sources also build **`yuneta_agent44`** ([`main44.c`](https://github.com/artgins/yunetas/blob/7.31.1/yunos/c/yuno_agent22/src/main44.c)), a second flavour
of the minimal agent.
