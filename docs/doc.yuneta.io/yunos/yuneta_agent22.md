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

```{danger}
`write-tty` / consoles are a real backdoor (an interactive shell on the node).
Use Yuneta only on private networks, or over encrypted **and** authenticated
public links.
```

## Variant

The same sources also build **`yuneta_agent44`** ([`main44.c`](https://github.com/artgins/yunetas/blob/7.30.26/yunos/c/yuno_agent22/src/main44.c)), a second flavour
of the minimal agent.
