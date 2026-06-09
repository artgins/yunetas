(yuno-yuneta_agent22)=
# `yuneta_agent22`

A minimal **second agent** — a redundant remote entry point for the control
center. Every node runs it alongside the primary [`yuneta_agent`](yuneta_agent.md);
the init scripts deliberately leave `agent22` alive when stopping the node, so
a remote operator is never stranded if the primary agent is down or being
upgraded (the two agents upgrade each other, never both at once).

## Control-center link

Like the primary agent, `agent22` dials the control center when the node has an
owner, but on its **own** endpoint — `__output_url__` defaults to
`yunetacontrol.ovh:1995` (port **1995**, vs the primary agent's `…com:1994`):

```
tcps://<machine>.<node_owner>.yunetacontrol.ovh:1995
```

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

```{danger}
`write-tty` / consoles are a real backdoor (an interactive shell on the node).
Use Yuneta only on private networks, or over encrypted **and** authenticated
public links.
```

## Variant

The same sources also build **`yuneta_agent44`** ([`main44.c`](https://github.com/artgins/yunetas/blob/7.5.11/yunos/c/yuno_agent22/src/main44.c)), a second flavour
of the minimal agent.
