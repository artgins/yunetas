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
| `install-package` | `url`, `sha256` | Install a `yuneta-agent` package on a package node ([Installing a package](#agent22-install-package)) |
| `install-package-status` | `lines` | How the last `install-package` went |
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
3. It copies the binary that runs from `/proc/<MainPID>/exe` of
   `yuneta_agent.service` to `/yuneta/agent/upgrade/running/yuneta_agent`.
   It reads the old version from that copy (7.31.28+).
4. It keeps that copy as `yuneta_agent.bak-pre-<new version>`. If that
   backup is already there, it stays as it is.
5. It renames the new binary over `/yuneta/agent/yuneta_agent`.
6. It restarts `yuneta_agent.service` with `sudo -n systemctl restart
   --no-block`.

The answer comes before the main agent is back:

```text
yuneta_agent22^deploycenter: yuneta_agent 7.31.26 -> 7.31.27, the one that
runs kept as /yuneta/agent/yuneta_agent.bak-pre-7.31.27; restart of
yuneta_agent.service queued
```

The data of the answer has `old_version`, `old_from` (`running` or `disk`),
`new_version`, `backup`, `backup_was_there` and `restart`.

A build on the node replaces `/yuneta/agent/yuneta_agent` while the agent
runs. So the file on disk can be the new binary already, and only
`/proc/<MainPID>/exe` is the binary that runs. Up to 7.31.27 the command
kept the file on disk and read the old version from it. After a build, the
backup was the new binary, and the answer said `7.31.27 -> 7.31.27`.

If the main agent runs outside its unit, or the copy fails, the command
keeps the file on disk as before (a hard link). It logs a WARNING, and the
answer says `the one on disk (NOT the one that runs: see the log)`.

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

(agent22-install-package)=
## Installing a package

On a package node, the agent comes in the `yuneta-agent` package.
`agent22` downloads the package and installs it (7.31.27+). Send the
commands through the control center to the `agent22` of the node:

```bash
# 1. On your machine, get the sha256 of the package.
curl -fsSL https://github.com/artgins/yunetas/releases/download/7.31.27/yuneta-agent-7.31.27-1-amd64.deb | sha256sum

# 2. Start the install.
ycommand --url=wss://<control center>:1997 <login options> -w 20 \
  -c 'command-agent agent_id=<node> cmd2agent="install-package url=https://github.com/artgins/yunetas/releases/download/7.31.27/yuneta-agent-7.31.27-1-amd64.deb sha256=<sha256>"'

# 3. When agent22 is back in list-agents, read the result.
ycommand --url=wss://<control center>:1997 <login options> -w 20 \
  -c 'command-agent agent_id=<node> cmd2agent="install-package-status"'
```

A node on Rocky or Red Hat takes the `.rpm` asset of the same release.

| Parameter | Type | Description |
|-----------|------|-------------|
| `url` | string | The `https://` URL of the `.deb` or `.rpm` of `yuneta-agent`. |
| `sha256` | string | The sha256 of the package, 64 hex digits. |
| `lines` | integer | `install-package-status` only: the lines of the journal to show (default 40). |

The answer of `install-package` comes at once, with the name of the unit
that installs the package:

```text
yuneta_agent22^<node>: installing yuneta-agent-7.31.27-1-amd64.deb in the unit
yuneta-install-package-20261120211600. Its postinst restarts the main agent
and then this one: ask install-package-status when this agent is back
```

`install-package-status` gives four things:

- `result`: what the script wrote. It is `done <rc>`, `download-failed <rc>`,
  or `sha256-mismatch`. It is empty while the install runs.
- `installed`: the version of `yuneta-agent` that the package database has.
- `units`: the units `yuneta-install-package-*` that systemd knows.
- `journal`: the end of the journal of those units, with the output of
  `apt-get` or `dnf`.

The install has these steps:

1. `agent22` writes a script, the URL, the sha256 and the name of the
   package in `/yuneta/agent/install-package/`.
2. It runs the script as root in a transient unit,
   `yuneta-install-package-<UTC time>`, with `sudo -n systemd-run`.
3. The script downloads the package with `curl`, `https` only (redirects
   too).
4. If the sha256 of the download is not the one given, the script deletes
   the download and stops. Nothing is installed.
5. The script installs the package with `apt-get install -y` (and
   `--force-confold`, so the configurations of the node stay) or
   `dnf install -y`. Then it deletes the download.
6. The postinst of the package restarts the main agent and then `agent22`.

Each step has a reason:

- The install runs in its own unit, out of the cgroup of `agent22`. So the
  postinst can restart `agent22`, and the install goes on.
- systemd expands `%` and `$` in the command line of a unit, and a URL can
  contain both. The option that stops it (`--expand-environment=no`) is in
  systemd 254 and later, and Rocky 9 has 252. So the URL goes in a file, and
  the command line of the unit has only the fixed path of the script.
- `agent22` restarts during the install, and it forgets the install. So
  `install-package-status` reads the result from a file and from the journal.

`install-package` refuses these cases:

- A node that builds from source. The preinst of the package refuses it
  too. Use [`upgrade-agent`](#agent22-upgrade-agent) there.
- A URL that does not start with `https://`, has a blank or a quote, or does
  not end in the name of a `.deb` or a `.rpm`.
- A `.deb` on a node with no `apt-get`, or a `.rpm` on a node with no `dnf`.
- A second install while one runs.

The permission of both commands is `install-package`.

## Variant

The same sources also build **`yuneta_agent44`** ([`main44.c`](https://github.com/artgins/yunetas/blob/7.31.27/yunos/c/yuno_agent22/src/main44.c)), a second flavour
of the minimal agent.
